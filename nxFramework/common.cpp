#include "common.h"

// TODO: might want to do a similar thing to have a cheap memtracker
#ifdef DEBUG
#include <map>
std::map<FILE*, std::string> openFiles;

extern "C" FILE* __real_fopen  (const char *fn, const char *mode);
extern "C" int   __real_fclose (FILE* f);

extern "C" FILE* __wrap_fopen (const char *fn, const char *mode)
{
    FILE* f = __real_fopen(fn, mode);
    LOG("HOOK: fopen(\"%s\", \"%s\") == %p\n", fn, mode, f);

    // Add the entry to the open file list
    openFiles[f] = std::string(fn);

    return f;
}

extern "C" int __wrap_fclose (FILE* f)
{
    // Remove the entry from the open file list
    auto elt = openFiles.find(f);
    if(elt != openFiles.end())
    {
        LOG("HOOK: fclose(%p) %s\n", f, elt->second.c_str());
        openFiles.erase(elt);
    }
    LOG("HOOK: open files: %d\n", (int)openFiles.size());
    return __real_fclose(f);
}
#endif

int openFileCount()
{
#ifdef DEBUG
    return (int)openFiles.size();
#endif
    return 0;
}

void displayOpenFiles()
{
#ifdef DEBUG
    LOG("\n%d Open files:\n", (int)openFiles.size());
    LOG("------------------\n");
    for (auto it=openFiles.begin(); it!=openFiles.end(); ++it)
        LOG("%p %s\n", it->first, it->second.c_str());
#endif
}

extern "C" void throw_runtime_error(int status)
{
    printf("\nWRAPPER throw_runtime_error: %d\n", status);
    throw std::runtime_error(std::string("Exception on Exit: ") + char('0' + status));
}

void printBytes(u8 *bytes, size_t size, bool includeHeader)
{
    int count = 0;

    if (includeHeader)
    {
        printf("\n\n00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
        printf("-----------------------------------------------\n");
    }

    for (int i = 0; i < (int)size; i++)
    {
        printf("%02x ", bytes[i]);
        count++;
        if ((count % 16) == 0)
            printf("\n");
    }
    printf("\n");
}
