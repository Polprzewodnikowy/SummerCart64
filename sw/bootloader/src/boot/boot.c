#include "boot.h"
#include "crc32.h"
#include "n64_regs.h"


static const struct crc32_to_cic_seed {
    uint32_t ipl3_crc32;
    uint16_t cic_seed;
} crc32_to_cic_seed[] = {
    { .ipl3_crc32 = 0x587BD543, .cic_seed = 0x00AC },   // CIC5101
    { .ipl3_crc32 = 0x6170A4A1, .cic_seed = 0x013F },   // CIC6101
    { .ipl3_crc32 = 0x009E9EA3, .cic_seed = 0x013F },   // CIC7102
    { .ipl3_crc32 = 0x90BB6CB5, .cic_seed = 0x003F },   // CICx102
    { .ipl3_crc32 = 0x0B050EE0, .cic_seed = 0x0078 },   // CICx103
    { .ipl3_crc32 = 0x98BC2C86, .cic_seed = 0x0091 },   // CICx105
    { .ipl3_crc32 = 0xACC8580A, .cic_seed = 0x0085 },   // CICx106
    { .ipl3_crc32 = 0x10C68B18, .cic_seed = 0x00DD },   // JP 64DD dev
    { .ipl3_crc32 = 0x0E018159, .cic_seed = 0x00DD },   // JP 64DD retail
    { .ipl3_crc32 = 0x8FEBA21E, .cic_seed = 0x00DE },   // US 64DD retail
};


static cart_header_t global_cart_header __attribute__((aligned(16)));


cart_header_t *boot_load_cart_header(bool ddipl) {
    cart_header_t *cart_header_pointer = &global_cart_header;

    platform_pi_dma_read(cart_header_pointer, CART_BASE, sizeof(cart_header_t));
    platform_cache_invalidate(cart_header_pointer, sizeof(cart_header_t));

    return cart_header_pointer;
}

uint16_t boot_get_cic_seed(cart_header_t *cart_header) {
    uint16_t cic_seed = crc32_to_cic_seed[3].cic_seed;
    uint32_t ipl3_crc32 = crc32_calculate(cart_header->boot_code, sizeof(cart_header->boot_code));

    for (size_t i = 0; i < ARRAY_ITEMS(crc32_to_cic_seed); i++) {
        if (crc32_to_cic_seed[i].ipl3_crc32 == ipl3_crc32) {
            cic_seed = crc32_to_cic_seed[i].cic_seed;
        }
    }

    return cic_seed;
}

tv_type_t boot_get_tv_type(cart_header_t *cart_header) {
    switch (cart_header->country_code) {
        case 'D':
        case 'F':
        case 'H':
        case 'I':
        case 'P':
        case 'S':
        case 'W':
        case 'X':
        case 'Y':
            return TV_PAL;
        case '7':
        case 'A':
        case 'C':
        case 'E':
        case 'J':
        case 'K':
        case 'N':
        case 'U':
            return TV_NTSC;
        case 'B':
            return TV_MPAL;
        default:
            return -1;
    }
}

