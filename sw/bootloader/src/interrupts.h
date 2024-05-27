#ifndef INTERRUPTS_H__
#define INTERRUPTS_H__


#include <stdint.h>


#define WITH_INTERRUPTS_DISABLED(x) { \
    uint32_t __sr = interrupts_disable(); \
    { x } \
    interrupts_restore(__sr); \
}


void interrupts_init (void);
uint32_t interrupts_disable (void);
void interrupts_restore (uint32_t sr);
void interrupts_start_watchdog (void);
void interrupts_stop_watchdog (void);


#endif
