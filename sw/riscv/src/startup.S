#define STR(x)  #x
#define XSTR(s) STR(s)
#define VERSION XSTR(__SC64_VERSION)


.section .text.reset_handler
reset_handler:
    .global reset_handler

    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    la sp, __stack_pointer$

    la ra, init_text
    jalr zero, 0(ra)

    .org 0x20, 0x00
header_text_info:
    .global header_text_info
    .ascii "SummerGovernor64 "
    .ascii VERSION
    .org 0x40, 0x00

init_text:
    la a0, _sitext
    la a1, _stext
    la a2, _etext
    call copy_section

init_data:
    la a0, _sidata
    la a1, _sdata
    la a2, _edata
    call copy_section

init_bss:
    la a0, _sbss
    la a1, _ebss
    bge a0, a1, 2f
1:
    sw zero, 0(a0)
    addi a0, a0, 4
    blt a0, a1, 1b
2:

run_in_ram:
    la ra, process_loop
    jalr zero, 0(ra)

copy_section:
    bge a1, a2, 2f
1:
    lw a3, 0(a0)
    sw a3, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    blt a1, a2, 1b
2:
    ret
