#pragma once
#include <vector>
#include <stdint.h>
#include <string>

namespace NXFramework
{

// Class to handle virtual files split in 4GB parts
struct virtualFile
{
    static const uint64_t chunkSize = 0xFFFF0000;// # 4,294,901,760 bytes

    bool        open(const char *fn, const char *mode);
    int         close();
    uint64_t    tell();
    int         seek(uint64_t offset, int origin);
    size_t      write(const void * ptr, size_t size, size_t count);
    size_t      read( void * ptr, size_t size, size_t count);

    struct FileEntry
    {
        std::string filename;
        FILE*       file = nullptr;
    };

    std::vector<FileEntry>  physicalFiles;
    FileEntry               currentChunk;
	uint64_t                currentChunkOffset;
	uint64_t                absoluteOffset;
    std::string             logicalFilename;
    std::string             mode;
    bool                    split = true;
};

}

extern "C"
{
    void*       vfcreate(bool split);
    void        vfdestroy(void* vFile);
    int         vfopen(void* vFile, const char *fn, const char *mode);
    int         vfclose(void* vFile);
    size_t      vfwrite(const void * ptr, size_t size, size_t count, void* vFile);
    size_t      vfread(void * ptr, size_t size, size_t count, void* vFile);
    int         vfseek(void* vFile, uint64_t offset, int origin);
    uint64_t    vftell(void* vFile);
    uint64_t    vfchunkSize();
}

