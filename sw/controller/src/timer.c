#include "hw.h"
#include "timer.h"


static volatile uint32_t timer[__TIMER_ID_COUNT];


uint32_t timer_get (timer_id_t id) {
    return (uint32_t) (timer[id]);
}

void timer_set (timer_id_t id, uint32_t ticks) {
    hw_tim_disable_irq(TIM_ID_LED);
    timer[id] = ticks;
    hw_tim_enable_irq(TIM_ID_LED);
}

void timer_init (void) {
    for (timer_id_t id = 0; id < __TIMER_ID_COUNT; id++) {
        timer[id] = 0;
    }
}

void timer_update (void) {
    for (timer_id_t id = 0; id < __TIMER_ID_COUNT; id++) {
        if (timer[id] > 0) {
            timer[id] -= 1;
        }
    }
}
