#ifndef WRITEBACK_H__
#define WRITEBACK_H__


#include <stdbool.h>
#include <stdint.h>


#define WRITEBACK_SECTOR_TABLE_SIZE     (1024)


void writeback_load_sector_table (uint32_t address);
void writeback_enable (void);
void writeback_disable (void);
void writeback_init (void);
void writeback_process (void);


#endif
