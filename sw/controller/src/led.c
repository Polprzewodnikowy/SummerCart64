#include <stdbool.h>
#include "hw.h"
#include "led.h"
#include "task.h"


#define LED_MS_PER_TICK         (10)
#define LED_ERROR_TICKS_PERIOD  (50)
#define LED_ERROR_TICKS_ON      (25)
#define LED_ACT_TICKS_PERIOD    (15)
#define LED_ACT_TICKS_ON        (6)


static bool error_mode = false;
static uint32_t error_timer = 0;
static volatile bool cic_error = false;
static volatile bool rtc_error = false;

static uint32_t act_timer = 0;
static uint32_t current_act_counter = 0;
static volatile uint32_t next_act_counter = 0;


static void led_task_resume (void) {
    task_set_ready(TASK_ID_LED);
}

static void led_update_error_mode (void) {
    if (error_mode) {
        if (!(cic_error || rtc_error)) {
            hw_gpio_reset(GPIO_ID_LED);
            error_mode = false;
            act_timer = 0;
        }
    } else {
        if (cic_error || rtc_error) {
            hw_gpio_reset(GPIO_ID_LED);
            error_mode = true;
            error_timer = 0;
        }
    }
}

static void led_process_errors (void) {
    if (error_timer == 0) {
        error_timer = LED_ERROR_TICKS_PERIOD;
        if (cic_error) {
            error_timer *= 1;
        } else if (rtc_error) {
            error_timer *= 2;
        }
        error_timer += LED_ERROR_TICKS_PERIOD;
    }

    if (error_timer > 0) {
        error_timer -= 1;
        if (error_timer >= LED_ERROR_TICKS_PERIOD) {
            uint32_t error_cycle = (error_timer % LED_ERROR_TICKS_PERIOD);
            if (error_cycle == LED_ERROR_TICKS_ON) {
                hw_gpio_set(GPIO_ID_LED);
            }
            if (error_cycle == 0) {
                hw_gpio_reset(GPIO_ID_LED);
            }
        }
    }
}

static void led_process_act (void) {
    if (act_timer == 0) {
        if (current_act_counter != next_act_counter) {
            current_act_counter = next_act_counter;
            act_timer = LED_ACT_TICKS_PERIOD;
        }
    }

    if (act_timer > 0) {
        act_timer -= 1;
        if (act_timer == LED_ACT_TICKS_ON) {
            hw_gpio_set(GPIO_ID_LED);
        }
        if (act_timer == 0) {
            hw_gpio_reset(GPIO_ID_LED);
        }
    }
}


void led_blink_error (led_error_t error) {
    switch (error) {
        case LED_ERROR_CIC:
            cic_error = true;
            break;
        case LED_ERROR_RTC:
            rtc_error = true;
            break;
    }
}

void led_clear_error (led_error_t error) {
    switch (error) {
        case LED_ERROR_CIC:
            cic_error = false;
            break;
        case LED_ERROR_RTC:
            rtc_error = false;
            break;
    }
}

void led_blink_act (void) {
    next_act_counter += 1;
}

void led_task (void) {
    while (1) {
        hw_tim_setup(TIM_ID_LED, LED_MS_PER_TICK, led_task_resume);

        led_update_error_mode();

        if (error_mode) {
            led_process_errors();
        } else {
            led_process_act();
        }

        task_yield();
    }
}
