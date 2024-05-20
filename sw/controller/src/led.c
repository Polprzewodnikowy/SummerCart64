#include <stdbool.h>
#include "hw.h"
#include "led.h"
#include "rtc.h"
#include "timer.h"


#define LED_REFRESH_PERIOD_MS   (50)

#define LED_PULSE_LENGTH_MS     (200)

#define ERROR_BLINK_PERIOD_MS   (100)
#define ERROR_TOTAL_PERIOD_MS   (1000)

#define CIC_ERROR_BLINKS        (1)
#define RTC_ERROR_BLINKS        (2)


static bool activity_pulse = false;
static int activity_pulse_timer = 0;

static bool cic_error = false;
static bool rtc_error = false;
static bool error_active = false;
static int error_timer = 0;


void led_activity_on (void) {
    rtc_settings_t *settings = rtc_get_settings();
    if (!activity_pulse && !error_active && settings->led_enabled) {
        hw_gpio_set(GPIO_ID_LED);
    }
}

void led_activity_off (void) {
    rtc_settings_t *settings = rtc_get_settings();
    if (!activity_pulse && !error_active && settings->led_enabled) {
        hw_gpio_reset(GPIO_ID_LED);
    }
}

void led_activity_pulse (void) {
    activity_pulse = true;
    activity_pulse_timer = LED_PULSE_LENGTH_MS;
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

    error_active = true;
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


void led_init (void) {
    timer_countdown_start(TIMER_ID_LED, LED_REFRESH_PERIOD_MS);
}


void led_process (void) {
    if (!timer_countdown_elapsed(TIMER_ID_LED)) {
        return;
    }

    timer_countdown_start(TIMER_ID_LED, LED_REFRESH_PERIOD_MS);

    if (error_active) {
        int blinks = 0;

        if (cic_error) {
            blinks = CIC_ERROR_BLINKS;
        } else if (rtc_error) {
            blinks = RTC_ERROR_BLINKS;
        } else {
            activity_pulse = false;
            activity_pulse_timer = 0;
            error_active = false;
            error_timer = 0;
            hw_gpio_reset(GPIO_ID_LED);
            return;
        }

        bool led_on = false;

        for (int i = 0; i < blinks; i++) {
            bool lower_bound = (error_timer >= (ERROR_BLINK_PERIOD_MS * (i * 2)));
            bool upper_bound = (error_timer < (ERROR_BLINK_PERIOD_MS * ((i * 2) + 1)));
            if (lower_bound && upper_bound) {
                led_on = true;
                break;
            }
        }

        if (led_on) {
            hw_gpio_set(GPIO_ID_LED);
        } else {
            hw_gpio_reset(GPIO_ID_LED);
        }

        error_timer += LED_REFRESH_PERIOD_MS;

        if (error_timer >= ERROR_TOTAL_PERIOD_MS) {
            error_timer = 0;
        }

        return;
    }

    if (activity_pulse) {
        if (activity_pulse_timer > 0) {
            hw_gpio_set(GPIO_ID_LED);
        } else {
            activity_pulse = false;
            hw_gpio_reset(GPIO_ID_LED);
            return;
        }

        activity_pulse_timer -= LED_REFRESH_PERIOD_MS;
    }
}
