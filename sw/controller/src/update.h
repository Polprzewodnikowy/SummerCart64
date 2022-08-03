#ifndef UPDATE_H__
#define UPDATE_H__


#include <stdint.h>


typedef enum {
    UPDATE_OK,
    UPDATE_ERROR_INVALID_HEADER,
    UPDATE_ERROR_CHECKSUM,
} update_error_t;


uint32_t update_backup (uint32_t address);
update_error_t update_prepare (uint32_t address, uint32_t length);
void update_start (void);
void update_perform (uint32_t *parameters);
void update_notify_done (void);


#endif
