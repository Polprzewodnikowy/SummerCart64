#ifndef RTC_H__
#define RTC_H__


#include <stdbool.h>
#include <stdint.h>


typedef struct {
    volatile uint8_t second;
    volatile uint8_t minute;
    volatile uint8_t hour;
    volatile uint8_t weekday;
    volatile uint8_t day;
    volatile uint8_t month;
    volatile uint8_t year;
} rtc_time_t;

typedef struct {
    volatile bool led_enabled;
} rtc_settings_t;


bool rtc_get_time (rtc_time_t *time);
void rtc_set_time (rtc_time_t *time);
uint8_t rtc_get_region (void);
void rtc_set_region (uint8_t region);
rtc_settings_t *rtc_get_settings (void);
void rtc_set_settings (rtc_settings_t *settings);
void rtc_task (void);
void rtc_process (void);


#endif
