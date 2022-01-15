#include "exception.h"


void error_display (const char *fmt, ...) {
    EXCEPTION_TRIGGER(TRIGGER_CODE_ERROR);
    while (1);
}

void __assert_func (const char *file, int line, const char *func, const char *failedexpr) {
    EXCEPTION_TRIGGER(TRIGGER_CODE_ASSERT);
    while (1);
}
