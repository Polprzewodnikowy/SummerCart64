#ifndef DD_H__
#define DD_H__


#include "sys.h"


typedef enum {
    DD_DISK_EJECTED,
    DD_DISK_INSERTED,
    DD_DISK_CHANGED,
} disk_state_t;


void dd_set_disk_state (disk_state_t disk_state);
disk_state_t dd_get_disk_state (void);
void dd_set_drive_id (uint16_t id);
uint16_t dd_get_drive_id (void);
uint32_t dd_get_thb_table_offset (void);
void dd_init (void);
void process_dd (void);


#endif
