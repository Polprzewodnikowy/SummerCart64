#include <stdbool.h>
#include <stdint.h>
#include <stm32g0xx.h>
#include "task.h"


#define TASK_INITIAL_XPSR       (0x21000000UL)
#define TASK_CONTEXT_SWITCH()   { SCB->ICSR = (1 << SCB_ICSR_PENDSVSET_Pos); }
#define TASK_STACK_FILL_VALUE   (0xDEADBEEF)


typedef struct {
    volatile uint32_t sp;
    volatile bool ready;
} task_t;


static task_t task_table[__TASK_ID_MAX];
static volatile task_id_t task_current = 0;


static void task_exit (void) {
    while (1) {
        task_yield();
    }
}

static uint32_t task_switch_context (uint32_t sp) {
    task_table[task_current].sp = sp;

    for (task_id_t id = 0; id < __TASK_ID_MAX; id++) {
        if (task_table[id].ready) {
            task_current = id;
            break;
        }
    }

    return task_table[task_current].sp;
}


void task_create (task_id_t id, void (*code)(void), void *stack, size_t stack_size) {
    if (id < __TASK_ID_MAX) {
        for (size_t i = 0; i < stack_size; i += sizeof(uint32_t)) {
            (*(uint32_t *) (stack + i)) = TASK_STACK_FILL_VALUE;
        }
        uint32_t *sp =  ((uint32_t *) ((uint32_t) (stack) + stack_size));
        *--sp = TASK_INITIAL_XPSR;
        *--sp = (uint32_t) (code);
        *--sp = ((uint32_t) (task_exit));
        for (int i = 0; i < 13; i++) {
            *--sp = 0;
        }
        task_t *task = &task_table[id];
        task->sp = ((uint32_t) (sp));
        task->ready = true;
    }
}

void task_yield (void) {
    __disable_irq();
    task_table[task_current].ready = false;
    __enable_irq();
    TASK_CONTEXT_SWITCH();
}

void task_set_ready (task_id_t id) {
    __disable_irq();
    task_table[id].ready = true;
    __enable_irq();
    TASK_CONTEXT_SWITCH();
}

size_t task_get_stack_usage (void *stack, size_t stack_size) {
    for (size_t i = 0; i < stack_size; i += sizeof(uint32_t)) {
        if ((*(uint32_t *) (stack + i)) != TASK_STACK_FILL_VALUE) {
            return (stack_size - i);
        }
    }
    return 0;
}

__attribute__((naked)) void task_scheduler_start (void) {
    uint32_t sp = task_table[task_current].sp;

    NVIC_SetPriority(PendSV_IRQn, 3);

    asm volatile (
        "add %[sp], #32 \n"
        "msr psp, %[sp] \n"
        "movs r0, #2 \n"
        "msr CONTROL, r0 \n"
        "isb \n"
        "pop {r0-r5} \n"
        "mov lr, r5 \n"
        "pop {r3} \n"
        "pop {r2} \n"
        "cpsie i \n"
        "bx r3 \n"
        :: [sp] "r" (sp)
    );

    while (1);
}


__attribute__((naked)) void PendSV_Handler (void) {
    asm volatile (
        "mrs r1, psp \n"
        "sub r1, r1, #32 \n"
        "mov r0, r1 \n"
        "stmia r1!, {r4-r7} \n"
        "mov r4, r8 \n"
        "mov r5, r9 \n"
        "mov r6, r10 \n"
        "mov r7, r11 \n"
        "stmia r1!, {r4-r7} \n"
        "push {lr} \n"
        "cpsid i \n"
        "blx %[task_switch_context] \n"
        "cpsie i \n"
        "pop {r2} \n"
        "add r0, #16 \n"
        "ldmia r0!, {r4-r7} \n"
        "mov r8, r4 \n"
        "mov r9, r5 \n"
        "mov r10, r6 \n"
        "mov r11, r7 \n"
        "msr psp, r0 \n"
        "sub r0, #32 \n"
        "ldmia r0!, {r4-r7} \n"
        "bx r2 \n"
        :: [task_switch_context] "r" (task_switch_context)
    );
}
