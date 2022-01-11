#include "error.h"
#include "exception.h"


void error_display (const char *message) {
    EXCEPTION_TRIGGER(TRIGGER_CODE_ERROR);
    while (1);
}
