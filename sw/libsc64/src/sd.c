#include "helpers.h"
#include "registers.h"
#include "sd.h"


void sc64_sd_access(bool is_enabled) {
    sc64_helpers_reg_write(&SC64_CART->SCR, SC64_CART_SCR_SD_ENABLE, is_enabled);
}


// #include "registers.h"
// #include "sd.h"
// #include "io_dma.h"


// #define CMD8_ARG_SUPPLY_VOLTAGE_27_36_V (1 << 8)
// #define CMD8_ARG_CHECK_PATTERN_AA       (0xAA << 0)

// #define ACMD41_ARG_HCS                  (1 << 30)

// #define R3_CCS                          (1 << 30)
// #define R3_BUSY                         (1 << 31)

// #define R7_SUPPLY_VOLTAGE_27_36_V       (1 << 8)
// #define R7_CHECK_PATTERN_AA             (0xAA << 0)

// #define SD_BLOCK_SIZE                   (512)


// typedef enum sc64_sd_cmd_flags_e {
//     NO_FLAGS        = 0,
//     ACMD            = (1 << 0),
//     SKIP_RESPONSE   = (1 << 1),
//     LONG_RESPONSE   = (1 << 2),
//     IGNORE_CRC      = (1 << 3),
//     IGNORE_INDEX    = (1 << 4),
// } sc64_sd_cmd_flags_t;


// static bool sd_card_initialized = false;
// static bool sd_card_type_block = false;
// static bool sd_card_selected = false;
// static uint32_t sd_card_rca = 0;
// static uint8_t sd_buffer[64] __attribute__((aligned(16)));


// static void sc64_sd_clock_set(uint32_t clock) {
//     uint32_t scr = sc64_io_read(&SC64_SD->SCR);

//     sc64_io_write(&SC64_SD->SCR, (scr & (~SC64_SD_SCR_CLK_MASK)) | (clock & SC64_SD_SCR_CLK_MASK));
// }

// static void sc64_sd_dat_4bit_set(bool is_4bit) {
//     uint32_t scr = sc64_io_read(&SC64_SD->SCR);
    
//     scr &= ~SC64_SD_SCR_DAT_WIDTH;
    
//     if (is_4bit) {
//         scr |= SC64_SD_SCR_DAT_WIDTH;
//     }

//     sc64_io_write(&SC64_SD->SCR, scr);
// }

// static void sc64_sd_peripheral_reset(void) {
//     while (sc64_io_read(&SC64_SD->CMD) & SC64_SD_CMD_BUSY);
//     sc64_io_write(&SC64_SD->DMA_SCR, SC64_SD_DMA_SCR_STOP);
//     sc64_io_write(&SC64_SD->DAT, SC64_SD_DAT_TX_FIFO_FLUSH | SC64_SD_DAT_RX_FIFO_FLUSH | SC64_SD_DAT_STOP);
//     sc64_io_write(&SC64_SD->SCR, 0);
// }

// static void sc64_sd_peripheral_init(void) {
//     sc64_control_sd_enable();
//     sc64_sd_peripheral_reset();
// }

// static void sc64_sd_peripheral_deinit(void) {
//     if (sc64_control_sd_is_enabled()) {
//         sc64_sd_peripheral_reset();
//     }
//     sc64_control_sd_disable();
// }

// static bool sc64_sd_cmd_send(uint8_t index, uint32_t arg, sc64_sd_cmd_flags_t flags, uint32_t *response) {
//     uint32_t reg;

//     if (flags & ACMD) {
//         if (!sc64_sd_cmd_send(55, sd_card_rca, NO_FLAGS, response)) {
//             return false;
//         }
//     }

//     sc64_io_write(&SC64_SD->ARG, arg);

//     reg = SC64_SD_CMD_START | SC64_SD_CMD_INDEX(index);
//     if (flags & SKIP_RESPONSE) {
//         reg |= SC64_SD_CMD_SKIP_RESPONSE;
//     }
//     if (flags & LONG_RESPONSE) {
//         reg |= SC64_SD_CMD_LONG_RESPONSE;
//     }

