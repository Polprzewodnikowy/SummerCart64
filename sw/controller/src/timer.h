#ifndef TIMER_H__
#define TIMER_H__


#include <stdbool.h>
#include <stdint.h>


typedef enum {
    TIMER_ID_DD,
    TIMER_ID_RTC,
    TIMER_ID_SD,
    TIMER_ID_USB,
    TIMER_ID_WRITEBACK,
    __TIMER_ID_COUNT
} timer_id_t;


void timer_countdown_start (timer_id_t id, uint32_t value_ms);
void timer_countdown_abort (timer_id_t id);
bool timer_countdown_elapsed (timer_id_t id);

void timer_init (void);


#endif
