#include "error.h"
#include "exception.h"


void error_display (const char *fmt, ...) {
    EXCEPTION_TRIGGER(TRIGGER_CODE_ERROR);
    while (1);
}
