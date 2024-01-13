#include "hw.h"
#include "timer.h"


#define TIMER_PERIOD_MS     (50)


typedef struct {
    volatile bool pending;
    volatile bool running;
    volatile uint32_t value;
} timer_t;

static timer_t timer[__TIMER_ID_COUNT];


static void timer_update (void) {
    for (timer_id_t id = 0; id < __TIMER_ID_COUNT; id++) {
        if (timer[id].value > 0) {
            timer[id].value -= 1;
        }
        if (timer[id].pending) {
            timer[id].pending = false;
        } else if(timer[id].value == 0) {
            timer[id].running = false;
        }
    }
}


void timer_countdown_start (timer_id_t id, uint32_t value_ms) {
    hw_enter_critical();
    if (value_ms > 0) {
        timer[id].pending = true;
        timer[id].running = true;
        timer[id].value = ((value_ms + (TIMER_PERIOD_MS - 1)) / TIMER_PERIOD_MS);
    }
    hw_exit_critical();
}

void timer_countdown_abort (timer_id_t id) {
    hw_enter_critical();
    timer[id].pending = false;
    timer[id].running = false;
    timer[id].value = 0;
    hw_exit_critical();
}

bool timer_countdown_elapsed (timer_id_t id) {
    return (!timer[id].running);
}


void timer_init (void) {
    hw_systick_config(TIMER_PERIOD_MS, timer_update);
}
