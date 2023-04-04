#ifndef WRITEBACK_H__
#define WRITEBACK_H__


#include <stdbool.h>
#include <stdint.h>


#define WRITEBACK_SECTOR_TABLE_SIZE     (1024)


typedef enum {
    WRITEBACK_SD,
    WRITEBACK_USB,
} writeback_mode_t;


void writeback_load_sector_table (uint32_t address);
void writeback_enable (writeback_mode_t mode);
void writeback_disable (void);
void writeback_init (void);
void writeback_process (void);


#endif
