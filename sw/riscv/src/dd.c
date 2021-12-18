#include "dd.h"
#include "rtc.h"
#include "usb.h"


#define DD_DRIVE_ID_RETAIL          (0x0003)
#define DD_DRIVE_ID_DEVELOPMENT     (0x0004)
#define DD_VERSION_RETAIL           (0x0114)
#define DD_POWER_UP_DELAY_TICKS     (320000000UL)   // 3.2 s
#define DD_TRACK_SEEK_TIME_TICKS    (10000)         // 0.1 ms
#define DEFAULT_DD_BUFFER_OFFSET    (0x03BD8000UL)
#define USB_DEBUG_ID_DD_BLOCK       (0xF5)

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
    bool is_dev_disk;
    rtc_time_t time;
    io32_t *buffer;
};

static struct process p;

enum {
    READ_PHASE_IDLE,
    READ_PHASE_STARTED,
    READ_PHASE_WAIT,
} read_phase = READ_PHASE_IDLE;

static void dd_block_read (void) {
    read_phase = READ_PHASE_STARTED;

    io32_t *dst = p.buffer;

    const char *dma = "DMA@";
    const char *cmp = "CMPH";

    *dst++ = *((uint32_t *) (dma));
    *dst++ = (((2 * 4) << 24) | USB_DEBUG_ID_DD_BLOCK);
    *dst++ = p.transfer_mode;
    *dst++ = (((DD->SECTOR_SIZE + 1) << 24) | (p.starting_block << 16) | DD->HEAD_TRACK);
    *dst++ = *((uint32_t *) (cmp));

    usb_debug_tx_data((uint32_t) (p.buffer), (5 * 4));
}

static bool dd_block_read_done (void) {
    uint32_t type;
    size_t length;

    if (read_phase == READ_PHASE_STARTED) {
        if (usb_debug_rx_ready(&type, &length)) {
            usb_debug_rx_data((uint32_t) (p.buffer), length);
            read_phase = READ_PHASE_WAIT;
        }
        return false;
    } else if (read_phase == READ_PHASE_WAIT) {
        if (!usb_debug_rx_busy()) {
            read_phase = READ_PHASE_IDLE;
            return true;
        }
        return false;
    }

    return true;
}

static void dd_sector_read (void) {
    io32_t *src = p.buffer;
    io32_t *dst = DD->SECTOR_BUFFER;

    uint8_t sector_size = ((DD->SECTOR_SIZE + 1) / sizeof(io32_t));

    src += (sector_size * p.current_sector);

    for (int i = 0; i < sector_size; i++) {
        *dst++ = *src++;
    }
}

static void dd_sector_write (void) {
    io32_t *src = DD->SECTOR_BUFFER;
    io32_t *dst = p.buffer + 4;

    uint8_t sector_size = ((DD->SECTOR_SIZE + 1) / sizeof(io32_t));

    dst += (sector_size * p.current_sector);

    for (int i = 0; i < sector_size; i++) {
        *dst++ = *src++;
    }
}

static void dd_block_write (void) {
    io32_t *dst = p.buffer;

    const char *dma = "DMA@";
    const char *cmp = "CMPH";

    uint32_t block_length = ((DD->SECTOR_SIZE + 1) * 85);

    *dst++ = *((uint32_t *) (dma));
    *dst++ = (((2 * 4) << 24) | USB_DEBUG_ID_DD_BLOCK);
    *dst++ = p.transfer_mode;
    *dst++ = (((DD->SECTOR_SIZE + 1) << 24) | (p.starting_block << 16) | DD->HEAD_TRACK);
    dst += block_length / 4;
    *dst++ = *((uint32_t *) (cmp));
    usb_debug_tx_data((uint32_t) (p.buffer), ((5 * 4) + block_length));
}

static bool dd_block_write_done (void) {
    return usb_debug_tx_ready();
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

void dd_set_drive_type_development (bool value) {
    if (value) {
        DD->DRIVE_ID = DD_DRIVE_ID_DEVELOPMENT;
        p.is_dev_disk = true;
    } else {
        DD->DRIVE_ID = DD_DRIVE_ID_RETAIL;
        p.is_dev_disk = false;
    }
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
    p.is_dev_disk = false;
    p.buffer = (io32_t *) (SDRAM_BASE + DEFAULT_DD_BUFFER_OFFSET);
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
                    DD->DATA = (p.time.year << 8) | p.time.month;
                    break;

                case DD_CMD_GET_RTC_DAY_HOUR:
                    DD->DATA = (p.time.day << 8) | p.time.hour;
                    break;

                case DD_CMD_GET_RTC_MINUTE_SECOND:
                    p.time = *rtc_get_time();
                    DD->DATA = (p.time.minute << 8) | p.time.second;
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
                if (!p.is_dev_disk && ((DD->HEAD_TRACK & DD_HEAD_TRACK_MASK) == 6) && !p.starting_block) {
                    p.state = STATE_SECTOR_READ;
                    DD->SCR |= DD_SCR_BM_MICRO_ERROR;
                } else {
                    p.state = STATE_BLOCK_READ;
                    DD->SCR |= DD_SCR_BM_TRANSFER_DATA;
                }
            } else {
                p.state = STATE_IDLE;
                DD->SCR |= DD_SCR_BM_TRANSFER_DATA | DD_SCR_BM_READY;
            }
            break;

        case STATE_BLOCK_READ:
            if (dd_block_read_done()) {
                dd_block_read();
                p.state = STATE_BLOCK_READ_WAIT;
            }
            break;

        case STATE_BLOCK_READ_WAIT:
            if (dd_block_read_done()) {
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
            if (dd_block_write_done()) {
                dd_block_write();
                p.state = STATE_BLOCK_WRITE_WAIT;
            }
            break;

        case STATE_BLOCK_WRITE_WAIT:
            if (dd_block_write_done()) {
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
                    DD->SCR &= ~(DD_SCR_BM_TRANSFER_DATA);
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