void boot(cart_header_t *cart_header, uint16_t cic_seed, tv_type_t tv_type, uint32_t ddipl_override) {
    uint32_t is_x105_boot = (cic_seed == crc32_to_cic_seed[5].cic_seed);
    uint32_t is_ddipl_boot = (
        ddipl_override || 
        (cic_seed == crc32_to_cic_seed[7].cic_seed) ||
        (cic_seed == crc32_to_cic_seed[8].cic_seed) ||
        (cic_seed == crc32_to_cic_seed[9].cic_seed)
    );
    tv_type_t os_tv_type = tv_type == -1 ? OS_BOOT_CONFIG->tv_type : tv_type;

    volatile uint64_t gpr_regs[32];

    MI->interrupt_mask = MI_INT_CLEAR_SP | MI_INT_CLEAR_SI | MI_INT_CLEAR_AI | MI_INT_CLEAR_VI | MI_INT_CLEAR_PI | MI_INT_CLEAR_DP;

    while (!(SP->status & SP_STATUS_HALT));

    SP->status = SP_STATUS_CLEAR_INTERRUPT | SP_STATUS_SET_HALT;

    while (SP->dma_busy & SP_DMA_BUSY);

    PI->status = PI_STATUS_CLEAR_INTERRUPT | PI_STATUS_RESET_CONTROLLER;
    VI->v_intr = 0x3FF;
    VI->h_limits = 0;
    VI->current_line = 0;
    AI->dram_addr = 0;
    AI->len = 0;

    PI->dom1_lat = cart_header->pi_conf & 0xFF;
    PI->dom1_pwd = cart_header->pi_conf >> 8;
    PI->dom1_pgs = cart_header->pi_conf >> 16;
    PI->dom1_rls = cart_header->pi_conf >> 20;

    if (DP_CMD->status & DP_CMD_STATUS_XBUS_DMEM_DMA) {
        while (DP_CMD->status & DP_CMD_STATUS_PIPE_BUSY);
    }

    for (size_t i = 0; i < ARRAY_ITEMS(cart_header->boot_code); i++) {
        SP_MEM->dmem[16 + i] = cart_header->boot_code[i];
    }

    // CIC X105 based games checks if start of IPL2 code exists in SP IMEM

    SP_MEM->imem[0] = 0x3C0DBFC0;   // lui   t5, 0xBFC0
    SP_MEM->imem[1] = 0x8DA807FC;   // lw    t0, 0x07FC(t5)
    SP_MEM->imem[2] = 0x25AD07C0;   // addiu t5, t5, 0x07C0
    SP_MEM->imem[3] = 0x31080080;   // andi  t0, t0, 0x0080
    SP_MEM->imem[4] = 0x5500FFFC;   // bnel  t0, zero, &SP_MEM->imem[1]
    SP_MEM->imem[5] = 0x3C0DBFC0;   // lui   t5, 0xBFC0
    SP_MEM->imem[6] = 0x8DA80024;   // lw    t0, 0x0024(t5)
    SP_MEM->imem[7] = 0x3C0BB000;   // lui   t3, 0xB000

    if (is_x105_boot) {
        OS_BOOT_CONFIG->mem_size_6105 = OS_BOOT_CONFIG->mem_size;
    }

    for (size_t i = 0; i < ARRAY_ITEMS(gpr_regs); i++) {
        gpr_regs[i] = 0;
    }

    gpr_regs[CPU_REG_T3] = CPU_ADDRESS_IN_REG(SP_MEM->dmem[16]);
    gpr_regs[CPU_REG_S3] = is_ddipl_boot ? OS_BOOT_ROM_TYPE_DD : OS_BOOT_ROM_TYPE_GAME_PAK;
    gpr_regs[CPU_REG_S4] = os_tv_type;
    gpr_regs[CPU_REG_S5] = OS_BOOT_RESET_TYPE_COLD;
    gpr_regs[CPU_REG_S6] = BOOT_SEED_IPL3(cic_seed);
    gpr_regs[CPU_REG_S7] = BOOT_SEED_OS_VERSION(cic_seed);
    gpr_regs[CPU_REG_SP] = CPU_ADDRESS_IN_REG(SP_MEM->imem[ARRAY_ITEMS(SP_MEM->imem) - 4]);
    gpr_regs[CPU_REG_RA] = CPU_ADDRESS_IN_REG(SP_MEM->imem[(os_tv_type == TV_PAL) ? 341 : 340]);

    __asm__ (
        ".set noat \n\t"
        ".set noreorder \n\t"
        "li $t0, 0x34000000 \n\t"
        "mtc0 $t0, $12 \n\t"
        "nop \n\t"
        "li $t0, 0x0006E463 \n\t"
        "mtc0 $t0, $16 \n\t"
        "nop \n\t"
        "li $t0, 0x00005000 \n\t"
        "mtc0 $t0, $9 \n\t"
        "nop \n\t"
        "li $t0, 0x0000005C \n\t"
        "mtc0 $t0, $13 \n\t"
        "nop \n\t"
        "li $t0, 0x007FFFF0 \n\t"
        "mtc0 $t0, $4 \n\t"
        "nop \n\t"
        "li $t0, 0xFFFFFFFF \n\t"
        "mtc0 $t0, $14 \n\t"
        "nop \n\t"
        "mtc0 $t0, $30 \n\t"
        "nop \n\t"
        "lui $t0, 0x0000 \n\t"
        "mthi $t0 \n\t"
        "nop \n\t"
        "mtlo $t0 \n\t"
        "nop \n\t"
        "ctc1 $t0, $31 \n\t"
        "nop \n\t"
        "add $ra, $zero, %[gpr_regs] \n\t"
        "ld $at, 0x08($ra) \n\t"
        "ld $v0, 0x10($ra) \n\t"
        "ld $v1, 0x18($ra) \n\t"
        "ld $a0, 0x20($ra) \n\t"
        "ld $a1, 0x28($ra) \n\t"
        "ld $a2, 0x30($ra) \n\t"
        "ld $a3, 0x38($ra) \n\t"
        "ld $t0, 0x40($ra) \n\t"
        "ld $t1, 0x48($ra) \n\t"
        "ld $t2, 0x50($ra) \n\t"
        "ld $t3, 0x58($ra) \n\t"
        "ld $t4, 0x60($ra) \n\t"
        "ld $t5, 0x68($ra) \n\t"
        "ld $t6, 0x70($ra) \n\t"
        "ld $t7, 0x78($ra) \n\t"
        "ld $s0, 0x80($ra) \n\t"
        "ld $s1, 0x88($ra) \n\t"
        "ld $s2, 0x90($ra) \n\t"
        "ld $s3, 0x98($ra) \n\t"
        "ld $s4, 0xA0($ra) \n\t"
        "ld $s5, 0xA8($ra) \n\t"
        "ld $s6, 0xB0($ra) \n\t"
        "ld $s7, 0xB8($ra) \n\t"
        "ld $t8, 0xC0($ra) \n\t"
        "ld $t9, 0xC8($ra) \n\t"
        "ld $k0, 0xD0($ra) \n\t"
        "ld $k1, 0xD8($ra) \n\t"
        "ld $gp, 0xE0($ra) \n\t"
        "ld $sp, 0xE8($ra) \n\t"
        "ld $fp, 0xF0($ra) \n\t"
        "ld $ra, 0xF8($ra) \n\t"
        "jr $t3 \n\t"
        "nop"
        :
        : [gpr_regs] "r" (gpr_regs)
        : "t0", "ra"
    );

    while (1);
}
