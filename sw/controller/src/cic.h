#ifndef CIC_H__
#define CIC_H__


#include <stdbool.h>
#include <stdint.h>


void cic_reset_parameters (void);
void cic_set_parameters (uint32_t *args);
void cic_set_dd_mode (bool enabled);

void cic_init (void);

void cic_process (void);


#endif
