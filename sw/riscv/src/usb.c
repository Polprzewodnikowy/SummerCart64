#include "usb.h"
#include "dma.h"
#include "cfg.h"


static bool rx_byte (uint8_t *data) {
    if (!(USB->SCR & USB_SCR_RXNE)) {
        return false;
    }

    *data = USB->DR;

    return true;
}

static bool rx_word (uint32_t *data) {
    static uint8_t current_byte = 0;
    static uint32_t buffer = 0;
    uint8_t tmp;

    while (rx_byte(&tmp)) {
        buffer = (buffer << 8) | tmp;
        current_byte += 1;
        if (current_byte == 4) {
            current_byte = 0;
            *data = buffer;
            buffer = 0;

            return true;
        }
    }

    return false;
}

static bool tx_byte (uint8_t data) {
    if (!(USB->SCR & USB_SCR_TXE)) {
        return false;
    }

    USB->DR = data;

    return true;
}

static bool tx_word (uint32_t data) {
    static uint8_t current_byte = 0;
    while (tx_byte(data >> ((3 - current_byte) * 8))) {
        current_byte += 1;
        if (current_byte == 4) {
            current_byte = 0;

            return true;
        }
    }

    return false;
}


#define USB_CMD_TOKEN   (0x434D4400)
#define USB_CMP_TOKEN   (0x434D5000)
#define USB_ERR_TOKEN   (0x45525200)

enum state {
    STATE_IDLE,
    STATE_ARGS,
    STATE_DATA,
    STATE_RESPONSE,
};

struct process {
    enum state state;
    uint8_t counter;
    uint8_t cmd;
    uint32_t args[2];
    bool error;
    bool dma_in_progress;
};

static struct process p;


void usb_init (void) {
    USB->SCR = USB_SCR_FLUSH_TX | USB_SCR_FLUSH_RX;
    p.state = STATE_IDLE;
}


void process_usb (void) {
    switch (p.state) {
        case STATE_IDLE:
            if (rx_word(&p.args[0])) {
                if ((p.args[0] & 0xFFFFFF00) == USB_CMD_TOKEN) {
                    p.cmd = p.args[0] & 0xFF;
                    p.counter = 0;
                    p.error = false;
                    p.dma_in_progress = false;
                    p.state = STATE_ARGS;
                } else {
                    p.cmd = '!';
                    p.error = true;
                    p.state = STATE_RESPONSE;
                }
            }
            break;

        case STATE_ARGS:
            if (rx_word(&p.args[p.counter])) {
                p.counter += 1;
                if (p.counter == 2) {
                    p.state = STATE_DATA;
                }
            }
            break;

        case STATE_DATA:
            switch (p.cmd) {
                case 'C':
                    cfg_update(p.args);
                    p.state = STATE_RESPONSE;
                    break;

                case 'Q':
                    cfg_query(p.args);
                    if (tx_word(p.args[1])) {
                        p.state = STATE_RESPONSE;
                    }
                    break;

                case 'R':
                case 'W':
                    if (!dma_busy()) {
                        if (!p.dma_in_progress) {
                            enum dma_dir dir = p.cmd == 'W' ? DMA_DIR_TO_SDRAM : DMA_DIR_FROM_SDRAM;
                            dma_start(p.args[0], p.args[1], DMA_ID_USB, dir);
                            p.dma_in_progress = true;
                        } else {
                            p.state = STATE_RESPONSE;
                        }
                    }
                    break;

                default:
                    p.error = false;
                    p.state = STATE_RESPONSE;
                    break;
            }
            break;

        case STATE_RESPONSE:
            if (tx_word(p.error ? USB_ERR_TOKEN : (USB_CMP_TOKEN | p.cmd))) {
                p.state = STATE_IDLE;
            }
            break;
    }
}
