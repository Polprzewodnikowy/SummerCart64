#ifndef DD_H__
#define DD_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    DD_DRIVE_TYPE_RETAIL = 0,
    DD_DRIVE_TYPE_DEVELOPMENT = 1,
} dd_drive_type_t;

typedef enum {
    DD_DISK_STATE_EJECTED = 0,
    DD_DISK_STATE_INSERTED = 1,
    DD_DISK_STATE_CHANGED = 2,
} dd_disk_state_t;


void dd_set_block_ready (bool valid);
dd_drive_type_t dd_get_drive_type (void);
bool dd_set_drive_type (dd_drive_type_t type);
dd_disk_state_t dd_get_disk_state (void);
bool dd_set_disk_state (dd_disk_state_t state);
bool dd_get_sd_mode (void);
void dd_set_sd_mode (bool value);
void dd_set_sd_info (uint32_t address, uint32_t length);
void dd_handle_button (void);
void dd_init (void);
void dd_process (void);


#endif
