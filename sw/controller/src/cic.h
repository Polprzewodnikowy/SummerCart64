#ifndef CIC_H__
#define CIC_H__


#include <stdbool.h>
#include <stdint.h>


void cic_set_dd_mode (bool enabled);
void cic_set_seed (uint8_t seed);
void cic_set_checksum (uint8_t *checksum);
void cic_hw_init (void);
void cic_task (void);


#endif
