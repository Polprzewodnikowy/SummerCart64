/*
 * TRX-N64: Linker script for Tomb Raider N64 port
 *
 * Extended from SC64 bootloader linker script with larger memory
 * regions for game code and data.
 *
 * Memory map (8 MB Expansion Pak assumed):
 *
 *   0x80000000 - 0x800003FF  Exception vectors (1 KB)
 *   0x80000400 - 0x800FFFFF  Code + data + heap (1020 KB)
 *   0x80100000 - 0x8017FFFF  Framebuffer 0 (320×240×2 = 150 KB)
 *   0x80180000 - 0x801FFFFF  Framebuffer 1 (150 KB)
 *   0x80200000 - 0x80227FFF  Z-buffer (320×240×2 = 150 KB)
 *   0x80228000 - 0x802FFFFF  Game heap (864 KB)
 *   0x80300000 - 0x807FFFFF  Available for expansion (5 MB)
 *
 * The asset streaming cache lives in SC64 SDRAM space:
 *   0xB1000000+  (accessed via PI, not in RDRAM)
 */

MEMORY {
    exceptions  (rx)  : org = 0x80000000, len = 0x400
    ram         (rwx) : org = 0x80000400, len = 7M
    framebuf0   (rw)  : org = 0x80720000, len = 160k
    framebuf1   (rw)  : org = 0x80748000, len = 160k
    zbuffer     (rw)  : org = 0x80770000, len = 160k
}

ENTRY(entry_handler)

__stack_size = 32k;
__exception_stack_size = 8k;

SECTIONS {
    .exceptions : {
        KEEP(*(.text.exception_vector));
        . = ALIGN(8);
    } > exceptions

    .text : {
        *(.text.entry_handler)
        . = ALIGN(8);
        *(.text .text.* .gnu.linkonce.t.*)
        . = ALIGN(8);
    } > ram

    .rodata : {
        *(.rdata .rodata .rodata.* .gnu.linkonce.r.*)
        . = ALIGN(8);
    } > ram

    .data : {
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(8);
    } > ram

    .sdata : {
        _gp = . + 0x8000;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        . = ALIGN(8);
    } > ram

    .lit8 : { *(.lit8) . = ALIGN(8); } > ram
    .lit4 : { *(.lit4) . = ALIGN(8); } > ram

    .sbss : {
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        . = ALIGN(8);
    } > ram

    .bss : {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
    } > ram

    _sheap = .;
    . = ORIGIN(ram) + LENGTH(ram) - __stack_size - __exception_stack_size;
    _eheap = .;

    . += __stack_size;
    _sp = .;

    . += __exception_stack_size;
    _esp = .;

    /* Double-buffered framebuffers */
    .framebuffer0 (NOLOAD) : SUBALIGN(64) {
        __framebuffer_0 = .;
        . += 320 * 240 * 2;
    } > framebuf0

    .framebuffer1 (NOLOAD) : SUBALIGN(64) {
        __framebuffer_1 = .;
        . += 320 * 240 * 2;
    } > framebuf1

    .zbuffer (NOLOAD) : SUBALIGN(64) {
        __zbuffer = .;
        . += 320 * 240 * 2;
    } > zbuffer

    /DISCARD/ : {
        *(.MIPS.*)
    }
}
