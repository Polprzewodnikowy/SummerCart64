#ifndef TIMER_H__
#define TIMER_H__


#include <stdint.h>


typedef enum {
    TIMER_ID_USB,
    TIMER_ID_WRITEBACK,
    __TIMER_ID_COUNT
} timer_id_t;


uint32_t timer_get (timer_id_t id);
void timer_set (timer_id_t id, uint32_t ticks);
void timer_init (void);
void timer_update (void);


#endif
