#include "sys.h"
#include "driver.h"


static e_cfg_save_type_t save_type = CFG_SAVE_TYPE_NONE;
static uint16_t cic_seed = 0xFFFF;
static uint8_t tv_type = 0xFF;


void process_usb (void);
void process_cfg (void);
void process_flashram (void);

void config_update (uint32_t *args);
void config_query (uint32_t *args);


void main (void) {
    UART->DR = 'X';
    GPIO->ODR = 0;
    GPIO->OER = (1 << 0);

    dma_abort();
    usb_flush_rx();
    usb_flush_tx();

    cfg_set_sdram_switch(true);
    cfg_set_save_type(CFG_SAVE_TYPE_NONE);
    cfg_set_dd_offset(CFG_DEFAULT_DD_OFFSET);
    cfg_set_cpu_ready(true);

    while (1) {
        process_usb();
        process_cfg();
        process_flashram();
    }
}


typedef enum {
    USB_STATE_IDLE,
    USB_STATE_ARGS,
    USB_STATE_DATA,
    USB_STATE_RESPONSE,
} e_usb_state_t;

#define USB_CMD_TOKEN   (0x434D4400)
#define USB_CMP_TOKEN   (0x434D5000)
#define USB_ERR_TOKEN   (0x45525200)

void process_usb (void) {
    static e_usb_state_t state = USB_STATE_IDLE;
    static uint8_t cmd;
    static uint32_t args[2];
    static bool error = false;
    static bool processing = false;
    static uint8_t current_word = 0;

    switch (state) {
        case USB_STATE_IDLE:
            if (usb_rx_word(&args[0])) {
                if ((args[0] & 0xFFFFFF00) == USB_CMD_TOKEN) {
                    state = USB_STATE_ARGS;
                    cmd = args[0] & 0xFF;
                    error = false;
                    processing = false;
                    current_word = 0;
                } else {
                    state = USB_STATE_RESPONSE;
                    error = true;
                }
            }
            break;

        case USB_STATE_ARGS:
            if (usb_rx_word(&args[current_word])) {
                current_word += 1;
                if (current_word == 2) {
                    state = USB_STATE_DATA;
                }
            }
            break;

        case USB_STATE_DATA:
            switch (cmd) {
                case 'C':
                    config_update(args);
                    state = USB_STATE_RESPONSE;
                    break;

                case 'Q':
                    if (!processing) {
                        config_query(args);
                        processing = true;
                    } else if (usb_tx_word(args[1])) {
                        state = USB_STATE_RESPONSE;
                    }
                    break;

                case 'R':
                case 'W':
                    if (!dma_busy()) {
                        if (!processing) {
                            e_dma_id_t id = args[1] >> 30;
                            e_dma_dir_t dir = cmd == 'W' ? DMA_DIR_TO_SDRAM : DMA_DIR_FROM_SDRAM;
                            dma_start(args[0], args[1], id, dir);
                            processing = true;
                        } else {
                            state = USB_STATE_RESPONSE;
                        }
                    }
                    break;

                case 'D':
                    break;

                default:
                    state = USB_STATE_RESPONSE;
                    error = true;
                    break;
            }
            break;

        case USB_STATE_RESPONSE:
            if (usb_tx_word(error ? USB_ERR_TOKEN : (USB_CMP_TOKEN | cmd))) {
                state = USB_STATE_IDLE;
            }
            break;
        
        default:
            state = USB_STATE_IDLE;
            break;
    }
}

void process_cfg (void) {
    uint8_t cmd;
    uint32_t args[2];

    if (cfg_get_command(&cmd, args)) {
        switch (cmd) {
            case 'C':
                config_update(args);
                cfg_set_response(args, false);
                break;

            case 'Q':
                config_query(args);
                cfg_set_response(args, false);
                break;

            case 'D':
                break;

            default:
                cfg_set_response(args, true);
                return;
        }
    }
}

void process_flashram (void) {
    e_flashram_op_t op = flashram_get_pending_operation();

    if (op != FLASHRAM_OP_NONE) {
        uint32_t length = flashram_get_operation_length(op);
        uint32_t page = flashram_get_page();
        uint32_t offset = cfg_get_save_offset() + (page * FLASHRAM_PAGE_SIZE);
        volatile uint32_t *src = flashram_get_page_buffer();
        volatile uint32_t *dst = (uint32_t *) (SDRAM_BASE + offset);

        for (uint32_t i = 0; i < (length / 4); i++) {
            if (op == FLASHRAM_OP_WRITE_PAGE) {
                dst[i] &= src[i];       
            } else {
                dst[i] = FLASHRAM_ERASE_VALUE;
            }
        }

        flashram_set_operation_done();
    }
}


typedef enum {
    CONFIG_UPDATE_SDRAM_SWITCH,
    CONFIG_UPDATE_SDRAM_WRITABLE,
    CONFIG_UPDATE_DD_ENABLE,
    CONFIG_UPDATE_SAVE_TYPE,
    CONFIG_UPDATE_CIC_SEED,
    CONFIG_UPDATE_TV_TYPE
} e_config_update_t;

void config_update (uint32_t *args) {
    switch (args[0]) {
        case CONFIG_UPDATE_SDRAM_SWITCH:
            cfg_set_sdram_switch(args[1]);
            break;
        case CONFIG_UPDATE_SDRAM_WRITABLE:
            cfg_set_sdram_writable(args[1]);
            break;
        case CONFIG_UPDATE_DD_ENABLE:
            cfg_set_dd_enable(args[1]);
            break;
        case CONFIG_UPDATE_SAVE_TYPE:
            save_type = args[1] & 0x07;
            cfg_set_save_type(save_type);
            break;
        case CONFIG_UPDATE_CIC_SEED:
            cic_seed = args[1] & 0xFFFF;
            break;
        case CONFIG_UPDATE_TV_TYPE:
            tv_type = args[1] & 0xFF;
            break;
    }
}

typedef enum {
    CONFIG_QUERY_STATUS,
    CONFIG_QUERY_SAVE_TYPE,
    CONFIG_QUERY_CIC_SEED,
    CONFIG_QUERY_TV_TYPE,
    CONFIG_QUERY_SAVE_OFFSET,
    CONFIG_QUERY_DD_OFFSET,
} e_config_query_t;

void config_query (uint32_t *args) {
    switch (args[0]) {
        case CONFIG_QUERY_STATUS:
            args[1] = cfg_get_status();
            break;
        case CONFIG_QUERY_SAVE_TYPE:
            args[1] = save_type;
            break;
        case CONFIG_QUERY_CIC_SEED:
            args[1] = cic_seed;
            break;
        case CONFIG_QUERY_TV_TYPE:
            args[1] = tv_type;
            break;
        case CONFIG_QUERY_SAVE_OFFSET:
            args[1] = cfg_get_save_offset();
            break;
        case CONFIG_QUERY_DD_OFFSET:
            args[1] = cfg_get_dd_offset();
            break;
    }
}
