.section .reset
.global reset_handler
reset_handler:
    la sp, __stack_pointer
    j main
