#include "usb.h"
#include "dma.h"
#include "cfg.h"
#include "dd.h"


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


#define USB_CMD_TOKEN       (0x434D4400)
#define USB_CMP_TOKEN       (0x434D5000)
#define USB_DMA_TOKEN       (0x444D4100)
#define USB_ERR_TOKEN       (0x45525200)

#define DEBUG_ID_EVENT      (0xFE)


enum state {
    STATE_IDLE,
    STATE_ARGS,
    STATE_DATA,
    STATE_RESPONSE,
    STATE_EVENT,
};

struct process {
    enum state state;
    uint32_t counter;
    uint8_t cmd;
    uint32_t args[2];
    bool error;
    bool dma_in_progress;
    bool queried;

    bool event_pending;
    bool event_callback_pending;
    usb_event_t event;
    uint8_t event_data[16];
    uint32_t event_data_length;
};

static struct process p;


bool usb_put_event (usb_event_t *event, void *data, uint32_t length) {
    if (p.event_pending || p.event_callback_pending) {
        return false;
    }

    uint8_t *src = (uint8_t *) (data);
    uint8_t *dst = p.event_data;

    p.event_pending = true;
    p.event_callback_pending = false;
    p.event = *event;
    p.event_data_length = length <= sizeof(p.event_data) ? length : sizeof(p.event_data);
    for (int i = 0; i < p.event_data_length; i++) {
        *dst++ = *src++;
    }

    return true;
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
    USB->SCR = (USB_SCR_ENABLED | USB_SCR_FLUSH_TX | USB_SCR_FLUSH_RX);

    p.state = STATE_IDLE;
    p.event_pending = false;
    p.event_callback_pending = false;

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
            if (rx_cmd(&p.args[0])) {
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
            } else if (p.event_pending && (!p.event_callback_pending)) {
                p.state = STATE_EVENT;
                p.counter = 0;
            }
            break;

        case STATE_ARGS:
            if (rx_word(&p.args[p.counter])) {
                p.counter += 1;
                if (p.counter == 2) {
                    p.counter = 0;
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
                case 'L':
                case 'S':
                    if (!dma_busy()) {
                        if (!p.dma_in_progress) {
                            bool is_write = (p.cmd == 'W') || (p.cmd == 'S');
                            enum dma_dir dir = is_write ? DMA_DIR_TO_SDRAM : DMA_DIR_FROM_SDRAM;
                            dma_start(p.args[0], p.args[1], DMA_ID_USB, dir);
                            p.dma_in_progress = true;
                        } else {
                            if (p.cmd == 'L' || p.cmd == 'S') {
                                if (p.event_callback_pending) {
                                    if (p.cmd == 'L' && p.event.trigger == CALLBACK_SDRAM_READ) {
                                        p.event_callback_pending = false;
                                        p.event.callback();
                                    }
                                    if (p.cmd == 'S' && p.event.trigger == CALLBACK_SDRAM_WRITE) {
                                        p.event_callback_pending = false;
                                        p.event.callback();
                                    }
                                }
                                if (p.cmd == 'L') {
                                    USB->SCR |= USB_SCR_FORCE_TX;
                                }
                                p.state = STATE_IDLE;
                            } else {
                                p.state = STATE_RESPONSE;
                            }
                        }
                    }
                    break;

                case 'F':
                case 'T':
                    while ((p.args[0] + p.counter) != (p.args[0] + p.args[1])) {
                        uint8_t *buffer = (uint8_t *) (RAMBUFFER_BASE + p.args[0] + p.counter);
                        if (p.cmd == 'F') {
                            if (tx_byte(*buffer)) {
                                p.counter += 1;
                            } else {
                                break;
                            }
                        }
                        if (p.cmd == 'T') {
                            if (rx_byte(buffer)) {
                                p.counter += 1;
                            } else {
                                break;
                            }
                        }
                    }
                    if ((p.args[0] + p.counter) == (p.args[0] + p.args[1])) {
                        if (p.event_callback_pending) {
                            if (p.cmd == 'F' && p.event.trigger == CALLBACK_BUFFER_READ) {
                                p.event_callback_pending = false;
                                p.event.callback();
                            }
                            if (p.cmd == 'T' && p.event.trigger == CALLBACK_BUFFER_WRITE) {
                                p.event_callback_pending = false;
                                p.event.callback();
                            }
                        }
                        if (p.cmd == 'F') {
                            USB->SCR |= USB_SCR_FORCE_TX;
                        }
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
                USB->SCR |= USB_SCR_FORCE_TX;
            }
            break;

        case STATE_EVENT:
            if ((p.counter == 0) && tx_word(USB_DMA_TOKEN | '@')) {
                p.counter += 1;
            }
            if ((p.counter == 1) && tx_word((DEBUG_ID_EVENT << 24) | (sizeof(p.event.id) + p.event_data_length))) {
                p.counter += 1;
            }
            if ((p.counter == 2) && tx_word(p.event.id)) {
                p.counter += 1;
            }
            if (p.counter >= 3) {
                while (((p.counter - 3) < p.event_data_length) && tx_byte(p.event_data[p.counter - 3])) {
                    p.counter += 1;
                }
            }
            if ((p.counter == (p.event_data_length + 3)) && tx_word(USB_CMP_TOKEN | 'H')) {
                if (p.event.callback != NULL) {
                    p.event_callback_pending = true;
                }
                USB->SCR |= USB_SCR_FORCE_TX;
                p.event_pending = false;
                p.state = STATE_IDLE;
            }
            break;
    }
}
