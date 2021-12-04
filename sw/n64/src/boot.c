#include "boot.h"
#include "crc32.h"
#include "sc64.h"
#include "sys.h"


extern uint32_t ipl2 __attribute__((section(".data")));


typedef struct {
    const uint32_t crc32;
    const uint8_t seed;
    const uint8_t version;
} ipl3_crc32_t;

static const ipl3_crc32_t ipl3_crc32[] = {
    { .crc32 = 0x587BD543, .seed = 0xAC, .version = 0 },   // CIC5101
    { .crc32 = 0x6170A4A1, .seed = 0x3F, .version = 1 },   // CIC6101
    { .crc32 = 0x009E9EA3, .seed = 0x3F, .version = 1 },   // CIC7102
    { .crc32 = 0x90BB6CB5, .seed = 0x3F, .version = 0 },   // CICx102
    { .crc32 = 0x0B050EE0, .seed = 0x78, .version = 0 },   // CICx103
    { .crc32 = 0x98BC2C86, .seed = 0x91, .version = 0 },   // CICx105
    { .crc32 = 0xACC8580A, .seed = 0x85, .version = 0 },   // CICx106
    { .crc32 = 0x0C965795, .seed = 0xDD, .version = 0 },   // JP 64DD
    { .crc32 = 0x10C68B18, .seed = 0xDD, .version = 0 },   // JP 64DD dev
    { .crc32 = 0x0E018159, .seed = 0xDD, .version = 0 },   // JP 64DD retail
    { .crc32 = 0x8FEBA21E, .seed = 0xDE, .version = 0 },   // US 64DD retail
};


bool boot_get_tv_type (boot_info_t *info) {
    io32_t *device_base_address = ROM_CART;
    if (info->device_type == BOOT_DEVICE_TYPE_DD) {
        device_base_address = ROM_DDIPL;
    }

    char region = ((pi_io_read(&device_base_address[15]) >> 8) & 0xFF);

    switch (region) {
        case 'D':
        case 'F':
        case 'H':
        case 'I':
        case 'P':
        case 'S':
        case 'W':
        case 'X':
        case 'Y':
            info->tv_type = BOOT_TV_TYPE_PAL;
            break;

        case '7':
        case 'A':
        case 'C':
        case 'E':
        case 'J':
        case 'K':
        case 'N':
        case 'U':
            info->tv_type = BOOT_TV_TYPE_NTSC;
            break;

        case 'B':
            info->tv_type = BOOT_TV_TYPE_MPAL;
            break;

        default:
            LOG_E("Unknown region: [0x%02X]\r\n", region);
            return false;
    }

    return true;
}

bool boot_get_cic_seed_version (boot_info_t *info) {
    io32_t *device_base_address = ROM_CART;
    if (info->device_type == BOOT_DEVICE_TYPE_DD) {
        device_base_address = ROM_DDIPL;
    }

    uint32_t ipl3[1008];

    io32_t *ipl3_src = &device_base_address[16];
    uint32_t *ipl3_dst = ipl3;

    for (int i = 0; i < sizeof(ipl3); i += sizeof(uint32_t)) {
        *ipl3_dst++ = pi_io_read(ipl3_src++);
    }

    uint32_t crc32 = crc32_calculate(ipl3, sizeof(ipl3));

    for (int i = 0; i < sizeof(ipl3_crc32) / sizeof(ipl3_crc32_t); i++) {
        if (ipl3_crc32[i].crc32 == crc32) {
            info->cic_seed = ipl3_crc32[i].seed;
            info->version = ipl3_crc32[i].version;
            return true;
        }
    }

    LOG_E("Unknown IPL3 CRC32: [0x%08lX]\r\n", crc32);

    return false;
}

void boot (boot_info_t *info) {
    c0_set_status(C0_SR_CU1 | C0_SR_CU0 | C0_SR_FR);

    OS_INFO->mem_size_6105 = OS_INFO->mem_size;

    while (!(io_read(&SP->SR) & SP_SR_HALT));

    io_write(&SP->SR, SP_SR_CLR_INTR | SP_SR_SET_HALT);

    while (io_read(&SP->DMA_BUSY));

    io_write(&PI->SR, PI_SR_CLR_INTR | PI_SR_RESET);
    io_write(&VI->V_INTR, 0x3FF);
    io_write(&VI->H_LIMITS, 0);
    io_write(&VI->CURR_LINE, 0);
    io_write(&AI->MADDR, 0);
    io_write(&AI->LEN, 0);

    io32_t *device_base_address = ROM_CART;
    if (info->device_type == BOOT_DEVICE_TYPE_DD) {
        device_base_address = ROM_DDIPL;
    }

    uint32_t pi_config = pi_io_read(device_base_address);

    io_write(&PI->DOM[0].LAT, pi_config & 0xFF);
    io_write(&PI->DOM[0].PWD, pi_config >> 8);
    io_write(&PI->DOM[0].PGS, pi_config >> 16);
    io_write(&PI->DOM[0].RLS, pi_config >> 20);

    if (io_read(&DPC->SR) & DPC_SR_XBUS_DMEM_DMA) {
        while (io_read(&DPC->SR) & DPC_SR_PIPE_BUSY);
    }

    uint32_t *ipl2_src = &ipl2;
    io32_t *ipl2_dst = SP_MEM->IMEM;

    for (int i = 0; i < 8; i++) {
        io_write(&ipl2_dst[i], ipl2_src[i]);
    }

    io32_t *ipl3_src = device_base_address;
    io32_t *ipl3_dst = SP_MEM->DMEM;
    uint32_t tmp;

    for (int i = 16; i < 1024; i++) {
        tmp = pi_io_read(&ipl3_src[i]);
        io_write(&ipl3_dst[i], tmp);
    }

    uint32_t boot_device = (info->device_type & 0x01);
    uint32_t tv_type = (info->tv_type & 0x03);
    uint32_t reset_type = (info->reset_type & 0x01);
    uint32_t cic_seed = (info->cic_seed & 0xFF);
    uint32_t version = (info->version & 0x01);
    void (*entry_point)(void) = (void (*)(void)) UNCACHED(&SP_MEM->DMEM[16]);
    void *stack_pointer = (void *) UNCACHED(&SP_MEM->IMEM[1020]);

    __asm__ volatile (
        ".set noat \n"
        ".set noreorder \n"
        "lw $t3, %[entry_point] \n"
        "lw $s3, %[boot_device] \n"
        "lw $s4, %[tv_type] \n"
        "lw $s5, %[reset_type] \n"
        "lw $s6, %[cic_seed] \n"
        "lw $s7, %[version] \n"
        "lw $sp, %[stack_pointer] \n"
        "jr $t3 \n"
        "nop \n"
        :
        : [entry_point] "R" (entry_point),
          [boot_device] "R" (boot_device),
          [tv_type] "R" (tv_type),
          [reset_type] "R" (reset_type),
          [cic_seed] "R" (cic_seed),
          [version] "R" (version),
          [stack_pointer] "R" (stack_pointer)
        : "t3", "s3", "s4", "s5", "s6", "s7"
    );

    while (1);
}
