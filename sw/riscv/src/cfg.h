#ifndef CFG_H__
#define CFG_H__


#include "sys.h"


uint32_t cfg_get_version (void);
void cfg_query (uint32_t *args);
void cfg_update (uint32_t *args);
void cfg_get_time (uint32_t *args);
void cfg_set_time (uint32_t *args);
void cfg_init (void);
void process_cfg (void);


#endif
