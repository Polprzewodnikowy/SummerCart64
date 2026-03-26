#pragma once

#include <stdint.h>

typedef enum {
    SWITCH_STATE_ON = 0,
    SWITCH_STATE_OFF = 1,
    SWITCH_STATE_LINK = 2,
} SWITCH_STATE;

bool Switch_Trigger(int16_t item_num, int16_t timer);
