#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>
#include <string.h>
#include "dummy.h"
#include "dummy_files.h"


void dummy_create_cert(filepath_t *filepath, nsp_ctx_t *nsp_ctx)
{
    printf("Creating dummy cert %s\n",filepath->char_path);
    FILE *file;
    if (!(file = os_fopen(filepath->os_path, OS_MODE_WRITE)))
    {
        fprintf(stderr,"unable to create dummy cert\n");
        throw_runtime_error(EXIT_FAILURE);
    }
    fwrite(dummy_cert,1,DUMMYCERTSIZE,file);
    fclose(file);

    // Set file size for creating nsp
    nsp_ctx->nsp_entry[1].filesize = DUMMYCERTSIZE;

    // Set file path for creating nsp
    filepath_init(&nsp_ctx->nsp_entry[1].filepath);
    filepath_copy(&nsp_ctx->nsp_entry[1].filepath, filepath);

    // Set new filename for creating nsp
    nsp_ctx->nsp_entry[1].nsp_filename = (char *)calloc(1, 38);
    strncpy(nsp_ctx->nsp_entry[1].nsp_filename, basename2(nsp_ctx->nsp_entry[1].filepath.char_path), 37);
}

void dummy_create_tik(filepath_t *filepath, nsp_ctx_t *nsp_ctx)
{
    printf("Creating dummy tik %s\n",filepath->char_path);
    FILE *file;
    if (!(file = os_fopen(filepath->os_path, OS_MODE_WRITE)))
    {
        fprintf(stderr,"unable to create dummy tik\n");
        throw_runtime_error(EXIT_FAILURE);
    }
    fwrite(dummy_tik,1,DUMMYTIKSIZE,file);
    fclose(file);

    // Set file size for creating nsp
    nsp_ctx->nsp_entry[0].filesize = DUMMYTIKSIZE;

    // Set file path for creating nsp
    filepath_init(&nsp_ctx->nsp_entry[0].filepath);
    filepath_copy(&nsp_ctx->nsp_entry[0].filepath, filepath);
    
    // Set new filename for creating nsp
    nsp_ctx->nsp_entry[0].nsp_filename = (char *)calloc(1, 37);
    strncpy(nsp_ctx->nsp_entry[0].nsp_filename, basename2(nsp_ctx->nsp_entry[0].filepath.char_path), 36);
}
