#ifndef EXCEPTION_H__
#define EXCEPTION_H__


#define EXCEPTION_TRIGGER(code) { asm volatile ("syscall %[c]\n" :: [c] "i" (code)); }

#define TRIGGER_CODE_ERROR      (0)
#define TRIGGER_CODE_ASSERT     (1)


#endif
