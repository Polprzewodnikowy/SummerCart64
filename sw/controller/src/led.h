#ifndef LED_H__
#define LED_H__


typedef enum {
    LED_ERROR_CIC,
    LED_ERROR_RTC,
} led_error_t;


void led_activity_on (void);
void led_activity_off (void);
void led_activity_pulse (void);

void led_blink_error (led_error_t error);
void led_clear_error (led_error_t error);

void led_init (void);

void led_process (void);


#endif
