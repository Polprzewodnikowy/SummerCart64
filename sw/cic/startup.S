.option norvc
.section .text.entry_handler
entry_handler:
    .global entry_handler

init_stack_pointer:
    .option push
    .option norelax
    la sp, _sp
    .option pop

init_bss:
    la t5, _sbss
    la t6, _ebss
    beq t5, t6, 2f
1:
    sw zero, 0(t5)
    addi t5, t5, 4
    bltu t5, t6, 1b
2:

run_main:
    tail cic_main
