#include "dd.h"
#include "rtc.h"
#include "usb.h"


#define DD_USER_SECTORS_PER_BLOCK   (85)

#define DD_BUFFERS_OFFSET           (SDRAM_BASE + 0x03BD0000UL)
#define DD_THB_TABLE_OFFSET         (DD_BUFFERS_OFFSET + 0x0000)
#define DD_USB_BUFFER_OFFSET        (DD_BUFFERS_OFFSET + 0x5000)
#define DD_BLOCK_BUFFER_OFFSET      (DD_USB_BUFFER_OFFSET + 0x10)

#define USB_DEBUG_ID_DD_BLOCK       (0xF5)

#define DD_DRIVE_ID_RETAIL          (0x0003)
#define DD_DRIVE_ID_DEVELOPMENT     (0x0004)
#define DD_VERSION_RETAIL           (0x0114)

#define DD_POWER_UP_DELAY_TICKS     (200000000UL)   // 2 s
#define DD_TRACK_SEEK_TIME_TICKS    (10000)         // 0.1 ms


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


enum state {
    STATE_IDLE,
    STATE_START,
    STATE_BLOCK_READ,
    STATE_BLOCK_READ_WAIT,
    STATE_SECTOR_READ,
    STATE_SECTOR_WRITE,
    STATE_BLOCK_WRITE,
    STATE_BLOCK_WRITE_WAIT,
    STATE_NEXT_BLOCK,
    STATE_STOP,
};

struct process {
    enum state state;
    uint32_t track_seek_time;
    uint32_t next_seek_time;
    bool power_up_delay;
    bool deffered_cmd_ready;
    bool bm_running;
    bool transfer_mode;
    bool full_track_transfer;
    bool starting_block;
    uint8_t current_sector;
    rtc_time_t time;
    io32_t *thb_table;
    io32_t *block_buffer;
    bool block_ready;
};

static struct process p;


static uint16_t dd_track_head_block (void) {
    uint32_t head_track = DD->HEAD_TRACK;
    uint16_t track = ((head_track & DD_TRACK_MASK) << 2);
    uint16_t head = (((head_track & DD_HEAD_MASK) ? 1 : 0) << 1);
    uint16_t block = (p.starting_block ? 1 : 0);

    return (track | head | block);
}

static bool dd_block_valid (void) {
    return (p.thb_table[dd_track_head_block()] != 0xFFFFFFFF);
}

static bool dd_block_request (void) {
    if (!usb_internal_debug_tx_ready()) {
        return false;
    }

    if (!(DD->SCR & DD_SCR_DISK_INSERTED)) {
        return true;
    }

    io32_t offset = p.thb_table[dd_track_head_block()];
    uint32_t length = ((DD->SECTOR_SIZE + 1) * DD_USER_SECTORS_PER_BLOCK);
    size_t transfer_length = 16;

    io32_t *dst = (io32_t *) (DD_USB_BUFFER_OFFSET);

    *dst++ = SWAP32(p.transfer_mode);
    *dst++ = SWAP32((uint32_t) (p.block_buffer));
    *dst++ = offset;
    *dst++ = SWAP32(length);

    if (!p.transfer_mode) {
        transfer_length += length;
    }

    usb_internal_debug_tx_data(INT_DBG_ID_DD_BLOCK, DD_USB_BUFFER_OFFSET, transfer_length);

    p.block_ready = false;

    return true;
}

static bool dd_block_ready (void) {
    if (!(DD->SCR & DD_SCR_DISK_INSERTED)) {
        return true;
    }

    if (p.transfer_mode) {
        return p.block_ready;
    } else {
        return usb_internal_debug_tx_ready();
    }
}

static void dd_sector_read (void) {
    io32_t *src = p.block_buffer;
    io32_t *dst = DD->SECTOR_BUFFER;

    uint8_t sector_size = ((DD->SECTOR_SIZE + 1) / sizeof(io32_t));

    src += (sector_size * p.current_sector);

    for (int i = 0; i < sector_size; i++) {
        *dst++ = *src++;
    }
}

static void dd_sector_write (void) {
    io32_t *src = DD->SECTOR_BUFFER;
    io32_t *dst = p.block_buffer;

    uint8_t sector_size = ((DD->SECTOR_SIZE + 1) / sizeof(io32_t));

    dst += (sector_size * p.current_sector);

    for (int i = 0; i < sector_size; i++) {
        *dst++ = *src++;
    }
}


void dd_set_disk_state (disk_state_t disk_state) {
    uint32_t scr = (DD->SCR & (~(DD_SCR_DISK_CHANGED | DD_SCR_DISK_INSERTED)));

    switch (disk_state) {
        case DD_DISK_EJECTED:
            break;
        case DD_DISK_INSERTED:
            scr |= DD_SCR_DISK_INSERTED;
            break;
        case DD_DISK_CHANGED:
            scr |= (DD_SCR_DISK_CHANGED | DD_SCR_DISK_INSERTED);
            break;
    }

    DD->SCR = scr;
}

