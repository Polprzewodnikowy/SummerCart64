#ifndef CFG_H__
#define CFG_H__


#include <stdbool.h>
#include <stdint.h>


uint32_t cfg_get_version (void);
bool cfg_query (uint32_t *args);
bool cfg_update (uint32_t *args);
void cfg_get_time (uint32_t *args);
void cfg_set_time (uint32_t *args);
void cfg_reset (void);
void cfg_init (void);
void cfg_process (void);


#endif
