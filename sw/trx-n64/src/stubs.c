/*
 * TRX-N64: Stubs for bootloader symbols we don't use.
 *
 * The bootloader's exception handler and init code reference symbols
 * from the bootloader's own modules. We provide no-op stubs.
 */

#include <stdint.h>

/* SC64 logo asset (used by exception/interrupt handlers for error screen) */
const uint32_t assets_sc64_logo_640_240_dimmed[] __attribute__((section(".rodata"))) = { 0 };

/* Version print (used by error screens) */
void version_print(void) {}

/* Error display (used by init and SC64 error paths) */
void error_display(const char *fmt, ...) { (void)fmt; while(1); }

/* Test framework (used by init — optional self-test on boot) */
uint32_t test_check(void) { return 0; }
void test_execute(void) {}
