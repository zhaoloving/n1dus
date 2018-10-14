#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "utils.h"
#include "filepath.h"
#include "sha.h"

#include <switch.h>
extern float progress;
extern float progressSpeed;

// Virtual file system
void*       vfcreate(bool split);
void        vfdestroy(void* vFile);
int         vfopen(void* vFile, const char *fn, const char *mode);
int         vfclose(void* vFile);
size_t      vfwrite(const void * ptr, size_t size, size_t count, void* vFile);
size_t      vfread(void * ptr, size_t size, size_t count, void* vFile);
int         vfseek(void* vFile, uint64_t offset, int origin);
uint64_t    vftell(void* vFile);
uint64_t    vfchunkSize();


uint32_t align(uint32_t offset, uint32_t alignment) {
    uint32_t mask = ~(alignment-1);

    return (offset + (alignment-1)) & mask;
}

uint64_t align64(uint64_t offset, uint64_t alignment) {
    uint64_t mask = ~(uint64_t)(alignment-1);

    return (offset + (alignment-1)) & mask;
}


/* Taken mostly from ctrtool. */
void memdump(FILE *f, const char *prefix, const void *data, size_t size) {
    uint8_t *p = (uint8_t *)data;

    unsigned int prefix_len = strlen(prefix);
    size_t offset = 0;
    int first = 1;

    while (size) {
        unsigned int max = 32;

        if (max > size) {
            max = size;
        }

        if (first) {
            fprintf(f, "%s", prefix);
            first = 0;
        } else {
            fprintf(f, "%*s", prefix_len, "");
        }

        for (unsigned int i = 0; i < max; i++) {
            fprintf(f, "%02X", p[offset++]);
        }

        fprintf(f, "\n");

        size -= max;
    }
}

void save_buffer_to_file(void *buf, uint64_t size, struct filepath *filepath) {
    FILE *f_out = os_fopen(filepath->os_path, OS_MODE_WRITE);

    if (f_out == NULL) {
        fprintf(stderr, "Failed to open %s!\n", filepath->char_path);
        return;
    }
    
    fwrite(buf, 1, size, f_out);
    
    fclose(f_out);
}

void save_buffer_to_directory_file(void *buf, uint64_t size, struct filepath *dirpath, const char *filename) {
    struct filepath filepath;
    filepath_copy(&filepath, dirpath);
    filepath_append(&filepath, filename);
    if (filepath.valid == VALIDITY_VALID) {
        save_buffer_to_file(buf, size, &filepath);
    } else {
        fprintf(stderr, "Failed to create filepath!\n");
        throw_runtime_error(EXIT_FAILURE);
    }
}

void save_file_section(FILE *f_in, uint64_t ofs, uint64_t total_size, filepath_t *filepath) {

    void* vf_out = vfcreate(total_size > vfchunkSize());
    int res = vfopen(vf_out, filepath->char_path, OS_MODE_WRITE);
    if (res == 1)
    {
        fprintf(stderr, "Failed to open virtual file %s!\n", filepath->char_path);
        vfdestroy(vf_out);
        return;
    }

    uint64_t read_size = 0x400000; /* 4 MB buffer. */
    unsigned char *buf = malloc(read_size);
    if (buf == NULL) {
        fprintf(stderr, "Failed to allocate file-save buffer!\n");
        vfclose(vf_out);
        vfdestroy(vf_out);
        throw_runtime_error(EXIT_FAILURE);
    }
    memset(buf, 0xCC, read_size); /* Debug in case I fuck this up somehow... */
    uint64_t end_ofs = ofs + total_size;
    fseeko64(f_in, ofs, SEEK_SET);

    uint64_t readBytes = 0;
    uint64_t sizeToRead = end_ofs-ofs;

    uint64_t freq       = armGetSystemTickFreq();
    uint64_t startTime  = armGetSystemTick();
    progress            = 0.f;
    progressSpeed       = 0.f;

    while (ofs < end_ofs) {
        // Progress in %
        progress = (float)readBytes / (float)sizeToRead;

        // Progress speed in MB/s
        uint64_t newTime = armGetSystemTick();
        if (newTime - startTime >= freq)
        {
            double mbRead       = readBytes / (1024.0 * 1024.0);
            double duration     = (double)(newTime - startTime) / (double)freq;
            progressSpeed       = (float)(mbRead / duration);
        }

        if (readBytes % (0x400000 * 3) == 0)
            printf("> Saving Progress: %lu/%lu MB (%d%s)\n", (readBytes / 1000000), (sizeToRead / 1000000), (int)(progress * 100.0), "%");

        if (ofs + read_size >= end_ofs) read_size = end_ofs - ofs;
        if (fread(buf, 1, read_size, f_in) != read_size) {
            fprintf(stderr, "Failed to read file!\n");
            vfclose(vf_out);
            vfdestroy(vf_out);
            throw_runtime_error(EXIT_FAILURE);
        }
        vfwrite(buf, 1, read_size, vf_out);
        ofs += read_size;
        readBytes += read_size;
    }
    progress = 1.f;

    vfclose(vf_out);
    vfdestroy(vf_out);

    free(buf);
}

