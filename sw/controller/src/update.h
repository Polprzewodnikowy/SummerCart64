#ifndef UPDATE_H__
#define UPDATE_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    UPDATE_OK,
    UPDATE_ERROR_TOKEN,
    UPDATE_ERROR_CHECKSUM,
    UPDATE_ERROR_SIZE,
    UPDATE_ERROR_UNKNOWN_CHUNK,
    UPDATE_ERROR_READ,
} update_error_t;


update_error_t update_backup (uint32_t address, uint32_t *length);
update_error_t update_prepare (uint32_t address, uint32_t length);
void update_start (void);
bool update_check (void);
void update_perform (void);


#endif
