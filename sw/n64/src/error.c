#include <stdarg.h>
#include "exception.h"


void error_display (const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    EXCEPTION_TRIGGER(TRIGGER_CODE_ERROR);
    va_end(args);
}