//     sc64_io_write(&SC64_SD->CMD, reg);

//     do {
//         reg = sc64_io_read(&SC64_SD->CMD);
//     } while (reg & SC64_SD_CMD_BUSY);

//     *response = sc64_io_read(&SC64_SD->RSP);
    
//     return (
//         (reg & SC64_SD_CMD_TIMEOUT) |
//         ((!(flags & SKIP_RESPONSE)) && (
//             ((!(flags & IGNORE_CRC)) && (reg & SC64_SD_CMD_RESPONSE_CRC_ERROR)) |
//             ((!(flags & IGNORE_INDEX)) && (SC64_SD_CMD_INDEX_GET(reg) != index))
//         ))
//     );
// }


// bool sc64_sd_init(void) {
//     bool success;
//     uint32_t response;
//     uint32_t argument;

//     if (sd_card_initialized) {
//         return true;
//     }

//     sc64_sd_peripheral_init();

//     do {
//         sc64_sd_cmd_send(0, 0, SKIP_RESPONSE, &response);

//         argument = CMD8_ARG_SUPPLY_VOLTAGE_27_36_V | CMD8_ARG_CHECK_PATTERN_AA;
//         success = sc64_sd_cmd_send(8, argument, NO_FLAGS, &response);
//         if (success && (response != (R7_SUPPLY_VOLTAGE_27_36_V | R7_CHECK_PATTERN_AA))) {
//             break;
//         }

//         argument = (success ? ACMD41_ARG_HCS : 0) | 0x00FF8000;
//         for (int i = 0; i < 4000; i++) {
//             success = sc64_sd_cmd_send(41, argument, ACMD | IGNORE_CRC | IGNORE_INDEX, &response);
//             if (!success || (response & R3_BUSY)) {
//                 break;
//             }
//         }
//         if (!success || ((response & 0x00FF8000) == 0)) {
//             break;
//         }
//         sd_card_type_block = (response & R3_CCS) ? true : false;

//         success = sc64_sd_cmd_send(2, 0, LONG_RESPONSE | IGNORE_INDEX, &response);
//         if (!success) {
//             break;
//         }

//         success = sc64_sd_cmd_send(3, 0, NO_FLAGS, &response);
//         if (!success) {
//             break;
//         }
//         sd_card_rca = response & 0xFFFF0000;

//         success = sc64_sd_cmd_send(7, sd_card_rca, NO_FLAGS, &response);
//         if (!success) {
//             break;
//         }
//         sd_card_selected = true;

//         success = sc64_sd_cmd_send(6, 2, ACMD, &response);
//         if (!success) {
//             break;
//         }

//         sc64_sd_clock_set(SC64_SD_SCR_CLK_25_MHZ);
//         sc64_sd_dat_4bit_set(true);

//         sc64_sd_dat_prepare(1, 64, DAT_DIR_RX);
//         success = sc64_sd_cmd_send(6, 0x00000001, NO_FLAGS, &response);
//         if (!success) {
//             sc64_sd_dat_abort();
//             break;
//         }
//         success = sc64_sd_dat_read(64, sd_buffer);
//         if (!success) {
//             break;
//         }
//         if (sd_buffer[13] & 0x02) {
//             sc64_sd_dat_prepare(1, 64, DAT_DIR_RX);
//             success = sc64_sd_cmd_send(6, 0x80000001, NO_FLAGS, &response);
//             if (!success) {
//                 sc64_sd_dat_abort();
//                 break;
//             }
//             success = sc64_sd_dat_read(64, sd_buffer);
//             if (!success) {
//                 break;
//             }

//             sc64_sd_clock_set(SC64_SD_SCR_CLK_50_MHZ);
//         }

//         sd_card_initialized = true;
    
//         return true;
//     } while(0);

//     sc64_sd_deinit();

//     return false;
// }
