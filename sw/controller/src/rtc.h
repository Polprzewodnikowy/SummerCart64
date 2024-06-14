#ifndef RTC_H__
#define RTC_H__


#include <stdbool.h>
#include <stdint.h>


typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} rtc_real_time_t;

typedef struct {
    bool led_enabled;
} rtc_settings_t;


void rtc_get_time (rtc_real_time_t *time);
void rtc_set_time (rtc_real_time_t *time);

uint8_t rtc_get_region (void);
void rtc_set_region (uint8_t region);

rtc_settings_t *rtc_get_settings (void);
void rtc_save_settings (void);

void rtc_init (void);

void rtc_process (void);


#endif
