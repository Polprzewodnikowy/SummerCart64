#include "init.h"
#include "process.h"


__attribute__((naked)) void main (void) {
    __asm__("la sp, __stack_pointer");
    init();
    process();
}
