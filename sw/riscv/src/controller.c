#include "sys.h"
#include "driver.h"
#include "controller.h"


static e_cfg_save_type_t save_type = CFG_SAVE_TYPE_NONE;
static uint16_t cic_seed = 0xFFFF;
static uint8_t tv_type = 0xFF;

static bool eeprom_enabled = false;

static bool rtc_running = true;
static uint8_t rtc_wp_bits = SI_RTC_WP_MASK;
static uint8_t current_time[7];
static uint8_t new_time[7];
static bool new_time_pending = false;

static bool n64_to_pc_pending = false;
static uint32_t n64_to_pc_address;
static uint32_t n64_to_pc_length;

static bool pc_to_n64_ready = false;
static uint32_t pc_to_n64_arg;
static uint32_t pc_to_n64_address;
static uint32_t pc_to_n64_length;


void process_usb (void);
void process_cfg (void);
void process_flashram (void);
void process_si (void);
void process_rtc (void);
void process_uart (void);


bool debug_decide (uint32_t *args);
void config_update (uint32_t *args);
void config_query (uint32_t *args);


void main (void) {
    GPIO->ODR = 0x00;
    GPIO->OER = (1 << 0);

    dma_abort();
    usb_flush_rx();
    usb_flush_tx();
    si_reset();
    i2c_stop();

    cfg_set_sdram_switch(true);
    cfg_set_save_type(CFG_SAVE_TYPE_NONE);
    cfg_set_dd_offset(CFG_DEFAULT_DD_OFFSET);
    cfg_set_cpu_ready(true);

    print("App ready!\r\n");

    while (1) {
        process_usb();
        process_cfg();
        process_flashram();
        process_si();
        process_rtc();
        process_uart();
    }
}


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
                    cmd = args[0] & 0xFF;
                    error = false;
                    processing = false;
                    current_word = 0;
                    state = USB_STATE_ARGS;
                } else {
                    cmd = '!';
                    error = true;
                    state = USB_STATE_RESPONSE;
                }
            } else if (n64_to_pc_pending) {
                if (!dma_busy()) {
                    dma_start(n64_to_pc_address, n64_to_pc_length, DMA_ID_USB, DMA_DIR_FROM_SDRAM);
                    state = USB_STATE_N64_TO_PC;
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
                    cfg_set_usb_waiting(true);
                    pc_to_n64_arg = args[0];
                    pc_to_n64_length = args[1];
                    state = USB_STATE_PC_TO_N64;
                    break;

                default:
                    error = true;
                    state = USB_STATE_RESPONSE;
                    break;
            }
            break;

        case USB_STATE_RESPONSE:
            if (usb_tx_word(error ? USB_ERR_TOKEN : (USB_CMP_TOKEN | cmd))) {
                state = USB_STATE_IDLE;
            }
            break;

        case USB_STATE_N64_TO_PC:
            if (!dma_busy()) {
                n64_to_pc_pending = false;
                state = USB_STATE_IDLE;
            }
            break;

        case USB_STATE_PC_TO_N64:
            if (!dma_busy()) {
                if (!processing) {
                    if (pc_to_n64_ready) {
                        dma_start(pc_to_n64_address, pc_to_n64_length, DMA_ID_USB, DMA_DIR_TO_SDRAM);
                        processing = true;
                    }
                } else {
                    pc_to_n64_ready = false;
                    processing = false;
                    state = USB_STATE_IDLE;
                }
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
    bool error;

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
                error = debug_decide(args);
                cfg_set_response(args, error);
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

void process_si (void) {
    uint8_t rx_data[10];
    uint8_t tx_data[12];
    uint32_t *save_data;
    uint32_t *data_offset;

    if (si_rx_ready()) {
        if (si_rx_stop_bit()) {
            si_rx(rx_data);

            for (size_t i = 0; i < sizeof(tx_data); i++) {
                tx_data[i] = 0x00;
            }

            if (eeprom_enabled) {
                save_data = (uint32_t *) (SDRAM_BASE + cfg_get_save_offset() + (rx_data[1] * 8));
                switch (rx_data[0]) {
                    case SI_CMD_EEPROM_STATUS:
                        tx_data[1] = save_type == CFG_SAVE_TYPE_EEPROM_4K ? SI_EEPROM_ID_4K : SI_EEPROM_ID_16K;
                        si_tx(tx_data, 3);
                        break;

                    case SI_CMD_EEPROM_READ:
                        data_offset = (uint32_t *) (&tx_data[0]);
                        data_offset[0] = swapb(save_data[0]);
                        data_offset[1] = swapb(save_data[1]);
                        si_tx(tx_data, 8);
                        break;

                    case SI_CMD_EEPROM_WRITE:
                        data_offset = (uint32_t *) (&rx_data[2]);
                        save_data[0] = swapb(data_offset[0]);
                        save_data[1] = swapb(data_offset[1]);
                        si_tx(tx_data, 1);
                        break;
                }
            }

            switch (rx_data[0]) {
                case SI_CMD_RTC_STATUS:
                    tx_data[1] = SI_RTC_ID;
                    tx_data[2] = SI_RTC_STATUS(rtc_running);
                    si_tx(tx_data, 3);
                    break;

                case SI_CMD_RTC_READ:
                    if (rx_data[1] == 0) {
                        tx_data[0] = rtc_wp_bits;
                        if (!rtc_running) {
                            tx_data[1] = SI_RTC_ST_MASK;
                        }
                    } else if (rx_data[1] == 2) {
                        for (size_t i = 0; i < 7; i++) {
                            tx_data[i] = current_time[i];
                        }
                        tx_data[7] = SI_RTC_CENTURY_20XX;
                    }
                    tx_data[8] = SI_RTC_STATUS(rtc_running);
                    si_tx(tx_data, 9);
                    break;

                case SI_CMD_RTC_WRITE:
                    if (rx_data[1] == 0) {
                        rtc_wp_bits = rx_data[2] & SI_RTC_WP_MASK;
                        rtc_running = (!(rx_data[3] & SI_RTC_ST_MASK));
                    } else if (rx_data[1] == 2) {
                        for (size_t i = 0; i < 7; i++) {
                            new_time[i] = rx_data[i + 2];
                        }
                        new_time_pending = !rtc_running;
                    }
                    tx_data[0] = SI_RTC_STATUS(rtc_running);
                    si_tx(tx_data, 1);
                    break;
            }
        }

        si_rx_reset();
    }
}

void process_rtc (void) {
    static e_rtc_state_t state = RTC_STATE_READ;
    static e_rtc_state_t next_state = RTC_STATE_READ;
    static uint8_t phase = 0;
    static uint8_t sub_phase = 0;
    static uint8_t rtc_current_time[7];
    static uint8_t rtc_new_time[7];

    if (new_time_pending) {
        new_time_pending = false;

        for (int i = 0; i < 7; i++) {
            current_time[i] = new_time[i];
        }

        rtc_convert_from_n64(new_time, rtc_new_time);

        rtc_new_time[RTC_RTCSEC] |= RTC_RTCSEC_ST;
        rtc_new_time[RTC_RTCWKDAY] |= RTC_RTCWKDAY_VBAT;

        next_state = RTC_STATE_WRITE;
    }

    // TODO: Rewrite this mess
    if (!i2c_busy()) {
        if (state == RTC_STATE_READ) {
            switch (phase) {
                case 0: phase++; i2c_start(); break;
                case 1: phase++; i2c_begin_trx(RTC_ADDR, false); break;
                case 2: phase++; i2c_begin_trx(RTC_RTCSEC, false); break;
                case 3: phase++; i2c_start(); break;
                case 4: phase++; i2c_begin_trx(RTC_ADDR | I2C_ADDR_READ, false); break;
                case 5:
                    if (sub_phase > 0) {
                        rtc_current_time[sub_phase - 1] = i2c_get_data();
                    }
                    if (sub_phase < 7) {
                        i2c_begin_trx(0xFF, true);
                    }
                    if (sub_phase == 7) {
                        phase++;
                        sub_phase = 0;
                        if (next_state != RTC_STATE_WRITE) {
                            rtc_convert_to_n64(rtc_current_time, current_time);
                        }
                        break;
                    }
                    sub_phase++;
                    break;
                case 6: phase = 0; i2c_stop(); state = next_state; break;
            }
        } else if (state == RTC_STATE_WRITE) {
            switch (phase) {
                case 0: phase++; break;

                case 1: phase++; i2c_start(); break;
                case 2: phase++; i2c_begin_trx(RTC_ADDR, false); break;
                case 3: phase++; i2c_begin_trx(RTC_RTCSEC, false); break;
                case 4: phase++; i2c_begin_trx(0x00, false); break;
                case 5: phase++; i2c_stop(); break;

                case 6: phase++; i2c_start(); break;
                case 7: phase++; i2c_begin_trx(RTC_ADDR, false); break;
                case 8: phase++; i2c_begin_trx(RTC_RTCWKDAY, false); break;
                case 9: phase++; i2c_start(); break;
                case 10: phase++; i2c_begin_trx(RTC_ADDR | I2C_ADDR_READ, false); break;
                case 11: phase++; i2c_begin_trx(0xFF, false); break;
                case 12: phase = (i2c_get_data() & RTC_RTCWKDAY_OSCRUN) ? 6 : 13; i2c_stop(); break;

                case 13: phase++; i2c_start(); break;
                case 14: phase++; i2c_begin_trx(RTC_ADDR, false); break;
                case 15: phase++; i2c_begin_trx(RTC_RTCMIN, false); break;
                case 16:
                    if (sub_phase < 6) {
                        i2c_begin_trx(rtc_new_time[RTC_RTCMIN + sub_phase], false);
                        sub_phase++;
                    } else {
                        phase++;
                        sub_phase = 0;
                        i2c_stop();
                    }
                    break;

                case 17: phase++; i2c_start(); break;
                case 18: phase++; i2c_begin_trx(RTC_ADDR, false); break;
                case 19: phase++; i2c_begin_trx(RTC_RTCSEC, false); break;
                case 20: phase++; i2c_begin_trx(rtc_new_time[RTC_RTCSEC], false); break;
                case 21: phase++; i2c_stop(); break;

                case 22: phase++; i2c_start(); break;
                case 23: phase++; i2c_begin_trx(RTC_ADDR, false); break;
                case 24: phase++; i2c_begin_trx(RTC_RTCWKDAY, false); break;
                case 25: phase++; i2c_start(); break;
                case 26: phase++; i2c_begin_trx(RTC_ADDR | I2C_ADDR_READ, false); break;
                case 27: phase++; i2c_begin_trx(0xFF, false); break;
                case 28: 
                    if (i2c_get_data() & RTC_RTCWKDAY_OSCRUN) {
                        phase = 0;
                        state = RTC_STATE_READ;
                        next_state = RTC_STATE_READ;
                    } else {
                        phase = 22;
                    }
                    i2c_stop();
                    break;
            }
        } else {
            state = RTC_STATE_READ;
            phase = 0;
            sub_phase = 0;
        }
    }
}

void process_uart (void) {
    if (UART->SCR & USB_SCR_RXNE) {
        char cmd = UART->DR;

        switch (cmd) {
            case '/':
                print("Bootloader reset...\r\n");
                reset_handler();
                break;

            case '\'':
                print("App reset...\r\n");
                app_handler();
                break;

            case 't':
                print("Current time: ");
                for (int i = 0; i < 7; i++) {
                    print_02hex(current_time[i]);
                    print(" ");
                }
                print("\r\n");
        }
    }
}


bool debug_decide (uint32_t *args) {
    switch (args[0]) {
        case DEBUG_WRITE_ADDRESS:
            if (!n64_to_pc_pending) {
                n64_to_pc_address = args[1];
            } else {
                return true;
            }
            break;
        case DEBUG_WRITE_LENGTH_START:
            if (!n64_to_pc_pending) {
                n64_to_pc_length = args[1];
                n64_to_pc_pending = true;
            } else {
                return true;
            }
            break;
        case DEBUG_WRITE_STATUS:
            args[1] = n64_to_pc_pending;
            break;
        case DEBUG_READ_ARG:
            args[1] = pc_to_n64_arg;
            break;
        case DEBUG_READ_LENGTH:
            args[1] = pc_to_n64_length;
            break;
        case DEBUG_READ_ADDRESS_START:
            if (!pc_to_n64_ready) {
                pc_to_n64_ready = true;
                pc_to_n64_address = args[1];
                cfg_set_usb_waiting(false);
            } else {
                return true;
            }
            break;
        default:
            return true;
    }

    return false;
}

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
            eeprom_enabled = (
                save_type == CFG_SAVE_TYPE_EEPROM_4K ||
                save_type == CFG_SAVE_TYPE_EEPROM_16K
            );
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
