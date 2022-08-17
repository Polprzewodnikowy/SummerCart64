#ifndef VENDOR_H__
#define VENDOR_H__


#include <stdint.h>


typedef enum {
    VENDOR_OK,
    VENDOR_ERROR_ARGS,
    VENDOR_ERROR_INIT,
    VENDOR_ERROR_ERASE,
    VENDOR_ERROR_PROGRAM,
    VENDOR_ERROR_VERIFY,
} vendor_error_t;


uint32_t vendor_flash_size (void);
vendor_error_t vendor_backup (uint32_t address, uint32_t *length);
vendor_error_t vendor_update (uint32_t address, uint32_t length);
vendor_error_t vendor_reconfigure (void);


#endif
