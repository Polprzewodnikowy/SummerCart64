#ifndef INTERRUPTS_H__
#define INTERRUPTS_H__


#include <stdint.h>


void interrupts_init (void);
uint32_t interrupts_disable (void);
void interrupts_restore (uint32_t sr);
void interrupts_start_watchdog (void);
void interrupts_stop_watchdog (void);


#endif
