#ifndef NXCI_PFS0_H
#define NXCI_PFS0_H

#include "types.h"
#include "utils.h"
#include "npdm.h"

#define MAGIC_PFS0 0x30534650

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t num_files;
    uint32_t string_table_size;
    uint32_t reserved;
} pfs0_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint64_t offset;
    uint64_t size;
    uint32_t string_table_offset;
    uint32_t reserved;
} pfs0_file_entry_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t master_hash[0x20]; /* SHA-256 hash of the hash table. */
    uint32_t block_size; /* In bytes. */
    uint32_t always_2;
    uint64_t hash_table_offset; /* Normally zero. */
    uint64_t hash_table_size; 
    uint64_t pfs0_offset;
    uint64_t pfs0_size;
    uint8_t _0x48[0xF0];
} pfs0_superblock_t;
#pragma pack(pop)

typedef struct {
    pfs0_superblock_t *superblock;
    FILE *file;
    nxci_ctx_t *tool_ctx;
    validity_t superblock_hash_validity;
    validity_t hash_table_validity;
    int is_exefs;
    npdm_t *npdm;
    pfs0_header_t *header;
} pfs0_ctx_t;

#endif
