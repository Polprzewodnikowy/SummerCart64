#include "custom_ops.S"


.global reset_handler

.global irq_handler
.weak irq_handler
.set irq_handler, default_irq_handler


.section .text
reset_handler:
    picorv32_maskirq_insn(zero, zero)
    j start_handler

.balign 16
irq_handler:
    call irq_handler
    picorv32_retirq_insn()

default_irq_handler:
    j default_irq_handler

start_handler:
    li ra, 0
    la sp, __stack_pointer
    li gp, 0
    li tp, 0
    li t0, 0
    li t1, 0
    li t2, 0
    li s0, 0
    li s1, 0
    li a0, 0
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    li a5, 0
    li a6, 0
    li a7, 0
    li s2, 0
    li s3, 0
    li s4, 0
    li s5, 0
    li s6, 0
    li s7, 0
    li s8, 0
    li s9, 0
    li s10, 0
    li s11, 0
    li t3, 0
    li t4, 0
    li t5, 0
    li t6, 0

init_data:
    la a0, __data_init_start
    la a1, __data_start
    la a2, __data_end
    bge a1, a2, init_bss
loop_init_data:
    lw a3, 0(a0)
    sw a3, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    blt a1, a2, loop_init_data

init_bss:
    la a0, __bss_start
    la a1, __bss_end
    bge a0, a1, entry_point
loop_init_bss:
    sw zero, 0(a0)
    addi a0, a0, 4
    blt a0, a1, loop_init_bss

entry_point:
    call main

loop:
    j loop
