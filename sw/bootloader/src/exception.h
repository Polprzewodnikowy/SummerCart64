#ifndef EXCEPTION_H__
#define EXCEPTION_H__


#define TRIGGER_CODE_ERROR          (0)

#define EXCEPTION_TRIGGER(code)     { asm volatile ("syscall %[c]\n" :: [c] "i" (code)); }


void exception_enable_interrupts (void);
void exception_disable_interrupts (void);
void exception_enable_watchdog (void);
void exception_disable_watchdog (void);


#endif
