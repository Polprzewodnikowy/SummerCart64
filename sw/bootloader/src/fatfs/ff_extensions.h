#ifndef FF_EXTENSIONS_H__
#define FF_EXTENSIONS_H__


#include "diskio.h"


typedef DRESULT (*transfer_function_t)(BYTE, FSIZE_t, LBA_t, UINT, UINT);


FRESULT fe_load(const TCHAR *path, UINT max_length, transfer_function_t transfer_function);


#endif
