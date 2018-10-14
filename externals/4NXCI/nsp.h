#ifndef NXCI_NSP_H_
#define NXCI_NSP_H_

#include <inttypes.h>
#include "filepath.h"
#include "settings.h"

#pragma pack(push, 1)
typedef struct {
    filepath_t filepath;
    char *nsp_filename;
    uint64_t filesize;
} nsp_entry_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint64_t offset;
    uint64_t size;
    uint32_t filename_offset;
    uint32_t padding;
} nsp_file_entry_table_t;
#pragma pack(pop)

typedef struct {
    filepath_t filepath;
    uint8_t entry_count;
    nsp_entry_t *nsp_entry;
} nsp_ctx_t;

void nsp_create(nsp_ctx_t *nsp_ctx);

extern nsp_ctx_t application_nsp;
extern nsp_ctx_t patch_nsp;
extern nsp_ctx_t *addon_nsps;

#endif
