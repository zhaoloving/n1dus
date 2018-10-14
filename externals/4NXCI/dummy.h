#ifndef NXCI_DUMMY_H_
#define NXCI_DUMMY_H_

#include "filepath.h"
#include "nsp.h"

void dummy_create_cert(filepath_t *filepath, nsp_ctx_t *nsp_ctx);
void dummy_create_tik(filepath_t *filepath, nsp_ctx_t *nsp_ctx);

#endif