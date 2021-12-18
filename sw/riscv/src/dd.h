#ifndef DD_H__
#define DD_H__


#include "sys.h"


typedef enum {
    DD_DISK_EJECTED,
    DD_DISK_INSERTED,
    DD_DISK_CHANGED,
} disk_state_t;


void dd_set_disk_state (disk_state_t disk_state);
void dd_set_drive_type_development (bool value);
void dd_init (void);
void process_dd (void);


#endif