disk_state_t dd_get_disk_state (void) {
    uint32_t scr = (DD->SCR & (DD_SCR_DISK_CHANGED | DD_SCR_DISK_INSERTED));

    if (scr == (DD_SCR_DISK_CHANGED | DD_SCR_DISK_INSERTED)) {
        return DD_DISK_CHANGED;
    } else if (scr == DD_SCR_DISK_INSERTED) {
        return DD_DISK_INSERTED;
    } else {
        return DD_DISK_EJECTED;
    }
}

void dd_set_drive_id (uint16_t id) {
    DD->DRIVE_ID = id;
}

uint16_t dd_get_drive_id (void) {
    return DD->DRIVE_ID;
}

void dd_set_block_ready (bool value) {
    p.block_ready = value;
}

void dd_set_thb_table_offset (uint32_t offset) {
    p.thb_table = (io32_t *) (SDRAM_BASE + offset);
}

uint32_t dd_get_thb_table_offset (void) {
    return (((uint32_t) (p.thb_table)) & 0x0FFFFFFF);
}


void dd_init (void) {
    DD->SCR = 0;
    DD->HEAD_TRACK = 0;
    DD->DRIVE_ID = DD_DRIVE_ID_RETAIL;
    p.state = STATE_IDLE;
    p.track_seek_time = DD_TRACK_SEEK_TIME_TICKS;
    p.power_up_delay = true;
    p.deffered_cmd_ready = false;
    p.bm_running = false;
    p.thb_table = (io32_t *) (DD_THB_TABLE_OFFSET);
    p.block_buffer = (io32_t *) (DD_BLOCK_BUFFER_OFFSET);
}


