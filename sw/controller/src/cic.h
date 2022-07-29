#ifndef CIC_H__
#define CIC_H__


#include <stdint.h>


void cic_set_parameters (uint32_t *args);
void cic_hw_init (void);
void cic_task (void);


#endif
