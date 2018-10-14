#include "nspHelper.h"
#include "virtualFile.h"
#include "filehelper.h"
#include <Tinfoil/include/install/simple_filesystem.hpp>
#include <Tinfoil/include/nx/content_meta.hpp>
#include <Tinfoil/include/nx/ncm.hpp>
#include <Tinfoil/include/util/title_util.hpp>
#include <iostream>
#include <memory>
#include <sys/stat.h>

using namespace tin::install::nsp;

extern float progress;
extern float progressSpeed;

namespace NXFramework
{

std::tuple<std::string, nx::ncm::ContentRecord> GetCNMTNCAInfo(std::string nspPath)
{
    // Open filesystem
    nx::fs::IFileSystem fileSystem;
    std::string nspExt          = ".nsp";
    std::string rootPath        = "/";
    std::string absolutePath    = nspPath + "/";

    // Check if this is an nsp file
    if (nspPath.compare(nspPath.size() - nspExt.size(), nspExt.size(), nspExt) == 0)
    {
        fileSystem.OpenFileSystemWithId(nspPath, FsFileSystemType_ApplicationPackage, 0);
    }
    else // Otherwise assume this is an extracted NSP directory
    {
        fileSystem.OpenSdFileSystem();
        rootPath        = nspPath.substr(9) + "/";
        absolutePath    = nspPath + "/";
    }
    tin::install::nsp::SimpleFileSystem simpleFS(fileSystem, rootPath, absolutePath);

    // Create the path of the cnmt NCA
    auto cnmtNCAName        = simpleFS.GetFileNameFromExtension("", "cnmt.nca");
    auto cnmtNCAFile        = simpleFS.OpenFile(cnmtNCAName);
    auto cnmtNCAFullPath    = simpleFS.m_absoluteRootPath + cnmtNCAName;
    u64 cnmtNCASize         = cnmtNCAFile.GetSize();

    // Prepare cnmt content record
    nx::ncm::ContentRecord contentRecord;
    contentRecord.ncaId         = tin::util::GetNcaIdFromString(cnmtNCAName);
    *(u64*)contentRecord.size   = cnmtNCASize & 0xFFFFFFFFFFFF;
    contentRecord.contentType   = nx::ncm::ContentType::META;
    return { cnmtNCAFullPath, contentRecord };
}

nx::ncm::ContentMeta GetContentMetaFromNCA(std::string& ncaPath)
{
    // Create the cnmt filesystem
    nx::fs::IFileSystem cnmtNCAFileSystem;
    cnmtNCAFileSystem.OpenFileSystemWithId(ncaPath, FsFileSystemType_ContentMeta, 0);
    tin::install::nsp::SimpleFileSystem cnmtNCASimpleFileSystem(cnmtNCAFileSystem, "/", ncaPath + "/");

    // Find and read the cnmt file
    auto cnmtName = cnmtNCASimpleFileSystem.GetFileNameFromExtension("", "cnmt");
    auto cnmtFile = cnmtNCASimpleFileSystem.OpenFile(cnmtName);
    u64 cnmtSize  = cnmtFile.GetSize();

    tin::data::ByteBuffer cnmtBuf;
    cnmtBuf.Resize(cnmtSize);
    cnmtFile.Read(0x0, cnmtBuf.GetData(), cnmtSize);

    return nx::ncm::ContentMeta(cnmtBuf.GetData(), cnmtBuf.GetSize());
}

std::tuple<nx::ncm::ContentMeta, nx::ncm::ContentRecord> ReadCNMT(SimpleFileSystem& simpleFS)
{
    std::tuple<std::string, nx::ncm::ContentRecord> cnmtNCAInfo = GetCNMTNCAInfo(simpleFS.m_absoluteRootPath.substr(0, simpleFS.m_absoluteRootPath.size() - 1));
    return { GetContentMetaFromNCA(std::get<0>(cnmtNCAInfo)), std::get<1>(cnmtNCAInfo) };
}

void InstallTicketCert(SimpleFileSystem& simpleFS)
{
    // Read the tik file and put it into a buffer
    auto tikName    = simpleFS.GetFileNameFromExtension("", "tik");     LOG("> Getting tik size\n");
    auto tikFile    = simpleFS.OpenFile(tikName);
    u64 tikSize     = tikFile.GetSize();
    auto tikBuf     = std::make_unique<u8[]>(tikSize);                  LOG("> Reading tik\n");
    tikFile.Read(0x0, tikBuf.get(), tikSize);

    // Read the cert file and put it into a buffer
    auto certName   = simpleFS.GetFileNameFromExtension("", "cert");    LOG("> Getting cert size\n");
    auto certFile   = simpleFS.OpenFile(certName);
    u64 certSize    = certFile.GetSize();
    auto certBuf    = std::make_unique<u8[]>(certSize);                 LOG("> Reading cert\n");
    certFile.Read(0x0, certBuf.get(), certSize);

    // Finally, let's actually import the ticket
    ASSERT_OK(esImportTicket(tikBuf.get(), tikSize, certBuf.get(), certSize), "Failed to import ticket\n");
}

void InstallContentMetaRecords(tin::data::ByteBuffer&     installContentMetaBuf,
                               nx::ncm::ContentMeta&      contentMeta,
                               const FsStorageId          destStorageId)
{
    NcmContentMetaDatabase contentMetaDatabase;
    NcmMetaRecord contentMetaKey = contentMeta.GetContentMetaKey();
    try
    {
        ASSERT_OK(ncmOpenContentMetaDatabase(destStorageId, &contentMetaDatabase), "Failed to open content meta database\n");
        ASSERT_OK(ncmContentMetaDatabaseSet(&contentMetaDatabase, &contentMetaKey, installContentMetaBuf.GetSize(), (NcmContentMetaRecordsHeader*)installContentMetaBuf.GetData()), "Failed to set content records\n");
        ASSERT_OK(ncmContentMetaDatabaseCommit(&contentMetaDatabase), "Failed to commit content records\n");
    }
    catch (std::runtime_error& e)
    {
        serviceClose(&contentMetaDatabase.s);
        throw e;
    }
}

void InstallApplicationRecord(nx::ncm::ContentMeta& contentMeta, const FsStorageId destStorageId)
{
    Result rc = 0;
    std::vector<ContentStorageRecord> storageRecords;
    u64 baseTitleId = tin::util::GetBaseTitleId(
            contentMeta.GetContentMetaKey().titleId,
            static_cast<nx::ncm::ContentMetaType>(contentMeta.GetContentMetaKey().type));

    LOG("Base title Id: 0x%lx\n", baseTitleId);

    // TODO: Make custom error with result code field
    // 0x410: The record doesn't already exist
    u32 contentMetaCount = 0;
    if (R_FAILED(rc = nsCountApplicationContentMeta(baseTitleId, &contentMetaCount)) && rc != 0x410)
    {
        throw std::runtime_error("Failed to count application content meta\n");
    }
    LOG("Content meta count: %u\n", contentMetaCount);

    // Obtain any existing app record content meta and append it to our vector
    if (contentMetaCount > 0)
    {
        storageRecords.resize(contentMetaCount);
        size_t contentStorageBufSize    = contentMetaCount * sizeof(ContentStorageRecord);
        auto contentStorageBuf          = std::make_unique<ContentStorageRecord[]>(contentMetaCount);
        u32 entriesRead;

        ASSERT_OK(nsListApplicationRecordContentMeta(0, baseTitleId, contentStorageBuf.get(), contentStorageBufSize, &entriesRead),"Failed to list application record content meta\n");
        if (entriesRead != contentMetaCount)
        {
            throw std::runtime_error("Mismatch between entries read and content meta count\n");
        }
        memcpy(storageRecords.data(), contentStorageBuf.get(), contentStorageBufSize);
    }

    // Add our new content meta
    ContentStorageRecord storageRecord;
    storageRecord.metaRecord = contentMeta.GetContentMetaKey();
    storageRecord.storageId  = destStorageId;
    storageRecords.push_back(storageRecord);

    // Replace the existing application records with our own
    try
    {
        nsDeleteApplicationRecord(baseTitleId);
    }
    catch (...) {}
    LOG("Pushing application record...\n");
    ASSERT_OK(nsPushApplicationRecord(baseTitleId, 0x3, storageRecords.data(), storageRecords.size() * sizeof(ContentStorageRecord)), "Failed to push application record\n");
}

void InstallNCA(SimpleFileSystem& simpleFS, const NcmNcaId& ncaId, const FsStorageId destStorageId)
{
    std::string ncaName = tin::util::GetNcaIdString(ncaId);

    if (simpleFS.HasFile(ncaName + ".nca"))
        ncaName += ".nca";
    else
    if (simpleFS.HasFile(ncaName + ".cnmt.nca"))
        ncaName += ".cnmt.nca";
    else
    {
        throw std::runtime_error(("Failed to find NCA file " + ncaName + ".nca/.cnmt.nca").c_str());
    }

    LOG("NcaId: %s\n", ncaName.c_str());
    LOG("Dest storage Id: %u\n", destStorageId);

    nx::ncm::ContentStorage contentStorage(destStorageId);

    // Attempt to delete any leftover placeholders
    try
    {
        contentStorage.DeletePlaceholder(ncaId);
    }
    catch (...) {}

    auto ncaFile    = simpleFS.OpenFile(ncaName);
    size_t ncaSize  = ncaFile.GetSize();
    u64 fileOff     = 0;
    size_t readSize = 0x400000; // 4 MB buffer.
    auto readBuffer = std::make_unique<u8[]>(readSize);

    if (readBuffer == NULL)
        throw std::runtime_error(("Failed to allocate read buffer for " + ncaName).c_str());

    LOG("Size: 0x%lx\n", ncaSize);
    contentStorage.CreatePlaceholder(ncaId, ncaId, ncaSize);

    u64 freq            = armGetSystemTickFreq();
    u64 startTime       = armGetSystemTick();
    progress            = 0.f;
    progressSpeed       = 0.f;
    while (fileOff < ncaSize)
    {
        // Progress in %
        progress = (float)fileOff / (float)ncaSize;

        // Progress speed in MB/s
        u64 newTime = armGetSystemTick();
        if (newTime - startTime >= freq)
        {
            double mbRead       = fileOff / (1024.0 * 1024.0);
            double duration     = (double)(newTime - startTime) / (double)freq;
            progressSpeed       = (float)(mbRead / duration);
        }

        if (fileOff % (0x400000 * 3) == 0)
            LOG("> Progress: %lu/%lu MB (%d%s)\n", (fileOff / 1000000), (ncaSize / 1000000), (int)(progress * 100.0), "%");

        if (fileOff + readSize >= ncaSize)
            readSize = ncaSize - fileOff;

        ncaFile.Read(fileOff, readBuffer.get(), readSize);
        contentStorage.WritePlaceholder(ncaId, fileOff, readBuffer.get(), readSize);
        fileOff += readSize;
    }
    progress = 1.f;

    // Clean up the line for whatever comes next
    LOG("                                                           \n");
    LOG("Registering placeholder...\n");

    try
    {
        contentStorage.Register(ncaId, ncaId);
    }
    catch (...)
    {
        LOG(("Failed to register " + ncaName + ". It may already exist.\n").c_str());
    }
    try
    {
        contentStorage.DeletePlaceholder(ncaId);
    }
    catch (...) {}
}

bool virtualNCAFileExist(const std::string rootPath, const NcmNcaId& ncaId)
{
    std::string ncaName = tin::util::GetNcaIdString(ncaId);
    virtualFile virtualNCAFile;
    if(virtualNCAFile.open(std::string(rootPath + ncaName + ".nca").c_str(), "rb"))
    {
        ncaName += ".nca";
        virtualNCAFile.close();
        LOG("Virtual file found: %s\n", std::string(rootPath + ncaName).c_str());
        return true;
    }
    else
    if(virtualNCAFile.open(std::string(rootPath + ncaName + ".cnmt.nca").c_str(), "rb"))
    {
        ncaName += ".cnmt.nca";
        virtualNCAFile.close();
        LOG("Virtual file found: %s\n", std::string(rootPath + ncaName).c_str());
        return true;
    }
    LOG("No virtual file found: %s\n", ncaName.c_str());
    return false;
}

void InstallVirtualNCA(const std::string rootPath, const NcmNcaId& ncaId, const FsStorageId destStorageId)
{
    virtualFile virtualNCAFile;
    std::string ncaName = tin::util::GetNcaIdString(ncaId);

    if(virtualNCAFile.open(std::string(rootPath + ncaName + ".nca").c_str(), "rb"))
        ncaName += ".nca";
    else
    if(virtualNCAFile.open(std::string(rootPath + ncaName + ".cnmt.nca").c_str(), "rb"))
        ncaName += ".cnmt.nca";
    else
    {
        throw std::runtime_error(("Failed to find virtual NCA file " + ncaName + ".nca/.cnmt.nca").c_str());
    }

    LOG("NcaId: %s\n", ncaName.c_str());
    LOG("Dest storage Id: %u\n", destStorageId);

    nx::ncm::ContentStorage contentStorage(destStorageId);

    // Attempt to delete any leftover placeholders
    try
    {
        contentStorage.DeletePlaceholder(ncaId);
    }
    catch (...) {}

    virtualNCAFile.seek(0, SEEK_END);
    size_t ncaSize = virtualNCAFile.tell();
    virtualNCAFile.seek(0, SEEK_SET);

    u64 fileOff     = 0;
    size_t readSize = 0x400000; // 4 MB buffer.
    auto readBuffer = std::make_unique<u8[]>(readSize);

    if (readBuffer == NULL)
        throw std::runtime_error(("Failed to allocate read buffer for " + ncaName).c_str());

    LOG("Size: 0x%lx\n", ncaSize);
    contentStorage.CreatePlaceholder(ncaId, ncaId, ncaSize);

    u64 freq            = armGetSystemTickFreq();
    u64 startTime       = armGetSystemTick();
    progress            = 0.f;
    progressSpeed       = 0.f;
    while (fileOff < ncaSize)
    {
        // Progress in %
        progress = (float)fileOff / (float)ncaSize;

        // Progress speed in MB/s
        u64 newTime = armGetSystemTick();
        if (newTime - startTime >= freq)
        {
            double mbRead       = fileOff / (1024.0 * 1024.0);
            double duration     = (double)(newTime - startTime) / (double)freq;
            progressSpeed       = (float)(mbRead / duration);
        }

        if (fileOff % (0x400000 * 3) == 0)
            LOG("> Progress: %lu/%lu MB (%d%s)\n", (fileOff / 1000000), (ncaSize / 1000000), (int)(progress * 100.0), "%");

        if (fileOff + readSize >= ncaSize)
            readSize = ncaSize - fileOff;

        virtualNCAFile.read(readBuffer.get(), 1, readSize);
        contentStorage.WritePlaceholder(ncaId, fileOff, readBuffer.get(), readSize);
        fileOff += readSize;
    }
    progress = 1.f;
    virtualNCAFile.close();

    // Clean up the line for whatever comes next
    LOG("                                                           \n");
    LOG("Registering placeholder...\n");

    try
    {
        contentStorage.Register(ncaId, ncaId);
    }
    catch (...)
    {
        LOG(("Failed to register " + ncaName + ". It may already exist.\n").c_str());
    }
    try
    {
        contentStorage.DeletePlaceholder(ncaId);
    }
    catch (...) {}
}

bool InstallNSP(const std::string& filename, const FsStorageId destStorageId, const bool ignoreReqFirmVersion, const bool isFolder)
{
    try
    {
        std::string fullPath = "@Sdcard:/" + filename;
        nx::fs::IFileSystem fileSystem;
        if(isFolder)
        {
            fileSystem.OpenSdFileSystem();
        }
        else
        {
            fileSystem.OpenFileSystemWithId(fullPath, FsFileSystemType_ApplicationPackage, 0);
        }
        SimpleFileSystem simpleFS(fileSystem, isFolder?filename+"/":"/", fullPath + "/");

        ///////////////////////////////////////////////////////
        // Prepare
        ///////////////////////////////////////////////////////

        LOG("                    \n");
        LOG("Preparing install...\n");

        tin::data::ByteBuffer cnmtBuf;
        auto cnmtTuple = ReadCNMT(simpleFS);
        nx::ncm::ContentMeta    contentMeta       = std::get<0>(cnmtTuple);
        nx::ncm::ContentRecord  cnmtContentRecord = std::get<1>(cnmtTuple);
        nx::ncm::ContentStorage contentStorage(destStorageId);
        if (!contentStorage.Has(cnmtContentRecord.ncaId))
        {
            LOG("Installing CNMT NCA...\n");
            if(virtualNCAFileExist(simpleFS.m_rootPath.c_str(), cnmtContentRecord.ncaId))
                InstallVirtualNCA(simpleFS.m_rootPath.c_str(), cnmtContentRecord.ncaId, destStorageId);
            else
                InstallNCA(simpleFS, cnmtContentRecord.ncaId, destStorageId);
        }
        else
        {
            LOG("CNMT NCA already installed. Proceeding...\n");
        }

        // Parse data and create install content meta
        if (ignoreReqFirmVersion)
            LOG("WARNING: Required system firmware version is being IGNORED!\n");

        tin::data::ByteBuffer installContentMetaBuf;
        contentMeta.GetInstallContentMeta(installContentMetaBuf, cnmtContentRecord, ignoreReqFirmVersion);

        InstallContentMetaRecords(installContentMetaBuf, contentMeta, destStorageId);
        InstallApplicationRecord(contentMeta, destStorageId);

        LOG("                    \n");
        LOG("Installing ticket and cert...\n");
        try
        {
            InstallTicketCert(simpleFS);
        }
        catch (std::runtime_error& e)
        {
            LOG("WARNING: Ticket installation failed! This may not be an issue, depending on your usecase.\nProceed with caution!\n");
        }

        ///////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////

        LOG("                    \n");
        LOG("Installing NCAs...\n");
        for (auto& record : contentMeta.GetContentRecords())
        {
            LOG("Installing from %s\n", tin::util::GetNcaIdString(record.ncaId).c_str());
            if(virtualNCAFileExist(simpleFS.m_rootPath.c_str(), record.ncaId))
                InstallVirtualNCA(simpleFS.m_rootPath.c_str(), record.ncaId, destStorageId);
            else
                InstallNCA(simpleFS, record.ncaId, destStorageId);
        }
    }
    catch (std::exception& e)
    {
        LOG("Install failed!\n");
        LOG("%s", e.what());
        return false;
    }
    return true;
}

bool InstallExtracted(const std::string& filename, const FsStorageId destStorageId, const bool ignoreReqFirmVersion)
{
    return InstallNSP(filename, destStorageId, ignoreReqFirmVersion, true);
}

//            0x10 = 16            = PFS0 (4bytes) + numFiles (4bytes) + str_table_len (4bytes) + unused (4bytes)
// numFiles * 0x18 = numFiles * 24 = data_offset (8bytes) + data_size (8bytes) + str_offset (4bytes) + unused (4bytes)
bool ExtractNSP(const std::string& filename)
{
    // Output Path
    char outputDir[1024];
    GetFileBasename(outputDir, filename.c_str());

    FILE* NSPfile = nullptr;

    try
    {
        // Clean-up dir if it exists
        RmDirRecursive(outputDir);
        mkdir(outputDir, 0777);

        if (!(NSPfile = fopen(filename.c_str(), "rb")))
        {
            fprintf(stderr, "unable to open %s: %s\n", filename.c_str(), strerror(errno));
            throw std::runtime_error(std::string("unable to open ") + filename.c_str());
        }

        // Read NSP header
        char FOURCC[5];
        fread(FOURCC, 4, 1, NSPfile);
        FOURCC[4] = '\0';
        LOG("Magic = %s\n", FOURCC);
        if(strcmp(FOURCC, "PFS0") != 0)
        {
            fprintf(stderr, "Error: File FOURCC is invalid (expected: PFS0, got: %s)", FOURCC);
            throw std::runtime_error("Error: File FOURCC is invalid");
        }
        uint fileCount;
        uint strTableLen;
        uint padding;
        fread(&fileCount    , 4, 1, NSPfile);
        fread(&strTableLen  , 4, 1, NSPfile);
        fread(&padding      , 4, 1, NSPfile);
        LOG("Discovered file count: %d\n", fileCount);

        uint filesMetaBase   = 0x10;
        uint stringTableBase = filesMetaBase + 0x18 * fileCount;

        // Read file entry table
        struct FileEntry
        {
            uint64_t dataOffset;
            uint64_t dataSize;
            uint64_t stringOffset;
        };
        std::vector<FileEntry> fileEntries;

        for(uint i = 0 ; i < fileCount ; ++i)
        {
            FileEntry entry;
            fread(&entry.dataOffset     , 8, 1, NSPfile);
            fread(&entry.dataSize       , 8, 1, NSPfile);
            fread(&entry.stringOffset   , 8, 1, NSPfile);
            fileEntries.push_back(entry);
            LOG("Discovered file entry: dataOffset %" PRIu64 " dataSize %" PRIu64 " stringOffset %" PRIu64 "\n", entry.dataOffset, entry.dataSize, entry.stringOffset);
        }

        uint64_t bodyBase = ftell(NSPfile) + strTableLen;

        // Read filenames
        struct FileDesc
        {
            uint64_t    absDataOffset;
            uint64_t    dataSize;
            std::string fileName;
        };
        std::vector<FileDesc> fileDescs;
        for (struct FileEntry& entry : fileEntries)
        {
            fseek(NSPfile, stringTableBase + entry.stringOffset, SEEK_SET);

            char fileEntryName[1024];
            int i = 0;
            while(1)
            {
                fread(fileEntryName + i, 1, 1, NSPfile);
                if(fileEntryName[i] == 0)
                    break;

                i++;
            }
            fileDescs.push_back({bodyBase + entry.dataOffset, entry.dataSize, std::string(fileEntryName)});
            LOG("Discovered file name: %s\n", fileEntryName);
        }

        // Extract files
        for (struct FileDesc& fileDesc : fileDescs)
        {
            // Create output file
            std::string outputFilename = std::string(outputDir) + "/" + fileDesc.fileName;
            LOG("Creating file: %s\n", outputFilename.c_str());

            void* vf_out = vfcreate(fileDesc.dataSize > virtualFile::chunkSize);
            if (vfopen(vf_out, outputFilename.c_str(), "wb"))
            {
                fprintf(stderr, "unable to open %s: %s\n", outputFilename.c_str(), strerror(errno));
                throw std::runtime_error(std::string("unable to open ") + outputFilename.c_str());
            }

            // Seek
            fseek(NSPfile, fileDesc.absDataOffset, SEEK_SET);

            // Read buffer
            size_t readSize = 0x400000; // 4 MB buffer.
            auto readBuffer = std::make_unique<u8[]>(readSize);
            if (readBuffer == NULL)
                fprintf(stderr, "Failed to allocate read buffer for %s", outputFilename.c_str());

            // Write file
            u64 fileOff         = 0;
            u64 freq            = armGetSystemTickFreq();
            u64 startTime       = armGetSystemTick();
            progress            = 0.f;
            progressSpeed       = 0.f;

            while (fileOff < fileDesc.dataSize)
            {
                // Progress in %
                progress = (float)fileOff / (float)fileDesc.dataSize;

                // Progress speed in MB/s
                u64 newTime = armGetSystemTick();
                if (newTime - startTime >= freq)
                {
                    double mbRead       = fileOff / (1024.0 * 1024.0);
                    double duration     = (double)(newTime - startTime) / (double)freq;
                    progressSpeed       = (float)(mbRead / duration);
                }

                if (fileOff % (0x400000 * 3) == 0)
                    LOG("> Progress: %lu/%lu MB (%d%s)\n", (fileOff / 1000000), (fileDesc.dataSize / 1000000), (int)(progress * 100.0), "%");

                if (fileOff + readSize >= fileDesc.dataSize)
                    readSize = fileDesc.dataSize - fileOff;

                fread (readBuffer.get(), readSize, 1, NSPfile);
                vfwrite(readBuffer.get(), readSize, 1, vf_out);
                fileOff += readSize;
            }
            vfclose(vf_out);
            vfdestroy(vf_out);
            progress = 1.f;
        }
        fclose(NSPfile);
    }
    catch (std::exception& e)
    {
        LOG("Failed to extract NSP!\n");
        LOG("%s", e.what());

        // Clean-up
        RmDirRecursive(outputDir);
        if(NSPfile != nullptr)
            fclose(NSPfile);

        return false;
    }
    return true;
}


}
