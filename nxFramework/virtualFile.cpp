#include "virtualFile.h"
#include "nxUtils.h"
#include <sys/stat.h>

namespace NXFramework
{

bool fileExist(const char* filename)
{
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

bool virtualFile::open(const char *fn, const char *mode)
{
    // Create or Open first chunk
    this->mode = mode;
    logicalFilename = fn;
    FileEntry fileEntry;
    fileEntry.filename      = std::string(logicalFilename) + "." + FixedWidth((int)physicalFiles.size()+1,3);
    fileEntry.file          = fopen(fileEntry.filename.c_str(), mode);
    absoluteOffset          = 0;
    currentChunkOffset      = 0;
    physicalFiles.clear();
    physicalFiles.push_back(fileEntry);
    currentChunk = physicalFiles.back();

    // Look for additional chunks
    while (1)
    {
        std::string filename = std::string(logicalFilename) + "." + FixedWidth((int)physicalFiles.size() + 1, 3);
        if (fileExist(filename.c_str()))
        {
            FileEntry fileEntry;
            fileEntry.filename	= filename;
            fileEntry.file		= nullptr;
            physicalFiles.push_back(fileEntry);
        }
        else
        {
            break;
        }
    }
    return currentChunk.file != nullptr;
}

int virtualFile::close()
{
    for (auto fileEntry : physicalFiles)
    {
        if(fileEntry.file != nullptr)
        {
            fclose(fileEntry.file);
            fileEntry.file = nullptr;
        }
    }
    physicalFiles.clear();
    currentChunk.filename	= std::string();
    currentChunk.file		= nullptr;
    currentChunkOffset		= 0;
    absoluteOffset			= 0;
    return 0;
}

uint64_t virtualFile::tell()
{
    return absoluteOffset;
}

int virtualFile::seek(uint64_t offset, int origin)
{
    if(physicalFiles.size() == 0) return 1;
    if(origin == SEEK_SET)
    {
        absoluteOffset = offset;
    }
    else
    if (origin == SEEK_CUR)
    {
        absoluteOffset += offset;
    }
    else
    if(origin == SEEK_END)
    {
        absoluteOffset = 0;
        for (uint32_t i = 0 ; i < physicalFiles.size() - 1 ; ++i)
            absoluteOffset += chunkSize;

        // GetSize of the last chunk
        FILE* lastChunk;
        if (!(lastChunk = fopen(physicalFiles.back().filename.c_str(), "rb")))
        {
            while (0); // error
        }
        fseek(lastChunk, 0, SEEK_END);
        uint64_t lastChunkSize = ftell(lastChunk);
        fclose(lastChunk);
        absoluteOffset += lastChunkSize;
    }
    currentChunkOffset  =			 absoluteOffset % chunkSize;
    uint32_t fileIdx    = (uint32_t)(absoluteOffset / chunkSize);
    fclose(currentChunk.file);
    currentChunk.file = nullptr;

    if (fileIdx > physicalFiles.size())
    {
        currentChunk.filename   = std::string();
        currentChunk.file		= nullptr;
    }
    else
    {
        physicalFiles[fileIdx].file = fopen(physicalFiles[fileIdx].filename.c_str(), mode.c_str());
        currentChunk = physicalFiles[fileIdx];
        fseek(currentChunk.file, currentChunkOffset, SEEK_SET);
    }
    return 0;
}

size_t virtualFile::write(const void * ptr, size_t size, size_t count)
{
    if(currentChunk.file == nullptr) return 0;
    uint64_t writeSize = size * count;

    uint64_t canWrite = chunkSize - currentChunkOffset;
    if(writeSize < canWrite)
    {
        currentChunkOffset  += writeSize;
        absoluteOffset      += writeSize;
        return fwrite(ptr, size, count, currentChunk.file);
    }
    else
    {
        // Write what we can write in the current chunk
        uint64_t writtenA = fwrite(ptr, 1, (size_t)canWrite, currentChunk.file);

        uint32_t nextFileIdx = (uint32_t)(absoluteOffset / chunkSize) + 1;
        if(nextFileIdx < physicalFiles.size())
        {
            fclose(currentChunk.file);
            currentChunk.file = nullptr;
            physicalFiles[nextFileIdx].file = fopen(physicalFiles[nextFileIdx].filename.c_str(), mode.c_str());
            currentChunk = physicalFiles[nextFileIdx];
        }
        else
        {
            fclose(currentChunk.file);
            currentChunk.file = nullptr;

            // create new chunk
            FileEntry fileEntry;
            fileEntry.filename      = logicalFilename + "." + FixedWidth((int)physicalFiles.size()+1,3);
            fileEntry.file          = fopen(fileEntry.filename.c_str(), mode.c_str());
            currentChunk			= physicalFiles.back();
            currentChunkOffset		= 0;
            physicalFiles.push_back(fileEntry);
        }
        uint64_t writtenB = fwrite((char*)ptr+canWrite, 1, (size_t)(writeSize-canWrite), currentChunk.file);
        currentChunkOffset  += writtenB;
        absoluteOffset		+= writeSize;
        return size_t(writtenA + writtenB);
    }
    return 0;
}

size_t virtualFile::read( void * ptr, size_t size, size_t count)
{
    if(currentChunk.file == nullptr) return 0;
    uint64_t readSize = size * count;

    uint64_t canRead = chunkSize - currentChunkOffset;
    if(readSize < canRead)
    {
        currentChunkOffset  += readSize;
        absoluteOffset      += readSize;
        return fread(ptr, size, count, currentChunk.file);
    }
    else
    {
        // Read what we can write in the current chunk
        uint64_t readA = fread(ptr, 1, (size_t)canRead, currentChunk.file);

        uint32_t nextFileIdx = (uint32_t)((absoluteOffset+readA) / chunkSize);
        if(nextFileIdx < physicalFiles.size())
        {
            fclose(currentChunk.file);
            currentChunk.file = nullptr;

            physicalFiles[nextFileIdx].file = fopen(physicalFiles[nextFileIdx].filename.c_str(), mode.c_str());
            currentChunk = physicalFiles[nextFileIdx];

            uint64_t readB = fread((char*)ptr+canRead, 1, (size_t)(readSize-canRead), currentChunk.file);

            currentChunkOffset	 = readB;
            absoluteOffset		+= readSize;
            return size_t(readA + readB);
        }
        // end of file
        currentChunkOffset	+= readA;
        absoluteOffset		+= readSize;
        return (size_t)readA;
    }
    return 0;
}

}

// Wrappers for C code
extern "C" void* vfcreate(bool split)
{
    NXFramework::virtualFile* vf = new NXFramework::virtualFile();
    vf->split = split;
    return vf;
}
extern "C" void vfdestroy(void* vFile)
{
    if(vFile == nullptr) return;
    NXFramework::virtualFile* vf = new NXFramework::virtualFile();
    delete vf;
}
extern "C" int vfopen(void* vFile, const char *fn, const char *mode)
{
    if(vFile == nullptr) return 1;
    NXFramework::virtualFile* vf = (NXFramework::virtualFile*)vFile;
    if(vf->split)
    {
        return (vf->open(fn, mode))?0:1;
    }
    else
    {
       vf->currentChunk.file = fopen(fn, mode);
       return (vf->currentChunk.file != NULL)?0:1;
    }
    return 1;
}
extern "C" int vfclose(void* vFile)
{
    if(vFile == nullptr) return 1;
    NXFramework::virtualFile* vf = (NXFramework::virtualFile*)vFile;
    if(vf->split)        return vf->close();
    else                 return fclose(vf->currentChunk.file);
    return 1;
}
extern "C" uint64_t vftell(void* vFile)
{
    if(vFile == nullptr) return 0;
    NXFramework::virtualFile* vf = (NXFramework::virtualFile*)vFile;
    if(vf->split)        return vf->tell();
    else                 return ftell(vf->currentChunk.file);
    return 0;
}
extern "C" int vfseek(void* vFile, uint64_t offset, int origin)
{
    if(vFile == nullptr) return 1;
    NXFramework::virtualFile* vf = (NXFramework::virtualFile*)vFile;
    if(vf->split)        return vf->seek(offset, origin);
    else                 return fseek(vf->currentChunk.file, offset, origin);
    return 1;
}
extern "C" size_t vfwrite(const void * ptr, size_t size, size_t count, void* vFile)
{
    if(vFile == nullptr) return 0;
    NXFramework::virtualFile* vf = (NXFramework::virtualFile*)vFile;
    if(vf->split)        return vf->write(ptr, size, count);
    else                 return fwrite(ptr, size, count, vf->currentChunk.file);
    return 0;
}
extern "C" size_t vfread(void * ptr, size_t size, size_t count, void* vFile)
{
    if(vFile == nullptr) return 0;
    NXFramework::virtualFile* vf = (NXFramework::virtualFile*)vFile;
    if(vf->split)        return vf->read(ptr, size, count);
    else                 return fread(ptr, size, count, vf->currentChunk.file);
    return 0;
}
extern "C" uint64_t vfchunkSize()
{
    return NXFramework::virtualFile::chunkSize;
}
