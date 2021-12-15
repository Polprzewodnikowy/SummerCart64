#include "dd.h"
#include "uart.h"
#include "rtc.h"
#include "usb.h"


#define DD_DRIVE_ID_RETAIL          (0x03)
#define DD_VERSION_RETAIL           (0x0114)

typedef enum {
    DD_CMD_SEEK_READ                = 0x01,
    DD_CMD_SEEK_WRITE               = 0x02,
    DD_CMD_CLEAR_DISK_CHANGE        = 0x08,
    DD_CMD_CLEAR_RESET_STATE        = 0x09,
    DD_CMD_READ_VERSION             = 0x0A,
    DD_CMD_SET_DISK_TYPE            = 0x0B,
    DD_CMD_REQUEST_STATUS           = 0x0C,
    DD_CMD_SET_RTC_YEAR_MONTH       = 0x0F,
    DD_CMD_SET_RTC_DAY_HOUR         = 0x10,
    DD_CMD_SET_RTC_MINUTE_SECOND    = 0x11,
    DD_CMD_GET_RTC_YEAR_MONTH       = 0x12,
    DD_CMD_GET_RTC_DAY_HOUR         = 0x13,
    DD_CMD_GET_RTC_MINUTE_SECOND    = 0x14,
    DD_CMD_READ_PROGRAM_VERSION     = 0x1B,
} dd_cmd_t;


static rtc_time_t time;
static uint8_t current_sector;
static bool continued_transfer;


static void log_bm_op (uint32_t scr) {
    if (scr & DD_SCR_BM_STOP) {
        uart_print("Got BM stop [0x");
        uart_print_02hex(current_sector); uart_print("]\r\n");
    }
    
    if (scr & DD_SCR_BM_START) {
        uart_print("Got BM start [0x");
        // uart_print_02hex((DD->HEAD_TRACK >> 8) & 0xFF);
        // uart_print_02hex((DD->HEAD_TRACK >> 0) & 0xFF); uart_print(", ");
        // uart_print_02hex(DD->SECTOR_NUM); uart_print(", ");
        // uart_print_02hex(DD->SECTOR_SIZE); uart_print(", ");
        // uart_print_02hex(DD->SECTOR_SIZE_FULL); uart_print(", ");
        // uart_print_02hex(DD->SECTORS_IN_BLOCK); uart_print("]\r\n");
        
        // uart_print("DD Info [0x");
        uart_print_08hex(DD->SCR); uart_print(", ");
        uart_print_02hex((DD->DATA >> 8) & 0xFF);
        uart_print_02hex((DD->DATA >> 0) & 0xFF); uart_print(", ");
        uart_print_02hex(DD->CMD); uart_print(", ");
        uart_print_02hex((DD->HEAD_TRACK >> 8) & 0xFF);
        uart_print_02hex((DD->HEAD_TRACK >> 0) & 0xFF); uart_print(", ");
        uart_print_02hex(DD->SECTOR_NUM); uart_print(", ");
        uart_print_02hex(DD->SECTOR_SIZE); uart_print(", ");
        uart_print_02hex(DD->SECTOR_SIZE_FULL); uart_print(", ");
        uart_print_02hex(DD->SECTORS_IN_BLOCK); uart_print(", ");
        uart_print_02hex(DD->DRIVE_ID); uart_print("]\r\n");
        // uart_print("Requesting data sector [0x00]\r\n");
    }
    
    if (scr & DD_SCR_BM_PENDING) {
        if (current_sector < 85) {
            // uart_print("Requesting data sector [0x");
            // uart_print_02hex(current_sector); uart_print("]\r\n");
        } else if (current_sector < 85 + 4) {
            uart_print("Read C2 sector [0x");
            uart_print_02hex(current_sector); uart_print("]\r\n");
        } else if (current_sector == 85 + 4 - 1) {
            uart_print("Gap sector??\r\n");
        } else {
            uart_print("Unknown BM pending [0x");
            uart_print_02hex(current_sector); uart_print("]\r\n");
        }
    }
}


enum state {
    STATE_IDLE,
    STATE_START,
    STATE_FETCH,
    STATE_WAIT,
    STATE_WAIT_2,
    STATE_TRANSFER,
    STATE_STOP,
};

