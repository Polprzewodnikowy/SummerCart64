.section .rodata.metadata
metadata:
    .word  __ram_size

.section .text.startup
.global reset_handler
reset_handler:
    la sp, __stack_pointer
    j main
