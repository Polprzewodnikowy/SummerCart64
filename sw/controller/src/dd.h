#ifndef DD_H__
#define DD_H__


#include <stdbool.h>


typedef enum {
    DD_DRIVE_TYPE_RETAIL = 0,
    DD_DRIVE_TYPE_DEVELOPMENT = 1,
} dd_drive_type_t;

typedef enum {
    DD_DISK_STATE_EJECTED = 0,
    DD_DISK_STATE_INSERTED = 1,
    DD_DISK_STATE_CHANGED = 2,
} dd_disk_state_t;


void dd_set_drive_type (dd_drive_type_t type);
void dd_set_disk_state (dd_disk_state_t state);
void dd_set_block_ready (bool valid);
void dd_init (void);
void dd_process (void);


#endif
