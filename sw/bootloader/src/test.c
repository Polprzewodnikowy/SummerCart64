#include <stddef.h>
#include "display.h"
#include "sc64.h"
#include "test.h"


bool test_check (void) {
    if (sc64_query_config(CFG_ID_BUTTON_STATE)) {
        return true;
    }
    return false;
}

void test_execute (void) {
    display_init(NULL);

    display_printf("SC64 Test suite\n");

    // TODO: implement tests

    while (1);
}
