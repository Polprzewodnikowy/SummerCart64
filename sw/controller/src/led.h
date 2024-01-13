#ifndef LED_H__
#define LED_H__


typedef enum {
    LED_ERROR_CIC,
    LED_ERROR_RTC,
} led_error_t;


void led_blink_error (led_error_t error);
void led_clear_error (led_error_t error);
void led_blink_act (void);
// void led_task (void);


#endif
