#ifndef RTC_H__
#define RTC_H__


#include "sys.h"


#define RTC_ADDR            (0xDE)

#define RTC_RTCSEC          (0x00)
#define RTC_RTCSEC_ST       (1 << 7)
#define RTC_RTCMIN          (0x01)
#define RTC_RTCWKDAY        (0x03)
#define RTC_RTCWKDAY_OSCRUN (1 << 5)
#define RTC_RTCWKDAY_VBAT   (1 << 3)


uint8_t rtc_set_time (uint8_t *rtc_data);
uint8_t rtc_get_time (uint8_t *rtc_data);
void rtc_init (void);


#endif