static enum state state;


void dd_init (void) {
    DD->SCR &= DD_SCR_DISK_INSERTED;
    DD->HEAD_TRACK = 0;
    DD->DRIVE_ID = DD_DRIVE_ID_RETAIL;
    state = STATE_IDLE;
}


void process_dd (void) {
    uint32_t scr = DD->SCR;

    if (scr & DD_SCR_HARD_RESET) {
        dd_init();
    }

    if (scr & DD_SCR_CMD_PENDING) {
        dd_cmd_t cmd = (dd_cmd_t) (DD->CMD);
        uint16_t data = DD->DATA;

        if (cmd < DD_CMD_GET_RTC_YEAR_MONTH || cmd > DD_CMD_GET_RTC_MINUTE_SECOND) {
            uart_print("Got DD command: [0x");
            uart_print_02hex(cmd); uart_print("], data: [0x");
            uart_print_02hex((data >> 8) & 0xFF); uart_print_02hex((data >> 0) & 0xFF); uart_print("]\r\n");
        }

        switch (cmd) {
            case DD_CMD_SEEK_READ:
                DD->HEAD_TRACK = DD_HEAD_TRACK_INDEX_LOCK | data;
                break;

            case DD_CMD_SEEK_WRITE:
                DD->HEAD_TRACK = DD_HEAD_TRACK_INDEX_LOCK | data;
                break;

            case DD_CMD_CLEAR_DISK_CHANGE:
                DD->SCR &= ~(DD_SCR_DISK_CHANGED);
                break;

            case DD_CMD_CLEAR_RESET_STATE:
                DD->SCR &= ~(DD_SCR_DISK_CHANGED);
                DD->SCR |= DD_SCR_HARD_RESET_CLEAR;
                break;

            case DD_CMD_READ_VERSION:
                DD->DATA = DD_VERSION_RETAIL;
                break;

            case DD_CMD_SET_DISK_TYPE:
                // TODO
                break;

            case DD_CMD_REQUEST_STATUS:
                DD->DATA = 0;
                break;

            case DD_CMD_SET_RTC_YEAR_MONTH:
                time.year = ((data >> 8) & 0xFF);
                time.month = (data & 0xFF);
                break;

            case DD_CMD_SET_RTC_DAY_HOUR:
                time.day = ((data >> 8) & 0xFF);
                time.hour = (data & 0xFF);
                break;

            case DD_CMD_SET_RTC_MINUTE_SECOND:
                time.minute = ((data >> 8) & 0xFF);
                time.second = (data & 0xFF);
                rtc_set_time(&time);
                break;

            case DD_CMD_GET_RTC_YEAR_MONTH:
                DD->DATA = (time.year << 8) | time.month;
                break;

            case DD_CMD_GET_RTC_DAY_HOUR:
                DD->DATA = (time.day << 8) | time.hour;
                break;

            case DD_CMD_GET_RTC_MINUTE_SECOND:
                time = *rtc_get_time();
                DD->DATA = (time.minute << 8) | time.second;
                break;

            case DD_CMD_READ_PROGRAM_VERSION:
                DD->DATA = 0;
                break;
        }

        DD->SCR |= DD_SCR_CMD_READY;
    }

    // log_bm_op (scr);

    uint16_t track = (DD->HEAD_TRACK & DD_HEAD_TRACK_MASK);
    uint8_t starting_sector = DD->SECTOR_NUM;
    bool forbidden_block = (track == 6) && (starting_sector == 0);

    if (scr & DD_SCR_BM_ACK) {
        DD->SCR |= DD_SCR_BM_ACK_CLEAR;
        if (current_sector <= 85) {
            // if (state == STATE_TRANSFER) {
            //     state = STATE_FETCH;
            //     current_sector += 1;
            // }
        } else if (current_sector < 85 + 3) {
            // for (volatile int i = 0; i < 100000; i++) {
            //     UART->SCR = 0;
            // }
            uart_print("issuing no C2 in freely [0x");
            uart_print_08hex(scr);
            uart_print("]\r\n");
            DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
            DD->SCR |= DD_SCR_BM_READY;
            current_sector += 1;
        } else if (current_sector < 85 + 4) {
            // for (volatile int i = 0; i < 100000; i++) {
            //     UART->SCR = 0;
            // }
            uart_print("issuing C2 freely [0x");
            uart_print_08hex(scr);
            uart_print("]\r\n");
            DD->SCR &= ~(DD_SCR_BM_TRANSFER_DATA);
            DD->SCR |= DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_READY;
            current_sector += 1;
        } else {
            uart_print("gap sector imaginary [0x");
            uart_print_08hex(scr);
            uart_print("]\r\n");
            current_sector = 0;
            DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
            if (continued_transfer) {
                state = STATE_STOP;
            } else {
                if (DD->SCR & DD_SCR_BM_TRANSFER_BLOCKS) {
                    continued_transfer = true;
                    state = STATE_FETCH;
                } else {
                    state = STATE_STOP;
                }
            }
        }
    }

    if (scr & DD_SCR_BM_STOP) {
        DD->SCR |= DD_SCR_BM_STOP_CLEAR;
        DD->SCR &= ~(DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
        state = STATE_STOP;
    } else if (scr & DD_SCR_BM_START) {
        DD->SCR |= DD_SCR_BM_START_CLEAR;
        DD->SCR &= ~(DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);

        if (forbidden_block) {
            DD->SCR |= DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_READY;
        } else {
            // kick off transfer here
            // DD->SCR |= DD_SCR_BM_TRANSFER_DATA;
            state = STATE_START;
        }

        continued_transfer = false;
        current_sector = 0;

        // current_sector = 1;
    } else if (scr & DD_SCR_BM_PENDING) {
        if (forbidden_block) {
            DD->SCR |= DD_SCR_BM_READY;
        } else {
            if (current_sector < 85) {
                if (state == STATE_TRANSFER) {
                    state = STATE_FETCH;
                    // current_sector += 1;
                }
            } else if (current_sector == 85) {
                // uart_print("issuing no C2 in bm_pending\r\n");
                DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
                DD->SCR |= DD_SCR_BM_READY;
                current_sector += 1;
            } else if (current_sector < 85 + 4) {
                // DD->SCR &= ~(DD_SCR_BM_TRANSFER_DATA);
                // DD->SCR |= DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_READY;
                // current_sector += 1;
            } else {
                uart_print("gap sector read\r\n");
                DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
                // DD->SCR |= DD_SCR_BM_READY;
                current_sector = 0;
                state = STATE_STOP;
            }
            
            //  else {
            //     DD->SCR |= DD_SCR_BM_READY;
            //     state = STATE_STOP;
            //     current_sector = 0;
            // }
        }
        // if (current_sector < 85) {
        //     DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2);
        //     DD->SCR |= DD_SCR_BM_TRANSFER_DATA;
        // } else if (current_sector < 85 + 4) {
        // } else if (current_sector == 88) {
        //     DD->SCR &= ~(DD_SCR_BM_TRANSFER_DATA);
        //     DD->SCR |= DD_SCR_BM_TRANSFER_C2;
        // } else {
        //     return;
        // }

        // current_sector += 1;

        // DD->SCR |= DD_SCR_BM_READY;


        // else {
        //     DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
        // }

        // if (forbidden_block) {
        //     DD->SCR |= DD_SCR_BM_READY;
        // } else {
        //     // request more data here
        //     DD->SCR |= DD_SCR_BM_READY;
        // }
    }

    // else if (current_sector < 85 + 4) {
    // }

    io32_t *sdram = (io32_t *) (SDRAM_BASE + (32 * 1024 * 1024));
    io32_t *dst = sdram;
    const char *dma = "DMA@";
    const char *cmp = "CMPH";
    io32_t *buf = DD->SEC_BUF;

    switch (state) {
        case STATE_IDLE:
            break;

        case STATE_START:
            usb_debug_reset();
            // if (usb_debug_tx_ready()) {
                // uart_print("state start\r\n");
            // current_sector = 0;
            state = STATE_FETCH;
            // }
            break;

        case STATE_FETCH:
            if (current_sector != 0) {
                state = STATE_WAIT_2;
            } else if (usb_debug_tx_ready()) {
                // uart_print("state fetch\r\n");
                *dst++ = *((uint32_t *) (dma));
                *dst++ = 0xF5 | (7 * 4) << 24;
                *dst++ = (uint32_t) DD->SCR;
                *dst++ = (uint32_t) current_sector;
                *dst++ = (uint32_t) DD->HEAD_TRACK;
                if (continued_transfer) {
                    *dst++ = DD->SECTOR_NUM == 0x5A ? 0 : 0x5A;
                } else {
                    *dst++ = (uint32_t) DD->SECTOR_NUM;
                }
                *dst++ = (uint32_t) DD->SECTOR_SIZE;
                *dst++ = (uint32_t) DD->SECTOR_SIZE_FULL;
                *dst++ = (uint32_t) DD->SECTORS_IN_BLOCK;
                *dst++ = *((uint32_t *) (cmp));
                usb_debug_tx_data((uint32_t) (sdram), 10 * 4);
                state = STATE_WAIT;
            }
            break;

        case STATE_WAIT: {
            uint32_t type;
            size_t length;
            // if (current_sector != 0) {}
            if (usb_debug_rx_ready(&type, &length)) {
                usb_debug_rx_data((uint32_t) (sdram), length);
                state = STATE_WAIT_2;
            }
        }
            break;

        case STATE_WAIT_2:
            if (current_sector != 0) {
                uint8_t sector_size = DD->SECTOR_SIZE + 1;
                dst += ((sector_size * current_sector) / sizeof(io32_t));
                for (int i = 0; i < sector_size; i += 4) {
                    *buf++ = *dst++;
                }
                DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2);
                DD->SCR |= DD_SCR_BM_TRANSFER_DATA | DD_SCR_BM_READY;
                state = STATE_TRANSFER;
                current_sector += 1;
            } else if (!usb_debug_rx_busy()) {
                // uart_print("state wait 2\r\n");
                uint8_t sector_size = DD->SECTOR_SIZE + 1;
                for (int i = 0; i < sector_size; i += 4) {
                    // uint32_t datat = *dst++;
                    // *buf++ = ((datat>>24)&0xff) | // move byte 3 to byte 0
                    // ((datat<<8)&0xff0000) | // move byte 1 to byte 2
                    // ((datat>>8)&0xff00) | // move byte 2 to byte 1
                    // ((datat<<24)&0xff000000); // byte 0 to byte 3
                    *buf++ = *dst++;
                }
                // if (current_sector < 85) {
                DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2);
                DD->SCR |= DD_SCR_BM_TRANSFER_DATA | DD_SCR_BM_READY;
                // } else {
                    // uart_print("shouldn't be here");
                // }
                state = STATE_TRANSFER;
                current_sector += 1;
            }
            break;

        // case STATE_TRANSFER:
        //     break;

        case STATE_STOP:
            // uart_print("state stop\r\n");
            state = STATE_IDLE;
            break;
    }
}


        // current_sector = 0;
        // DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2);
        // DD->SCR |= DD_SCR_BM_TRANSFER_DATA | DD_SCR_BM_READY;
        // if (current_sector < 85) {
        // } else if (current_sector == 85) {
        //     DD->SCR &= ~(DD_SCR_BM_TRANSFER_DATA);
        //     DD->SCR |= DD_SCR_BM_TRANSFER_C2;
        // } else if (current_sector == 85 + 4) {
        // } else {
        // }

        // current_sector += 1;

        // DD->SCR |= DD_SCR_BM_READY;


void usb_print (const char *text) {
    char *mmm = text;
    int length = 0;

    while (*mmm++ != 0) {
        ++length;
    }

    const char *dma = "DMA@";
    const char *cmp = "CMPH";

    io32_t *dst = &SDRAM;
    uint32_t *src = (uint32_t *) (text);

    *dst++ = *((uint32_t *)dma);
    *dst++ = 0x01 | (length & 0xFF) << 24;

    int i = 0;
    for (; i < length; i += 4) {
        *dst++ = *src++;
    }

    *dst++ = *((uint32_t *)cmp);

    usb_debug_tx_data(0, i + 12);
}
