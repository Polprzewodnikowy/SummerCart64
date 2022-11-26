#ifndef WRITEBACK_H__
#define WRITEBACK_H__


#include <stdbool.h>
#include <stdint.h>


void writeback_set_sd_info (uint32_t address, bool enabled);
void writeback_init (void);
void writeback_process (void);


#endif
