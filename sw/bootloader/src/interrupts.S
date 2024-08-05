#include "vr4300.h"


#define WATCHDOG_TIMEOUT    (5 * (93750000UL / 2))


.section .text.interrupts


.type interrupts_init, %function
.global interrupts_init
interrupts_init:
    li $t1, (C0_SR_IM4 | C0_SR_IM3 | C0_SR_IE)
    mfc0 $t0, C0_STATUS
    or $t0, $t1
    mtc0 $t0, C0_STATUS
    jr $ra


.type interrupts_disable, %function
.global interrupts_disable
interrupts_disable:
    li $t0, ~(C0_SR_IE)
    mfc0 $v0, C0_STATUS
    and $t0, $v0
    mtc0 $t0, C0_STATUS
    jr $ra


.type interrupts_restore, %function
.global interrupts_restore
interrupts_restore:
    mtc0 $a0, C0_STATUS
    jr $ra


.type interrupts_start_watchdog, %function
.global interrupts_start_watchdog
interrupts_start_watchdog:
    mtc0 $zero, C0_COUNT
    li $t1, WATCHDOG_TIMEOUT
    mtc0 $t1, C0_COMPARE
    li $t1, C0_SR_IM7
    mfc0 $t0, C0_STATUS
    or $t0, $t1
    mtc0 $t0, C0_STATUS
    jr $ra


.type interrupts_stop_watchdog, %function
.global interrupts_stop_watchdog
interrupts_stop_watchdog:
    li $t1, ~(C0_SR_IM7)
    mfc0 $t0, C0_STATUS
    and $t0, $t1
    mtc0 $t0, C0_STATUS
    mtc0 $zero, C0_COMPARE
    jr $ra