validity_t check_memory_hash_table(FILE *f_in, unsigned char *hash_table, uint64_t data_ofs, uint64_t data_len, uint64_t block_size, int full_block) {
    if (block_size == 0) {
        /* Block size of 0 is always invalid. */
        return VALIDITY_INVALID;
    }
    unsigned char cur_hash[0x20];
    uint64_t read_size = block_size;
    unsigned char *block = malloc(block_size);
    if (block == NULL) {
        fprintf(stderr, "Failed to allocate hash block!\n");
        throw_runtime_error(EXIT_FAILURE);
    }

    validity_t result = VALIDITY_VALID;
    unsigned char *cur_hash_table_entry = hash_table;
    for (uint64_t ofs = 0; ofs < data_len; ofs += read_size) {
        fseeko64(f_in, ofs + data_ofs, SEEK_SET);
        if (ofs + read_size > data_len) {
            /* Last block... */
            memset(block, 0, read_size);
            read_size = data_len - ofs;
        }
        
        if (fread(block, 1, read_size, f_in) != read_size) {
            fprintf(stderr, "Failed to read file!\n");
            throw_runtime_error(EXIT_FAILURE);
        }        
        sha256_hash_buffer(cur_hash, block, full_block ? block_size : read_size);
        if (memcmp(cur_hash, cur_hash_table_entry, 0x20) != 0) {
            result = VALIDITY_INVALID;
            break;
        }
        cur_hash_table_entry += 0x20;
    }
    free(block);

    return result;

}

validity_t check_file_hash_table(FILE *f_in, uint64_t hash_ofs, uint64_t data_ofs, uint64_t data_len, uint64_t block_size, int full_block) {
    if (block_size == 0) {
        /* Block size of 0 is always invalid. */
        return VALIDITY_INVALID;
    }    
    uint64_t hash_table_size = data_len / block_size;
    if (data_len % block_size) hash_table_size++;
    hash_table_size *= 0x20;
    unsigned char *hash_table = malloc(hash_table_size);
    if (hash_table == NULL) {
        fprintf(stderr, "Failed to allocate hash table!\n");
        throw_runtime_error(EXIT_FAILURE);
    }

    fseeko64(f_in, hash_ofs, SEEK_SET);
    if (fread(hash_table, 1, hash_table_size, f_in) != hash_table_size) {
        fprintf(stderr, "Failed to read file!\n");
        throw_runtime_error(EXIT_FAILURE);
    }

    validity_t result = check_memory_hash_table(f_in, hash_table, data_ofs, data_len, block_size, full_block);

    free(hash_table);

    return result;
}

FILE *open_key_file(const char *prefix) {
    filepath_t keypath;
    filepath_init(&keypath);
    /* Use $HOME/.switch/prod.keys if it exists */
    char *home = getenv("HOME");
    if (home == NULL)
        home = getenv("USERPROFILE");
    if (home != NULL) {
        filepath_set(&keypath, home);
        filepath_append(&keypath, ".switch");
        filepath_append(&keypath, "%s.keys", prefix);
    }

    /* Load external keys, if relevant. */
    FILE *keyfile = NULL;
    if (keypath.valid == VALIDITY_VALID) {
        keyfile = os_fopen(keypath.os_path, OS_MODE_READ);
    }

    /* If $HOME/.switch/prod.keys don't exist, try using $XDG_CONFIG_HOME */
    if (keyfile == NULL) {
        char *xdgconfig = getenv("XDG_CONFIG_HOME");
        if (xdgconfig != NULL)
            filepath_set(&keypath, xdgconfig);
        else if (home != NULL) {
            filepath_set(&keypath, home);
            filepath_append(&keypath, ".config");
        }
        /* Keypath contains xdg config. Add switch/%s.keys */
        filepath_append(&keypath, "switch");
        filepath_append(&keypath, "%s.keys", prefix);
    }

    if (keyfile == NULL && keypath.valid == VALIDITY_VALID) {
        keyfile = os_fopen(keypath.os_path, OS_MODE_READ);
    }
    
    return keyfile;
}

// Code by NullModel https://github.com/ENCODE-DCC/kentUtils/commits?author=NullModel
char hexTab[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
'8', '9', 'a', 'b', 'c', 'd', 'e', 'f', };
void hexBinaryString(unsigned char *in, int inSize, char *out, int outSize)
/* Convert possibly long binary string to hex string.
 * Out size needs to be at least 2x inSize+1 */
{
assert(inSize * 2 +1 <= outSize);
while (--inSize >= 0)
    {
    unsigned char c = *in++;
    *out++ = hexTab[c>>4];
    *out++ = hexTab[c&0xf];
    }
*out = 0;
}

/////////////


#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR '/'
#endif

#if defined (_WIN32) || defined (__MSDOS__) || defined (__DJGPP__) || \
  defined (__OS2__)
#define HAVE_DOS_BASED_FILE_SYSTEM
#ifndef DIR_SEPARATOR_2
#define DIR_SEPARATOR_2 '\\'
#endif
#endif

/* Define IS_DIR_SEPARATOR.  */
#ifndef DIR_SEPARATOR_2
# define IS_DIR_SEPARATOR(ch) ((ch) == DIR_SEPARATOR)
#else /* DIR_SEPARATOR_2 */
# define IS_DIR_SEPARATOR(ch) \
	(((ch) == DIR_SEPARATOR) || ((ch) == DIR_SEPARATOR_2))
#endif /* DIR_SEPARATOR_2 */

char* basename2 (const char *name)
{
  const char *base;

#if defined (HAVE_DOS_BASED_FILE_SYSTEM)
  /* Skip over the disk name in MSDOS pathnames. */
  if (ISALPHA (name[0]) && name[1] == ':')
    name += 2;
#endif

  for (base = name; *name; name++)
    {
      if (IS_DIR_SEPARATOR (*name))
	{
	  base = name + 1;
	}
    }
  return (char *) base;
}


