#ifndef RTC_H__
#define RTC_H__


#include "sys.h"


typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} rtc_time_t;


rtc_time_t *rtc_get_time (void);
bool rtc_is_time_valid (void);
bool rtc_is_time_running (void);
void rtc_set_time (rtc_time_t *time);
void rtc_init (void);
void process_rtc (void);


#endif
