#include <string.h>
#include <stdio.h>
#include "hfs0.h"
#include "nca.h"

void hfs0_process(hfs0_ctx_t *ctx) {
    /* Read *just* safe amount. */
    hfs0_header_t raw_header; 
    fseeko64(ctx->file, ctx->offset, SEEK_SET);
    if (fread(&raw_header, 1, sizeof(raw_header), ctx->file) != sizeof(raw_header)) {
        fprintf(stderr, "Failed to read HFS0 header!\n");
        throw_runtime_error(EXIT_FAILURE);
    }
    
    if (raw_header.magic != MAGIC_HFS0) {
        memdump(stdout, "Sanity: ", &raw_header, sizeof(raw_header));
        printf("Error: HFS0 is corrupt!\n");
        throw_runtime_error(EXIT_FAILURE);
    }

    uint64_t header_size = hfs0_get_header_size(&raw_header);
    ctx->header = malloc(header_size);
    if (ctx->header == NULL) {
        fprintf(stderr, "Failed to allocate HFS0 header!\n");
        throw_runtime_error(EXIT_FAILURE);
    }
    
    fseeko64(ctx->file, ctx->offset, SEEK_SET);
    if (fread(ctx->header, 1, header_size, ctx->file) != header_size) {
        fprintf(stderr, "Failed to read HFS0 header!\n");
        throw_runtime_error(EXIT_FAILURE);
    }
    
    /* Weak file validation. */
    uint64_t max_size = 0x1ULL;
    max_size <<= 48; /* Switch file sizes are capped at 48 bits. */ 
    uint64_t cur_ofs = 0;
    for (unsigned int i = 0; i < ctx->header->num_files; i++) {
        hfs0_file_entry_t *cur_file = hfs0_get_file_entry(ctx->header, i);
        if (cur_file->offset < cur_ofs) {
            printf("Error: HFS0 is corrupt!\n");
            throw_runtime_error(EXIT_FAILURE);
        }
        cur_ofs += cur_file->size;
    }
    hfs0_save(ctx);
}

int hfs0_saved_nca_process(filepath_t *filepath, nxci_ctx_t *tool)
{
    char *ext = filepath->char_path + (strlen(filepath->char_path) - 9);
    if (!(strncmp(ext,".cnmt.nca",9))) {
        nca_ctx_t nca_ctx;
        nca_init(&nca_ctx);
        nca_ctx.tool_ctx = tool;
        nca_saved_meta_process(&nca_ctx,filepath);
        nca_free_section_contexts(&nca_ctx);
    }
    return 1;
}

void hfs0_save_file(hfs0_ctx_t *ctx, uint32_t i, filepath_t *dirpath) {
    if (i >= ctx->header->num_files) {
        fprintf(stderr, "Could not save file %"PRId32"!\n", i);
        throw_runtime_error(EXIT_FAILURE);
    }
    hfs0_file_entry_t *cur_file = hfs0_get_file_entry(ctx->header, i);

    if (strlen(hfs0_get_file_name(ctx->header, i)) >= MAX_PATH - strlen(dirpath->char_path) - 1) {
        fprintf(stderr, "Filename too long in HFS0!\n");
        throw_runtime_error(EXIT_FAILURE);
    }

    filepath_t filepath;
    filepath_copy(&filepath, dirpath);
    filepath_append(&filepath, "%s", hfs0_get_file_name(ctx->header, i));
    printf("Saving %s to %s\n", hfs0_get_file_name(ctx->header, i), filepath.char_path);
    uint64_t ofs = hfs0_get_header_size(ctx->header) + cur_file->offset;
    save_file_section(ctx->file, ctx->offset + ofs, cur_file->size, &filepath);
    if (!hfs0_saved_nca_process(&filepath,ctx->tool_ctx))
    {
        throw_runtime_error(EXIT_FAILURE);
    }
}


void hfs0_save(hfs0_ctx_t *ctx) {
    /* Extract to directory. */
    filepath_t *dirpath = NULL;
    if (ctx->tool_ctx->file_type == FILETYPE_HFS0 && ctx->tool_ctx->settings.out_dir_path.enabled) {
        dirpath = &ctx->tool_ctx->settings.out_dir_path.path;
    }
    if (dirpath == NULL || dirpath->valid != VALIDITY_VALID) {
        dirpath = &ctx->tool_ctx->settings.hfs0_dir_path;
    }
    if (dirpath != NULL && dirpath->valid == VALIDITY_VALID) {
        os_makedir(dirpath->os_path);
        for (uint32_t i = 0; i < ctx->header->num_files; i++) {
            hfs0_save_file(ctx, i, dirpath);
        }
    }
}
