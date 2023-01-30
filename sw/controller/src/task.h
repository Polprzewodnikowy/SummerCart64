#ifndef TASK_H__
#define TASK_H__


#include <stddef.h>


typedef enum {
    TASK_ID_CIC,
    TASK_ID_RTC,
    TASK_ID_LED,
    TASK_ID_GVR,
    __TASK_ID_MAX
} task_id_t;


void task_create (task_id_t id, void (*code)(void), void *stack, size_t stack_size);
void task_yield (void);
void task_set_ready (task_id_t id);
void task_set_ready_and_reset (task_id_t id);
size_t task_get_stack_usage (void *stack, size_t stack_size);
void task_scheduler_start (void);


#endif
