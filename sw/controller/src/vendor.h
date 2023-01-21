#ifndef VENDOR_H__
#define VENDOR_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    VENDOR_OK,
    VENDOR_ERROR_ARGS,
    VENDOR_ERROR_INIT,
    VENDOR_ERROR_ERASE,
    VENDOR_ERROR_PROGRAM,
    VENDOR_ERROR_VERIFY,
} vendor_error_t;

typedef uint8_t vendor_get_cmd_t (uint8_t *buffer, uint8_t *rx_length);
typedef void vendor_send_response_t (uint8_t cmd, uint8_t *buffer, uint8_t tx_length, bool error);


uint32_t vendor_flash_size (void);
vendor_error_t vendor_backup (uint32_t address, uint32_t *length);
vendor_error_t vendor_update (uint32_t address, uint32_t length);
vendor_error_t vendor_reconfigure (void);

void vendor_initial_configuration (vendor_get_cmd_t get_cmd, vendor_send_response_t send_response);


#endif
