#include <stdarg.h>
#include "display.h"
#include "init.h"
#include "version.h"
#include "../assets/assets.h"


void error_display (const char *fmt, ...) {
    va_list args;

    deinit();

    display_init((uint32_t *) (&assets_sc64_logo_640_240_dimmed));

    version_print();
    display_printf("[ Runtime error ]\n");
    va_start(args, fmt);
    display_vprintf(fmt, args);
    va_end(args);
    display_printf("\n");

    while (true);
}
