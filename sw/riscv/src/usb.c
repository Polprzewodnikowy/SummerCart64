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

static uint8_t rx_word_current_byte = 0;
static uint32_t rx_word_buffer = 0;

static bool rx_word (uint32_t *data) {
    uint8_t tmp;

    while (rx_byte(&tmp)) {
        rx_word_buffer = (rx_word_buffer << 8) | tmp;
        rx_word_current_byte += 1;
        if (rx_word_current_byte == 4) {
            rx_word_current_byte = 0;
            *data = rx_word_buffer;
            rx_word_buffer = 0;

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

static uint8_t tx_word_current_byte = 0;

static bool tx_word (uint32_t data) {
    while (tx_byte(data >> ((3 - tx_word_current_byte) * 8))) {
        tx_word_current_byte += 1;
        if (tx_word_current_byte == 4) {
            tx_word_current_byte = 0;

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
    STATE_DEBUG_TX,
};

struct process {
    enum state state;
    uint8_t counter;
    uint8_t cmd;
    uint32_t args[2];
    bool error;
    bool dma_in_progress;
    bool queried;

    bool debug_rx_busy;
    uint32_t debug_rx_address;
    size_t debug_rx_length;

    bool debug_tx_busy;
    uint32_t debug_tx_address;
    size_t debug_tx_length;
};

static struct process p;


bool usb_debug_rx_ready (uint32_t *type, size_t *length) {
    if (p.state != STATE_DATA || p.cmd != 'D' || p.debug_rx_busy) {
        return false;
    }

    *type = p.args[0];
    *length = (size_t) p.args[1];

    return true;
}

bool usb_debug_rx_busy (void) {
    return p.debug_rx_busy;
}

bool usb_debug_rx_data (uint32_t address, size_t length) {
    if (p.debug_rx_busy) {
        return false;
    }

    p.debug_rx_busy = true;
    p.debug_rx_address = address;
    p.debug_rx_length = length;

    return true;
}

bool usb_debug_tx_ready (void) {
    return !p.debug_tx_busy;
}

bool usb_debug_tx_data (uint32_t address, size_t length) {
    if (p.debug_tx_busy) {
        return false;
    }

    p.debug_tx_busy = true;
    p.debug_tx_address = address;
    p.debug_tx_length = length;

    return true;
}

void usb_debug_reset (void) {
    uint8_t tmp;

    if (p.state == STATE_DATA && p.cmd == 'D') {
        for (size_t i = 0; i < p.args[1]; i++) {
            rx_byte(&tmp);
        }
        p.args[1] = 0;    
    }
    if (p.state == STATE_DEBUG_TX) {
        p.state = STATE_IDLE;
    }
    p.debug_rx_busy = false;
    p.debug_tx_busy = false;

    USB->SCR = USB_SCR_FLUSH_TX | USB_SCR_FLUSH_RX;
}

static uint8_t rx_cmd_current_byte = 0;
static uint32_t rx_cmd_buffer = 0;

static bool rx_cmd (uint32_t *data) {
    uint8_t tmp;

    while (rx_byte(&tmp)) {
        rx_cmd_current_byte += 1;
        if ((rx_cmd_current_byte != 4) && (tmp != (USB_CMD_TOKEN >> (8 * (4 - rx_cmd_current_byte)) & 0xFF))) {
            rx_cmd_current_byte = 0;
            rx_cmd_buffer = 0;

            return false;
        }
        rx_cmd_buffer = (rx_cmd_buffer << 8) | tmp;
        if (rx_cmd_current_byte == 4) {
            rx_cmd_current_byte = 0;
            *data = rx_cmd_buffer;
            rx_cmd_buffer = 0;

            return true;
        }
    }

    return false;
}

static void handle_escape (void) {
    if (USB->SCR & USB_SCR_ESCAPE_PENDING) {
        if (USB->ESCAPE == 'R') {
            if (p.dma_in_progress) {
                dma_stop();
                while (dma_busy());
            }
            usb_init();
        }
        USB->SCR |= USB_SCR_ESCAPE_ACK;
    }
}


void usb_init (void) {
    USB->SCR = USB_SCR_ENABLED | USB_SCR_FLUSH_TX | USB_SCR_FLUSH_RX;

    p.state = STATE_IDLE;
    p.debug_rx_busy = false;
    p.debug_tx_busy = false;

    rx_word_current_byte = 0;
    rx_word_buffer = 0;
    tx_word_current_byte = 0;
    rx_cmd_current_byte = 0;
    rx_cmd_buffer = 0;
}


void process_usb (void) {
    handle_escape();

    switch (p.state) {
        case STATE_IDLE:
            if (p.debug_tx_busy) {
                p.state = STATE_DEBUG_TX;
                p.dma_in_progress = false;
            } else if (rx_cmd(&p.args[0])) {
                if ((p.args[0] & 0xFFFFFF00) == USB_CMD_TOKEN) {
                    p.cmd = p.args[0] & 0xFF;
                    p.counter = 0;
                    p.error = false;
                    p.dma_in_progress = false;
                    p.queried = false;
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
                case 'V':
                    if (tx_word(cfg_get_version())) {
                        p.state = STATE_RESPONSE;
                    }
                    break;

                case 'C':
                    cfg_update(p.args);
                    p.state = STATE_RESPONSE;
                    break;

                case 'Q':
                    if (!p.queried) {
                        cfg_query(p.args);
                        p.queried = true;
                    }
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

                case 'D':
                    if (!dma_busy() && p.debug_rx_busy && p.args[1] > 0) {
                        if (!p.dma_in_progress) {
                            dma_start(p.debug_rx_address, p.debug_rx_length, DMA_ID_USB, DMA_DIR_TO_SDRAM);
                            p.dma_in_progress = true;
                        } else {
                            p.args[1] -= p.debug_rx_length > p.args[1] ? p.args[1] : p.debug_rx_length;
                            p.dma_in_progress = false;
                            p.debug_rx_busy = false;
                        }
                    }
                    if (p.args[1] == 0) {
                        p.state = STATE_IDLE;
                    }
                    break;

                default:
                    p.error = true;
                    p.state = STATE_RESPONSE;
                    break;
            }
            break;

        case STATE_RESPONSE:
            if (tx_word((p.error ? USB_ERR_TOKEN : USB_CMP_TOKEN) | p.cmd)) {
                p.state = STATE_IDLE;
            }
            break;

        case STATE_DEBUG_TX:
            if (!dma_busy()) {
                if (!p.dma_in_progress) {
                    dma_start(p.debug_tx_address, p.debug_tx_length, DMA_ID_USB, DMA_DIR_FROM_SDRAM);
                    p.dma_in_progress = true;
                } else {
                    p.debug_tx_busy = false;
                    p.state = STATE_IDLE;
                }
            }
            break;
    }
}
