#include "boot.h"
#include "crc32.h"
#include "io.h"
#include "vr4300.h"


extern uint32_t ipl2 __attribute__((section(".data")));


typedef struct {
    const uint32_t crc32;
    const uint8_t seed;
} ipl3_crc32_t;

static const ipl3_crc32_t ipl3_crc32[] = {
    { .crc32 = 0x587BD543, .seed = 0xAC },  // 5101
    { .crc32 = 0x6170A4A1, .seed = 0x3F },  // 6101
    { .crc32 = 0x009E9EA3, .seed = 0x3F },  // 7102
    { .crc32 = 0x90BB6CB5, .seed = 0x3F },  // 6102/7101
    { .crc32 = 0x0B050EE0, .seed = 0x78 },  // x103
    { .crc32 = 0x98BC2C86, .seed = 0x91 },  // x105
    { .crc32 = 0xACC8580A, .seed = 0x85 },  // x106
    { .crc32 = 0x0E018159, .seed = 0xDD },  // 5167
    { .crc32 = 0x10C68B18, .seed = 0xDD },  // NDXJ0
    { .crc32 = 0xBC605D0A, .seed = 0xDD },  // NDDJ0
    { .crc32 = 0x502C4466, .seed = 0xDD },  // NDDJ1
    { .crc32 = 0x0C965795, .seed = 0xDD },  // NDDJ2
    { .crc32 = 0x8FEBA21E, .seed = 0xDE },  // NDDE0
};


static io32_t *boot_get_device_base (boot_info_t *info) {
    io32_t *device_base_address = ROM_CART;
    if (info->device_type == BOOT_DEVICE_TYPE_DD) {
        device_base_address = ROM_DDIPL;
    }
    return device_base_address;
}

static bool boot_get_cic_seed (boot_info_t *info) {
    io32_t *base = boot_get_device_base(info);

    uint32_t ipl3[1008] __attribute__((aligned(8)));

    pi_dma_read(&base[16], ipl3, sizeof(ipl3));

    uint32_t crc32 = crc32_calculate(ipl3, sizeof(ipl3));

    for (int i = 0; i < sizeof(ipl3_crc32) / sizeof(ipl3_crc32_t); i++) {
        if (ipl3_crc32[i].crc32 == crc32) {
            info->cic_seed = ipl3_crc32[i].seed;
            return true;
        }
    }

    return false;
}

void boot (boot_info_t *info, bool detect_cic_seed) {
    if (info->tv_type == BOOT_TV_TYPE_PASSTHROUGH) {
        info->tv_type = OS_INFO->tv_type;
    }

    if (detect_cic_seed) {
        if (!boot_get_cic_seed(info)) {
            info->cic_seed = 0x3F;
        }
    }

    asm volatile (
        "li $t1, %[status] \n"
        "mtc0 $t1, $12 \n" ::
        [status] "i" (C0_SR_CU1 | C0_SR_CU0 | C0_SR_FR)
    );

    OS_INFO->mem_size_6105 = OS_INFO->mem_size;

    while (!(cpu_io_read(&SP->SR) & SP_SR_HALT));

    cpu_io_write(&SP->SR, SP_SR_CLR_INTR | SP_SR_SET_HALT);

    while (cpu_io_read(&SP->DMA_BUSY));

    cpu_io_write(&PI->SR, PI_SR_CLR_INTR | PI_SR_RESET);
    cpu_io_write(&VI->V_INTR, 0x3FF);
    cpu_io_write(&VI->H_LIMITS, 0);
    cpu_io_write(&VI->CURR_LINE, 0);
    cpu_io_write(&AI->MADDR, 0);
    cpu_io_write(&AI->LEN, 0);

    while (cpu_io_read(&SP->SR) & SP_SR_DMA_BUSY);

    uint32_t *ipl2_src = &ipl2;
    io32_t *ipl2_dst = SP_MEM->IMEM;

    for (int i = 0; i < 8; i++) {
        cpu_io_write(&ipl2_dst[i], ipl2_src[i]);
    }

    cpu_io_write(&PI->DOM[0].LAT, 0xFF);
    cpu_io_write(&PI->DOM[0].PWD, 0xFF);
    cpu_io_write(&PI->DOM[0].PGS, 0x0F);
    cpu_io_write(&PI->DOM[0].RLS, 0x03);

    io32_t *base = boot_get_device_base(info);
    uint32_t pi_config = pi_io_read(base);

    cpu_io_write(&PI->DOM[0].LAT, pi_config & 0xFF);
    cpu_io_write(&PI->DOM[0].PWD, pi_config >> 8);
    cpu_io_write(&PI->DOM[0].PGS, pi_config >> 16);
    cpu_io_write(&PI->DOM[0].RLS, pi_config >> 20);

    if (cpu_io_read(&DPC->SR) & DPC_SR_XBUS_DMEM_DMA) {
        while (cpu_io_read(&DPC->SR) & DPC_SR_PIPE_BUSY);
    }

    io32_t *ipl3_src = base;
    io32_t *ipl3_dst = SP_MEM->DMEM;

    for (int i = 16; i < 1024; i++) {
        cpu_io_write(&ipl3_dst[i], pi_io_read(&ipl3_src[i]));
    }

    register void (*entry_point)(void) asm ("t3");
    register uint32_t boot_device asm ("s3");
    register uint32_t tv_type asm ("s4");
    register uint32_t reset_type asm ("s5");
    register uint32_t cic_seed asm ("s6");
    register uint32_t version asm ("s7");
    void *stack_pointer;

    entry_point = (void (*)(void)) UNCACHED(&SP_MEM->DMEM[16]);
    boot_device = (info->device_type & 0x01);
    tv_type = (info->tv_type & 0x03);
    reset_type = (info->reset_type & 0x01);
    cic_seed = (info->cic_seed & 0xFF);
    version = (info->tv_type == BOOT_TV_TYPE_PAL) ? 6 : 1;
    stack_pointer = (void *) UNCACHED(&SP_MEM->IMEM[1020]);

    asm volatile (
        "move $sp, %[stack_pointer] \n"
        "jr %[entry_point] \n" ::
        [entry_point] "r" (entry_point),
        [boot_device] "r" (boot_device),
        [tv_type] "r" (tv_type),
        [reset_type] "r" (reset_type),
        [cic_seed] "r" (cic_seed),
        [version] "r" (version),
        [stack_pointer] "r" (stack_pointer)
    );

    while (1);
}
