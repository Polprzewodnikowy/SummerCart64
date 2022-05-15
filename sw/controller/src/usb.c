#include <stdbool.h>
#include <stdint.h>
#include "cfg.h"
#include "fpga.h"
#include "rtc.h"
#include "usb.h"
#include "flash.h"


enum state {
    STATE_IDLE,
    STATE_ARGS,
    STATE_DATA,
    STATE_RESPONSE
};

struct process {
    enum state state;
    uint32_t counter;
    uint8_t cmd;
    uint32_t args[2];
    bool error;
    bool dma_in_progress;
};

static struct process p;


static const char CMD_TOKEN[3] = { 'C', 'M', 'D' };
static const uint32_t CMP_TOKEN = (0x434D5000UL);
static const uint32_t ERR_TOKEN = (0x45525200UL);

static bool usb_rx_byte (uint8_t *data) {
    if (fpga_usb_status_get() & USB_STATUS_RXNE) {
        *data = fpga_usb_pop();
        return true;
    }
    return false;
}

static bool usb_tx_byte (uint8_t data) {
    if (fpga_usb_status_get() & USB_STATUS_TXE) {
        fpga_usb_push(data);
        return true;
    }
    return false;
}

static uint8_t usb_rx_word_counter = 0;
static uint32_t usb_rx_word_buffer = 0;

static bool usb_rx_word (uint32_t *data) {
    uint8_t tmp;
    while (usb_rx_byte(&tmp)) {
        usb_rx_word_buffer = (usb_rx_word_buffer << 8) | tmp;
        usb_rx_word_counter += 1;
        if (usb_rx_word_counter == 4) {
            usb_rx_word_counter = 0;
            *data = usb_rx_word_buffer;
            usb_rx_word_buffer = 0;
            return true;
        }
    }
    return false;
}

static uint8_t usb_tx_word_counter = 0;

static bool usb_tx_word (uint32_t data) {
    while (usb_tx_byte(data >> ((3 - usb_tx_word_counter) * 8))) {
        usb_tx_word_counter += 1;
        if (usb_tx_word_counter == 4) {
            usb_tx_word_counter = 0;
            return true;
        }
    }
    return false;
}

static uint8_t usb_rx_cmd_counter = 0;

static bool usb_rx_cmd (uint8_t *cmd) {
    uint8_t data;
    while (usb_rx_byte(&data)) {
        if (usb_rx_cmd_counter == 3) {
            *cmd = data;
            usb_rx_cmd_counter = 0;
            return true;
        }
        if (data != CMD_TOKEN[usb_rx_cmd_counter++]) {
            usb_rx_cmd_counter = 0;
            return false;
        }
    }
    return false;
}


void usb_init (void) {
    fpga_reg_set(REG_USB_DMA_SCR, DMA_SCR_STOP);
    fpga_reg_set(REG_USB_SCR, USB_SCR_FIFO_FLUSH);

    p.state = STATE_IDLE;

    usb_rx_word_counter = 0;
    usb_rx_word_buffer = 0;
    usb_tx_word_counter = 0;
    usb_rx_cmd_counter = 0;
}

void usb_process (void) {
    uint32_t scr = fpga_reg_get(REG_USB_SCR);

    if (scr & USB_SCR_RESET_PENDING) {
        usb_init();
        fpga_reg_set(REG_USB_SCR, USB_SCR_RESET_ACK);
        return;
    }

    switch (p.state) {
        case STATE_IDLE: {
            if (usb_rx_cmd(&p.cmd)) {
                p.counter = 0;
                p.error = false;
                p.dma_in_progress = false;
                p.state = STATE_ARGS;
            }
            break;
        }

        case STATE_ARGS: {
            if (usb_rx_word(&p.args[p.counter])) {
                p.counter += 1;
                if (p.counter == 2) {
                    p.counter = 0;
                    p.state = STATE_DATA;
                }
            }
            break;
        }

        case STATE_DATA: {
            switch (p.cmd) {
                case 'v':
                    if (usb_tx_word(cfg_get_version())) {
                        p.state = STATE_RESPONSE;
                    }
                    break;

                case 'c':
                    if (p.counter == 0) {
                        cfg_query(p.args);
                        p.counter += 1;
                    }
                    if (usb_tx_word(p.args[1])) {
                        p.state = STATE_RESPONSE;
                    }
                    break;

                case 'C':
                    cfg_update(p.args);
                    p.state = STATE_RESPONSE;
                    break;

                case 't':
                    if (p.counter == 0) {
                        cfg_get_time(p.args);
                        p.counter += 1;
                    }
                    if ((p.counter == 1) && usb_tx_word(p.args[0])) {
                        p.counter += 1;
                    }
                    if ((p.counter == 2) && usb_tx_word(p.args[1])) {
                        p.state = STATE_RESPONSE;
                    }
                    break;

                case 'T':
                    cfg_set_time(p.args);
                    p.state = STATE_RESPONSE;
                    break;

                case 'e':
                    if (p.args[1] == 0) {
                        p.state = STATE_RESPONSE;
                    } else {
                        uint8_t data[8];
                        int length = (p.args[1] > 8) ? 8 : p.args[1];
                        fpga_eeprom_read(p.args[0], length, data);
                        for (int i = 0; i < length; i++) {
                            while (!usb_tx_byte(data[i]));
                        }
                        p.args[0] += length;
                        p.args[1] -= length;
                    }
                    break;

                case 'E':
                    if (p.args[1] == 0) {
                        p.state = STATE_RESPONSE;
                    } else {
                        uint8_t data[8];
                        int length = (p.args[1] > 8) ? 8 : p.args[1];
                        for (int i = 0; i < length; i++) {
                            while (!usb_rx_byte(&data[i]));
                        }
                        fpga_eeprom_write(p.args[0], length, data);
                        p.args[0] += length;
                        p.args[1] -= length;
                    }
                    break;

                case 'm':
                case 'M':
                    if (!((fpga_reg_get(REG_USB_DMA_SCR) & DMA_SCR_BUSY))) {
                        if (!p.dma_in_progress) {
                            fpga_reg_set(REG_USB_DMA_ADDRESS, p.args[0]);
                            fpga_reg_set(REG_USB_DMA_LENGTH, p.args[1]);
                            fpga_reg_set(REG_USB_DMA_SCR, (p.cmd == 'M' ? DMA_SCR_DIRECTION : 0) | DMA_SCR_START);
                            p.dma_in_progress = true;
                        } else {
                            p.state = STATE_RESPONSE;
                        }
                    }
                    break;

                default:
                    p.error = true;
                    p.state = STATE_RESPONSE;
                    break;
            }
            break;
        }

        case STATE_RESPONSE: {
            if (usb_tx_word((p.error ? ERR_TOKEN : CMP_TOKEN) | p.cmd)) {
                p.state = STATE_IDLE;
                fpga_reg_set(REG_USB_SCR, USB_SCR_WRITE_FLUSH);
            }
            break;
        }
    }
}