void process_dd (void) {
    uint32_t scr = DD->SCR;

    if (scr & DD_SCR_HARD_RESET) {
        DD->SCR &= ~(DD_SCR_DISK_CHANGED);
        DD->HEAD_TRACK = 0;
        p.state = STATE_IDLE;
        p.power_up_delay = true;
        p.deffered_cmd_ready = false;
        p.bm_running = false;
    }

    if (scr & DD_SCR_CMD_PENDING) {
        dd_cmd_t cmd = DD->CMD;
        uint16_t data = DD->DATA;

        DD->DATA = data;

        if (p.deffered_cmd_ready) {
            if (DD->SEEK_TIMER >= p.next_seek_time) {
                p.deffered_cmd_ready = false;
                DD->HEAD_TRACK = DD_HEAD_TRACK_INDEX_LOCK | data;
                DD->SCR |= DD_SCR_CMD_READY;
            }
        } else if ((cmd == DD_CMD_SEEK_READ) || (cmd == DD_CMD_SEEK_WRITE)) {
            int track_distance = abs((DD->HEAD_TRACK & DD_TRACK_MASK) - (data & DD_TRACK_MASK));
            if (p.power_up_delay) {
                p.power_up_delay = false;
                p.next_seek_time = DD_POWER_UP_DELAY_TICKS;
            } else {
                p.next_seek_time = (track_distance * p.track_seek_time);
            }
            p.deffered_cmd_ready = true;
            DD->HEAD_TRACK &= ~(DD_HEAD_TRACK_INDEX_LOCK);
            DD->SCR |= DD_SCR_SEEK_TIMER_RESET;
        } else {
            switch (cmd) {
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
                    break;

                case DD_CMD_REQUEST_STATUS:
                    DD->DATA = 0;
                    break;

                case DD_CMD_SET_RTC_YEAR_MONTH:
                    p.time.year = ((data >> 8) & 0xFF);
                    p.time.month = (data & 0xFF);
                    break;

                case DD_CMD_SET_RTC_DAY_HOUR:
                    p.time.day = ((data >> 8) & 0xFF);
                    p.time.hour = (data & 0xFF);
                    break;

                case DD_CMD_SET_RTC_MINUTE_SECOND:
                    p.time.minute = ((data >> 8) & 0xFF);
                    p.time.second = (data & 0xFF);
                    rtc_set_time(&p.time);
                    break;

                case DD_CMD_GET_RTC_YEAR_MONTH:
                    DD->DATA = ((p.time.year << 8) | p.time.month);
                    break;

                case DD_CMD_GET_RTC_DAY_HOUR:
                    DD->DATA = ((p.time.day << 8) | p.time.hour);
                    break;

                case DD_CMD_GET_RTC_MINUTE_SECOND:
                    p.time = *rtc_get_time();
                    DD->DATA = ((p.time.minute << 8) | p.time.second);
                    break;

                case DD_CMD_READ_PROGRAM_VERSION:
                    DD->DATA = 0;
                    break;

                default:
                    break;
            }

            DD->SCR |= DD_SCR_CMD_READY;
        }
    } else {
        if (scr & DD_SCR_BM_STOP) {
            DD->SCR |= DD_SCR_BM_STOP_CLEAR;
            DD->SCR &= ~(DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
            p.state = STATE_STOP;
        } else if (scr & DD_SCR_BM_START) {
            DD->SCR |= DD_SCR_BM_CLEAR | DD_SCR_BM_ACK_CLEAR | DD_SCR_BM_START_CLEAR;
            DD->SCR &= ~(DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
            p.state = STATE_START;
            p.transfer_mode = (scr & DD_SCR_BM_TRANSFER_MODE);
            p.full_track_transfer = (scr & DD_SCR_BM_TRANSFER_BLOCKS);
            p.starting_block = (DD->SECTOR_NUM == (DD->SECTORS_IN_BLOCK + 1));
        } else if (p.bm_running) {
            if (scr & DD_SCR_BM_PENDING) {
                DD->SCR |= DD_SCR_BM_CLEAR;
                if (p.transfer_mode) {
                    if (p.current_sector < (DD->SECTORS_IN_BLOCK - 4)) {
                        p.state = STATE_SECTOR_READ;
                    } else if (p.current_sector == (DD->SECTORS_IN_BLOCK - 4)) {
                        p.current_sector += 1;
                        DD->SCR &= ~(DD_SCR_BM_TRANSFER_DATA);
                        DD->SCR |= DD_SCR_BM_READY;
                    } else if (p.current_sector == DD->SECTORS_IN_BLOCK) {
                        p.state = STATE_NEXT_BLOCK;
                    }
                } else {
                    if (p.current_sector < (DD->SECTORS_IN_BLOCK - 4)) {
                        p.state = STATE_SECTOR_WRITE;
                    }
                }
            }
            if (scr & DD_SCR_BM_ACK) {
                DD->SCR |= DD_SCR_BM_ACK_CLEAR;
                if (p.transfer_mode) {
                    if ((p.current_sector <= (DD->SECTORS_IN_BLOCK - 4))) {
                    } else if (p.current_sector < (DD->SECTORS_IN_BLOCK - 1)) {
                        p.current_sector += 1;
                        DD->SCR |= DD_SCR_BM_READY;
                    } else if (p.current_sector < DD->SECTORS_IN_BLOCK) {
                        p.current_sector += 1;
                        DD->SCR |= DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_READY;
                    }
                } else {
                    if (p.current_sector == (DD->SECTORS_IN_BLOCK - 4)) {
                        p.state = STATE_STOP;
                    }
                }
            }
        }
    }

    switch (p.state) {
        case STATE_IDLE:
            break;

        case STATE_START:
            p.bm_running = true;
            p.current_sector = 0;
            if (p.transfer_mode) {
                if (dd_block_valid()) {
                    p.state = STATE_BLOCK_READ;
                    DD->SCR |= DD_SCR_BM_TRANSFER_DATA;
                } else {
                    p.state = STATE_SECTOR_READ;
                    DD->SCR |= DD_SCR_BM_MICRO_ERROR;
                }
            } else {
                p.state = STATE_IDLE;
                if (dd_block_valid()) {
                    DD->SCR |= DD_SCR_BM_TRANSFER_DATA | DD_SCR_BM_READY;
                } else {
                    DD->SCR |= DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_READY;
                }
            }
            break;

        case STATE_BLOCK_READ:
            if (dd_block_request()) {
                p.state = STATE_BLOCK_READ_WAIT;
            }
            break;

        case STATE_BLOCK_READ_WAIT:
            if (dd_block_ready()) {
                p.state = STATE_SECTOR_READ;
            }
            break;

        case STATE_SECTOR_READ:
            dd_sector_read();
            p.state = STATE_IDLE;
            p.current_sector += 1;
            DD->SCR |= DD_SCR_BM_READY;
            break;

        case STATE_SECTOR_WRITE:
            dd_sector_write();
            p.current_sector += 1;
            if (p.current_sector < (DD->SECTORS_IN_BLOCK - 4)) {
                p.state = STATE_IDLE;
                DD->SCR |= DD_SCR_BM_READY;
            } else {
                p.state = STATE_BLOCK_WRITE;
            }
            break;

        case STATE_BLOCK_WRITE:
            if (dd_block_request()) {
                p.state = STATE_BLOCK_WRITE_WAIT;
            }
            break;

        case STATE_BLOCK_WRITE_WAIT:
            if (dd_block_ready()) {
                p.state = STATE_NEXT_BLOCK;
            }
            break;

        case STATE_NEXT_BLOCK:
            if (p.full_track_transfer) {
                p.state = STATE_START;
                p.full_track_transfer = false;
                p.starting_block = !p.starting_block;
                DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2);
            } else {
                if (p.transfer_mode) {
                    p.state = STATE_STOP;
                } else {
                    p.state = STATE_IDLE;
                    DD->SCR &= ~(DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
                    DD->SCR |= DD_SCR_BM_READY;
                }
            }
            break;

        case STATE_STOP:
            p.state = STATE_IDLE;
            p.bm_running = false;
            break;
    }
}
