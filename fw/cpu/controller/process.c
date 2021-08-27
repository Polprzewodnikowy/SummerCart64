#include "sys.h"
#include "process.h"

static const uint8_t cmd_token[3] = { 'C', 'M', 'D' };
static const uint8_t cmp_token[3] = { 'C', 'M', 'P' };
static const uint8_t err_token[3] = { 'E', 'R', 'R' };

static uint8_t save_type = 0;
static uint16_t cic_type = 0xFFFF;
static uint8_t tv_type = 0xFF;

void process_usb (void);
void process_cfg (void);
void process_dd (void);
void process_si (void);
void process_uart (void);
void cfg_set_save_type (uint8_t type);
void cfg_update_config (uint32_t *args);

void process (void) {
    while (1) {
        process_usb();
        process_cfg();
        process_dd();
        process_si();
        process_uart();
    }
}

void process_usb (void) {
    static int state = 0;
    static uint8_t current_byte = 0;
    static uint8_t cmd;
    static uint32_t args[2];
    static uint8_t is_error;
    static uint8_t dma_started;

    uint8_t data;

    switch (state) {
        case 0: {
            if (USB->SCR & USB_SCR_RXNE) {
                data = USB->DR;
                if (current_byte == 3) {
                    state = 1;
                    current_byte = 0;
                    cmd = data;
                    args[0] = 0;
                    args[1] = 0;
                    is_error = 0;
                    dma_started = 0;
                    break;
                }
                if (data != cmd_token[current_byte]) {
                    current_byte = 0;
                    break;
                }
                current_byte += 1;
            }
            break;
        }

        case 1: {
            if (USB->SCR & USB_SCR_RXNE) {
                data = USB->DR;
                uint32_t *p = args + (current_byte >= 4 ? 1 : 0);
                *p = (*p << 8) | data;
                current_byte += 1;
                if (current_byte == 8) {
                    state = 2;
                    current_byte = 0;
                    break;
                }
            }
            break;
        }

        case 2: {
            if (cmd == 'R' || cmd == 'W') {
                if (!dma_started) {
                    if (!(DMA->SCR & DMA_SCR_BUSY)) {
                        DMA->MADDR = args[0];
                        DMA->ID_LEN = args[1];
                        DMA->SCR = (cmd == 'W' ? DMA_SCR_DIR : 0) | DMA_SCR_START;
                        dma_started = 1;
                    }
                } else {
                    if (!(DMA->SCR & DMA_SCR_BUSY)) {
                        state = 3;
                    }
                }
            } else if (cmd == 'C') {
                cfg_update_config(args);
                state = 3;
            } else {
                state = 3;
                is_error = 1;
            }
            break;
        }

        case 3: {
            if (USB->SCR & USB_SCR_TXE) {
                const uint8_t *p = is_error ? err_token : cmp_token;
                USB->DR = (current_byte < 3) ? p[current_byte] : cmd;
                current_byte += 1;
                if (current_byte == 4) {
                    state = 0;
                    current_byte = 0;
                }
            }
            break;
        }

        default: {
            state = 0;
            break;
        }
    }
}

void process_cfg (void) {
    static uint8_t state = 0;
    static uint8_t cmd;
    static uint32_t args[3];

    switch (state) {
        case 0: {
            if (CFG->SCR & CFG_SCR_CPU_BUSY) {
                state = 1;
                cmd = CFG->CMD;
                args[0] = CFG->DATA[0];
                args[1] = CFG->DATA[1];
            }
            break;
        }

        case 1: {
            if (cmd == 'C') {
                cfg_update_config(args);
                CFG->DATA[0] = (save_type << 8) | (CFG->SCR & 0x07);
                CFG->DATA[1] = (tv_type << 16) | cic_type;
                state = 2;
            } else {
                CFG->DATA[0] = 0xFFFFFFFF;
                CFG->DATA[1] = 0xFFFFFFFF;
                state = 2;
            }
            break;
        }

        case 2: {
            CFG->SCR &= ~(CFG_SCR_CPU_BUSY);
            state = 0;
            break;
        }

        default: {
            state = 0;
            break;
        }
    }
}

void process_dd (void) {

}

void process_si (void) {

}

void process_uart (void) {
    if (UART->SCR & UART_SCR_RXNE) {
        uint8_t data = UART->DR;
        if (data == '/') {
            while (!(UART->SCR & UART_SCR_TXE));
            UART->DR = '>';
            void (*bootloader) (void) = (void *) &BOOTLOADER;
            bootloader();
        }
    }
}

void cfg_set_save_type (uint8_t type) {
    CFG->SCR &= ~(CFG_SCR_FLASHRAM_EN | CFG_SCR_SRAM_EN);

    switch (type) {
        case 0: {
            break;
        }
        case 1: {
            CFG->SAVE_OFFSET = SDRAM_SIZE - 512;
            break;
        }
        case 2: {
            CFG->SAVE_OFFSET = SDRAM_SIZE - 2048;
            break;
        }
        case 3: {
            CFG->SAVE_OFFSET = SDRAM_SIZE - (32 * 1024);
            CFG->SCR |= CFG_SCR_SRAM_EN;
            break;
        }
        case 4: {
            CFG->SAVE_OFFSET = SDRAM_SIZE - (256 * 1024);
            CFG->SCR |= CFG_SCR_FLASHRAM_EN;
            break;
        }
        case 5: {
            CFG->SAVE_OFFSET = SDRAM_SIZE - (3 * 32 * 1024);
            CFG->SCR |= CFG_SCR_SRAM_EN;
            break;
        }
        case 6: {
            CFG->SAVE_OFFSET = 0x01618000;
            CFG->SCR |= CFG_SCR_FLASHRAM_EN;
            break;
        }
        default: {
            return;
        }
    }

    save_type = type;
}

void cfg_update_config (uint32_t *args) {
    switch (args[0]) {
        case 0: {
            if (args[1]) {
                CFG->SCR |= CFG_SCR_SDRAM_SWITCH;
            } else {
                CFG->SCR &= ~CFG_SCR_SDRAM_SWITCH;
            }
            break;
        }
        case 1: {
            if (args[1]) {
                CFG->SCR |= CFG_SCR_SDRAM_WRITABLE;
            } else {
                CFG->SCR &= ~CFG_SCR_SDRAM_WRITABLE;
            }
            break;
        }
        case 2: {
            if (args[1]) {
                CFG->SCR |= CFG_SCR_DD_EN;
            } else {
                CFG->SCR &= ~CFG_SCR_DD_EN;
            }
            break;
        }
        case 3: {
            cfg_set_save_type(args[1] & 0xFF);
            break;
        }
        case 4: {
            cic_type = args[1] & 0xFFFF;
            break;
        }
        case 5: {
            tv_type = args[1] & 0xFF;
            break;
        }
    }
}
