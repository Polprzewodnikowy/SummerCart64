#include "boot.h"
#include "crc32.h"
#include "io.h"
#include "vr4300.h"


extern uint32_t reboot_start __attribute__((section(".text")));
extern size_t reboot_size __attribute__((section(".text")));


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


static io32_t *boot_get_device_base (boot_params_t *params) {
    io32_t *device_base_address = ROM_CART;
    if (params->device_type == BOOT_DEVICE_TYPE_64DD) {
        device_base_address = ROM_DDIPL;
    }
    return device_base_address;
}

static bool boot_detect_cic_seed (boot_params_t *params) {
    io32_t *base = boot_get_device_base(params);

    uint32_t ipl3[1008] __attribute__((aligned(8)));

    pi_dma_read(&base[16], ipl3, sizeof(ipl3));

    uint32_t crc32 = crc32_calculate(ipl3, sizeof(ipl3));

    for (int i = 0; i < sizeof(ipl3_crc32) / sizeof(ipl3_crc32_t); i++) {
        if (ipl3_crc32[i].crc32 == crc32) {
            params->cic_seed = ipl3_crc32[i].seed;
            return true;
        }
    }

    return false;
}

void boot (boot_params_t *params) {
    if (params->tv_type == BOOT_TV_TYPE_PASSTHROUGH) {
        params->tv_type = OS_INFO->tv_type;
    }

    if (params->detect_cic_seed) {
        if (!boot_detect_cic_seed(params)) {
            params->cic_seed = 0x3F;
        }
    }

    OS_INFO->mem_size_6105 = OS_INFO->mem_size;

    asm volatile (
        "li $t1, %[status] \n"
        "mtc0 $t1, $12 \n" ::
        [status] "i" (C0_SR_CU1 | C0_SR_CU0 | C0_SR_FR)
    );

    while (!(cpu_io_read(&SP->SR) & SP_SR_HALT));

    cpu_io_write(&SP->SR,
        SP_SR_CLR_SIG7 |
        SP_SR_CLR_SIG6 |
        SP_SR_CLR_SIG5 |
        SP_SR_CLR_SIG4 |
        SP_SR_CLR_SIG3 |
        SP_SR_CLR_SIG2 |
        SP_SR_CLR_SIG1 |
        SP_SR_CLR_SIG0 |
        SP_SR_CLR_INTR_BREAK |
        SP_SR_CLR_SSTEP |
        SP_SR_CLR_INTR |
        SP_SR_CLR_BROKE |
        SP_SR_SET_HALT
    );
    cpu_io_write(&SP->SEMAPHORE, 0);
    cpu_io_write(&SP->PC, 0);

    while (cpu_io_read(&SP->DMA_BUSY));

    cpu_io_write(&PI->SR, PI_SR_CLR_INTR | PI_SR_RESET);
    while ((cpu_io_read(&VI->CURR_LINE) & ~(VI_CURR_LINE_FIELD)) != 0);
    cpu_io_write(&VI->V_INTR, 0x3FF);
    cpu_io_write(&VI->H_LIMITS, 0);
    cpu_io_write(&VI->CURR_LINE, 0);
    cpu_io_write(&AI->MADDR, 0);
    cpu_io_write(&AI->LEN, 0);

    while (cpu_io_read(&SP->SR) & SP_SR_DMA_BUSY);

    uint32_t *reboot_src = &reboot_start;
    io32_t *reboot_dst = SP_MEM->IMEM;
    size_t reboot_instructions = (size_t) (&reboot_size) / sizeof(uint32_t);

    for (int i = 0; i < reboot_instructions; i++) {
        cpu_io_write(&reboot_dst[i], reboot_src[i]);
    }

    cpu_io_write(&PI->DOM[0].LAT, 0xFF);
    cpu_io_write(&PI->DOM[0].PWD, 0xFF);
    cpu_io_write(&PI->DOM[0].PGS, 0x0F);
    cpu_io_write(&PI->DOM[0].RLS, 0x03);

    io32_t *base = boot_get_device_base(params);
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

    register uint32_t boot_device asm ("s3");
    register uint32_t tv_type asm ("s4");
    register uint32_t reset_type asm ("s5");
    register uint32_t cic_seed asm ("s6");
    register uint32_t version asm ("s7");

    boot_device = (params->device_type & 0x01);
    tv_type = (params->tv_type & 0x03);
    reset_type = (params->reset_type & 0x01);
    cic_seed = (params->cic_seed & 0xFF);
    version = (params->tv_type == BOOT_TV_TYPE_PAL) ? 6
            : (params->tv_type == BOOT_TV_TYPE_NTSC) ? 1
            : (params->tv_type == BOOT_TV_TYPE_MPAL) ? 4
            : 0;

    asm volatile (
        "la $t3, reboot \n"
        "jr $t3 \n" ::
        [boot_device] "r" (boot_device),
        [tv_type] "r" (tv_type),
        [reset_type] "r" (reset_type),
        [cic_seed] "r" (cic_seed),
        [version] "r" (version) :
        "t3"
    );

    while (1);
}
