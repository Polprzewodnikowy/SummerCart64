#include "app.h"
#include "gvr.h"
#include "hw.h"
#include "led.h"
#include "rtc.h"
#include "task.h"


#define RTC_STACK_SIZE  (256)
#define LED_STACK_SIZE  (256)
#define GVR_STACK_SIZE  (2048)


uint8_t rtc_stack[RTC_STACK_SIZE] __attribute__((aligned(8)));
uint8_t led_stack[LED_STACK_SIZE] __attribute__((aligned(8)));
uint8_t gvr_stack[GVR_STACK_SIZE] __attribute__((aligned(8)));


void app_get_stack_usage (uint32_t *usage) {
    *usage++ = 0;
    *usage++ = task_get_stack_usage(rtc_stack, RTC_STACK_SIZE);
    *usage++ = task_get_stack_usage(led_stack, LED_STACK_SIZE);
    *usage++ = task_get_stack_usage(gvr_stack, GVR_STACK_SIZE);
}

void app (void) {
    hw_init();

    task_create(TASK_ID_RTC, rtc_task, rtc_stack, RTC_STACK_SIZE);
    task_create(TASK_ID_LED, led_task, led_stack, LED_STACK_SIZE);
    task_create(TASK_ID_GVR, gvr_task, gvr_stack, GVR_STACK_SIZE);

    task_scheduler_start();
}
