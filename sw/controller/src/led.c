#include <stdbool.h>
#include "hw.h"
#include "led.h"
#include "task.h"


#define LED_TICKRATE_MS         (10)
#define LED_CYCLE_TICKS_PERIOD  (10)
#define LED_CYCLE_TICKS_ON      (3)


typedef enum {
    ON,
    OFF
} led_state_e;

typedef struct {
    uint32_t timer;
    led_state_e state;
} error_cycle_t;


static uint32_t timer = 0;
static uint8_t error_cycle = 0;
static uint32_t current_act_counter = 0;
static volatile uint32_t next_act_counter = 0;
static volatile bool cic_error = false;
static volatile bool rtc_error = false;
static const error_cycle_t error_codes[2][8] = {
    { { 50, ON }, { 50, OFF }, { 5, ON }, { 0, OFF }, { 0, OFF }, { 0, OFF }, { 0, OFF }, { 100, OFF } },
    { { 50, ON }, { 50, OFF }, { 5, ON }, { 20, OFF }, { 5, ON }, { 0, OFF }, { 0, OFF }, { 100, OFF } },
};


static void led_task_resume (void) {
    task_set_ready(TASK_ID_LED);
}

static bool led_has_errors (void) {
    return (cic_error | rtc_error);
}

static void led_process_act (void) {
    if (timer > 0) {
        timer -= 1;
        uint32_t cycle = ((LED_CYCLE_TICKS_PERIOD - 1) - (timer % LED_CYCLE_TICKS_PERIOD));
        if (cycle < LED_CYCLE_TICKS_ON) {
            hw_gpio_set(GPIO_ID_LED);
        } else {
            hw_gpio_reset(GPIO_ID_LED);
        }
    } else {
        if (current_act_counter != next_act_counter) {
            current_act_counter = next_act_counter;
            timer = LED_CYCLE_TICKS_PERIOD;
        } else {
            hw_gpio_reset(GPIO_ID_LED);
        }
    }
}

static void led_process_errors (void) {
    if (timer > 0) {
        timer -= 1;
    } else {
        uint8_t error_code = 0;
        if (cic_error) {
            error_code = 0;
        } else if (rtc_error) {
            error_code = 1;
        }
        error_cycle_t error = error_codes[error_code][error_cycle];
        timer = error.timer;
        if (error.state == ON) {
            hw_gpio_set(GPIO_ID_LED);
        } else {
            hw_gpio_reset(GPIO_ID_LED);
        }
        error_cycle += 1;
        if (error_cycle >= 8) {
            error_cycle = 0;
        }
    }
}


void led_blink_act (void) {
    next_act_counter += 1;
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

void led_task (void) {
    while (1) {
        hw_tim_setup(TIM_ID_LED, LED_TICKRATE_MS, led_task_resume);

        if (led_has_errors()) {
            led_process_errors();
        } else {
            error_cycle = 0;
            led_process_act();
        }

        task_yield();
    }
}
