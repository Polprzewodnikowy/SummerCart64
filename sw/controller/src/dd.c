#include "dd.h"
#include "fpga.h"
#include "led.h"
#include "rtc.h"
#include "sd.h"
#include "timer.h"
#include "usb.h"


#define DD_SECTOR_MAX_SIZE          (232)
#define DD_BLOCK_DATA_SECTORS_NUM   (85)
#define DD_BLOCK_BUFFER_SIZE        (ALIGN(DD_SECTOR_MAX_SIZE * DD_BLOCK_DATA_SECTORS_NUM, SD_SECTOR_SIZE) + SD_SECTOR_SIZE)
#define DD_BLOCK_BUFFER_ADDRESS     (0x03BC0000UL - DD_BLOCK_BUFFER_SIZE)
#define DD_SECTOR_BUFFER_ADDRESS    (0x05002800UL)
#define DD_SD_SECTOR_TABLE_SIZE     (DD_BLOCK_BUFFER_SIZE / SD_SECTOR_SIZE)
#define DD_SD_MAX_DISKS             (4)

#define DD_DRIVE_ID_RETAIL          (0x0003)
#define DD_DRIVE_ID_DEVELOPMENT     (0x0004)
#define DD_VERSION_RETAIL           (0x0114)

#define DD_SPIN_UP_TIME_MS          (2000)

#define DD_THB_UNMAPPED             (0xFFFFFFFF)
#define DD_THB_WRITABLE_FLAG        (1 << 31)


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
    STATE_BLOCK_READ_WAIT,
    STATE_SECTOR_READ,
    STATE_SECTOR_WRITE,
    STATE_BLOCK_WRITE,
    STATE_BLOCK_WRITE_WAIT,
    STATE_NEXT_BLOCK,
};

typedef union {
    uint32_t full;
    struct {
        uint8_t sector_num;
        uint8_t sector_size;
        uint8_t sector_size_full;
        uint8_t sectors_in_block;
    };
} sector_info_t;

typedef struct {
    uint32_t thb_table_address;
    uint32_t sector_table_address;
} sd_disk_info_t;

struct process {
    enum state state;
    rtc_time_t time;
    bool disk_spinning;
    bool cmd_response_delayed;
    bool bm_running;
    bool transfer_mode;
    bool full_track_transfer;
    bool starting_block;
    uint16_t head_track;
    uint8_t current_sector;
    sector_info_t sector_info;
    bool block_ready;
    bool block_valid;
    uint32_t block_offset;
    dd_drive_type_t drive_type;
    bool sd_mode;
    uint8_t sd_current_disk;
    sd_disk_info_t sd_disk_info[DD_SD_MAX_DISKS];
};


static struct process p;


static uint16_t dd_track_head_block (void) {
    uint16_t track = ((p.head_track & DD_TRACK_MASK) << 2);
    uint16_t head = (((p.head_track & DD_HEAD_MASK) ? 1 : 0) << 1);
    uint16_t block = (p.starting_block ? 1 : 0);
    return (track | head | block);
}

static uint32_t dd_fill_sd_sector_table (uint32_t index, uint32_t *sector_table, bool is_write) {
    uint32_t tmp;
    sd_disk_info_t info = p.sd_disk_info[p.sd_current_disk];
    if (info.thb_table_address == 0xFFFFFFFF) {
        return 0;
    }
    uint32_t thb_entry_address = (info.thb_table_address + (index * sizeof(uint32_t)));
    fpga_mem_read(thb_entry_address, sizeof(uint32_t), (uint8_t *) (&tmp));
    uint32_t start_offset = SWAP32(tmp);
    if (start_offset == DD_THB_UNMAPPED) {
        return 0;
    }
    if (is_write && ((start_offset & DD_THB_WRITABLE_FLAG) == 0)) {
        return 0;
    }
    start_offset &= ~(DD_THB_WRITABLE_FLAG);
    p.block_offset = (start_offset % SD_SECTOR_SIZE);
    uint32_t block_length = ((p.sector_info.sector_size + 1) * DD_BLOCK_DATA_SECTORS_NUM);
    uint32_t end_offset = ((start_offset + block_length) - 1);
    uint32_t starting_sector = (start_offset / SD_SECTOR_SIZE);
    uint32_t sectors = (1 + ((end_offset / SD_SECTOR_SIZE) - starting_sector));
    for (int i = 0; i < sectors; i++) {
        uint32_t sector_entry_address = info.sector_table_address + ((starting_sector + i) * sizeof(uint32_t));
        fpga_mem_read(sector_entry_address, sizeof(uint32_t), (uint8_t *) (&tmp));
        sector_table[i] = SWAP32(tmp);
    }
    return sectors;
}

static bool dd_block_read_request (void) {
    uint16_t index = dd_track_head_block();
    uint32_t buffer_address = DD_BLOCK_BUFFER_ADDRESS;
    if (p.sd_mode) {
        uint32_t sector_table[DD_SD_SECTOR_TABLE_SIZE];
        uint32_t sectors = dd_fill_sd_sector_table(index, sector_table, false);
        bool error = sd_optimize_sectors(buffer_address, sector_table, sectors, sd_read_sectors);
        dd_set_block_ready(!error);
    } else {
        usb_tx_info_t packet_info;
        usb_create_packet(&packet_info, PACKET_CMD_DD_REQUEST);
        packet_info.data_length = 12;
        packet_info.data[0] = 1;
        packet_info.data[1] = buffer_address;
        packet_info.data[2] = index;
        p.block_ready = false;
        p.block_offset = 0;
        return usb_enqueue_packet(&packet_info);
    }
    return true;
}

static bool dd_block_write_request (void) {
    uint32_t index = dd_track_head_block();
    uint32_t buffer_address = DD_BLOCK_BUFFER_ADDRESS;
    if (p.sd_mode) {
        uint32_t sector_table[DD_SD_SECTOR_TABLE_SIZE];
        uint32_t sectors = dd_fill_sd_sector_table(index, sector_table, true);
        bool error = sd_optimize_sectors(buffer_address, sector_table, sectors, sd_write_sectors);
        dd_set_block_ready(!error);
    } else {
        usb_tx_info_t packet_info;
        usb_create_packet(&packet_info, PACKET_CMD_DD_REQUEST);
        packet_info.data_length = 12;
        packet_info.data[0] = 2;
        packet_info.data[1] = buffer_address;
        packet_info.data[2] = index;
        packet_info.dma_length = (p.sector_info.sector_size + 1) * DD_BLOCK_DATA_SECTORS_NUM;
        packet_info.dma_address = buffer_address;
        p.block_ready = false;
        p.block_offset = 0;
        return usb_enqueue_packet(&packet_info);
    }
    return true;
}


void dd_set_block_ready (bool valid) {
    p.block_ready = true;
    p.block_valid = valid;
}

dd_drive_type_t dd_get_drive_type (void) {
    return p.drive_type;
}

bool dd_set_drive_type (dd_drive_type_t type) {
    switch (type) {
        case DD_DRIVE_TYPE_RETAIL:
            fpga_reg_set(REG_DD_DRIVE_ID, DD_DRIVE_ID_RETAIL);
            p.drive_type = type;
            break;

        case DD_DRIVE_TYPE_DEVELOPMENT:
            fpga_reg_set(REG_DD_DRIVE_ID, DD_DRIVE_ID_DEVELOPMENT);
            p.drive_type = type;
            break;

        default:
            return true;
    }
    return false;
}

dd_disk_state_t dd_get_disk_state (void) {
    uint32_t scr = fpga_reg_get(REG_DD_SCR);
    if (scr & DD_SCR_DISK_CHANGED) {
        return DD_DISK_STATE_CHANGED;
    }
    if (scr & DD_SCR_DISK_INSERTED) {
        return DD_DISK_STATE_INSERTED;
    }
    return DD_DISK_STATE_EJECTED;
}

bool dd_set_disk_state (dd_disk_state_t state) {
    if (state > DD_DISK_STATE_CHANGED) {
        return true;
    }
    uint32_t scr = fpga_reg_get(REG_DD_SCR);
    scr &= ~(DD_SCR_DISK_CHANGED | DD_SCR_DISK_INSERTED);
    switch (state) {
        case DD_DISK_STATE_EJECTED:
            break;

        case DD_DISK_STATE_INSERTED:
            scr |= DD_SCR_DISK_INSERTED;
            break;

        case DD_DISK_STATE_CHANGED:
            scr |= (DD_SCR_DISK_CHANGED | DD_SCR_DISK_INSERTED);
            break;
    }
    fpga_reg_set(REG_DD_SCR, scr);
    return false;
}

bool dd_get_sd_mode (void) {
    return p.sd_mode;
}

void dd_set_sd_mode (bool value) {
    p.sd_mode = value;
}

void dd_set_disk_mapping (uint32_t address, uint32_t length) {
    sd_disk_info_t info;
    length /= sizeof(info);
    p.sd_current_disk = 0;
    for (int i = 0; i < DD_SD_MAX_DISKS; i++) {
        if (i < length) {
            fpga_mem_read(address, sizeof(info), (uint8_t *) (&info));
            p.sd_disk_info[i].thb_table_address = (SWAP32(info.thb_table_address) & 0x1FFFFFFF);
            p.sd_disk_info[i].sector_table_address = (SWAP32(info.sector_table_address) & 0x1FFFFFFF);
            address += sizeof(info);
        } else {
            p.sd_disk_info[i].thb_table_address = 0xFFFFFFFF;
            p.sd_disk_info[i].sector_table_address = 0xFFFFFFFF;
        }
    }
}

void dd_handle_button (void) {
    led_blink_act();
    if (dd_get_disk_state() == DD_DISK_STATE_EJECTED) {
        dd_set_disk_state(DD_DISK_STATE_INSERTED);
    } else {
        dd_set_disk_state(DD_DISK_STATE_EJECTED);
        for (uint8_t i = 0; i < DD_SD_MAX_DISKS; i++) {
            uint8_t sd_next_disk = ((p.sd_current_disk + i + 1) % DD_SD_MAX_DISKS);
            if (p.sd_disk_info[sd_next_disk].thb_table_address != 0xFFFFFFFF) {
                p.sd_current_disk = sd_next_disk;
                break;
            }
        }
    }
}


void dd_init (void) {
    fpga_reg_set(REG_DD_SCR, 0);
    fpga_reg_set(REG_DD_HEAD_TRACK, 0);
    fpga_reg_set(REG_DD_DRIVE_ID, DD_DRIVE_ID_RETAIL);
    p.state = STATE_IDLE;
    p.cmd_response_delayed = false;
    p.disk_spinning = false;
    p.bm_running = false;
    p.drive_type = DD_DRIVE_TYPE_RETAIL;
    p.sd_mode = false;
    p.sd_current_disk = 0;
    dd_set_disk_mapping(0, 0);
}


void dd_process (void) {
    uint32_t starting_scr = fpga_reg_get(REG_DD_SCR);
    uint32_t scr = starting_scr;

    if (scr & DD_SCR_HARD_RESET) {
        p.state = STATE_IDLE;
        p.cmd_response_delayed = false;
        p.disk_spinning = false;
        p.bm_running = false;
        p.head_track = 0;
        scr &= ~(DD_SCR_DISK_CHANGED);
    }

    if (scr & DD_SCR_CMD_PENDING) {
        uint32_t cmd_data = fpga_reg_get(REG_DD_CMD_DATA);
        uint8_t cmd = (cmd_data >> 16) & 0xFF;
        uint16_t data = cmd_data & 0xFFFF;

        if (p.cmd_response_delayed) {
            if (timer_countdown_elapsed(TIMER_ID_DD)) {
                p.cmd_response_delayed = false;
                fpga_reg_set(REG_DD_HEAD_TRACK, DD_HEAD_TRACK_INDEX_LOCK | data);
                scr |= DD_SCR_CMD_READY;
            }
        } else if ((cmd == DD_CMD_SEEK_READ) || (cmd == DD_CMD_SEEK_WRITE)) {
            p.cmd_response_delayed = true;
            if (!p.disk_spinning) {
                p.disk_spinning = true;
                timer_countdown_start(TIMER_ID_DD, DD_SPIN_UP_TIME_MS);
            }
            fpga_reg_set(REG_DD_HEAD_TRACK, p.head_track & ~(DD_HEAD_TRACK_INDEX_LOCK));
            p.head_track = data & DD_HEAD_TRACK_MASK;
        } else {
            switch (cmd) {
                case DD_CMD_CLEAR_DISK_CHANGE:
                    scr &= ~(DD_SCR_DISK_CHANGED);
                    break;

                case DD_CMD_CLEAR_RESET_STATE:
                    scr &= ~(DD_SCR_DISK_CHANGED);
                    scr |= DD_SCR_HARD_RESET_CLEAR;
                    break;

                case DD_CMD_READ_VERSION:
                    data = DD_VERSION_RETAIL;
                    break;

                case DD_CMD_SET_DISK_TYPE:
                    break;

                case DD_CMD_REQUEST_STATUS:
                    data = 0;
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
                    data = (p.time.year << 8) | p.time.month;
                    break;

                case DD_CMD_GET_RTC_DAY_HOUR:
                    data = (p.time.day << 8) | p.time.hour;
                    break;

                case DD_CMD_GET_RTC_MINUTE_SECOND:
                    rtc_get_time(&p.time);
                    data = (p.time.minute << 8) | p.time.second;
                    break;

                case DD_CMD_READ_PROGRAM_VERSION:
                    data = 0;
                    break;

                default:
                    break;
            }

            fpga_reg_set(REG_DD_CMD_DATA, data);
            scr |= DD_SCR_CMD_READY;
        }
    }

    if (scr & DD_SCR_BM_STOP) {
        scr |= DD_SCR_BM_STOP_CLEAR;
        scr &= ~(DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
        p.bm_running = false;
    } else if (scr & DD_SCR_BM_START) {
        scr |= DD_SCR_BM_CLEAR | DD_SCR_BM_ACK_CLEAR | DD_SCR_BM_START_CLEAR;
        scr &= ~(DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
        p.state = STATE_START;
        p.bm_running = true;
        p.transfer_mode = (scr & DD_SCR_BM_TRANSFER_MODE);
        p.full_track_transfer = (scr & DD_SCR_BM_TRANSFER_BLOCKS);
        p.sector_info.full = fpga_reg_get(REG_DD_SECTOR_INFO);
        p.starting_block = (p.sector_info.sector_num == (p.sector_info.sectors_in_block + 1));
    } else if (p.bm_running) {
        if (scr & DD_SCR_BM_PENDING) {
            scr |= DD_SCR_BM_CLEAR;
            if (p.transfer_mode) {
                if (p.current_sector < (p.sector_info.sectors_in_block - 4)) {
                    p.state = STATE_SECTOR_READ;
                } else if (p.current_sector == (p.sector_info.sectors_in_block - 4)) {
                    p.current_sector += 1;
                    scr &= ~(DD_SCR_BM_TRANSFER_DATA);
                    scr |= DD_SCR_BM_READY;
                } else if (p.current_sector >= p.sector_info.sectors_in_block) {
                    p.state = STATE_NEXT_BLOCK;
                } else {
                }
            } else {
                if (p.current_sector < (p.sector_info.sectors_in_block - 4)) {
                    p.state = STATE_SECTOR_WRITE;
                }
            }
        }
        if (scr & DD_SCR_BM_ACK) {
            scr |= DD_SCR_BM_ACK_CLEAR;
            if (p.transfer_mode) {
                if ((p.current_sector <= (p.sector_info.sectors_in_block - 4))) {
                } else if (p.current_sector < p.sector_info.sectors_in_block) {
                    p.current_sector += 1;
                    scr |= DD_SCR_BM_READY;
                } else if (p.current_sector == p.sector_info.sectors_in_block) {
                    p.current_sector += 1;
                    scr |= DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_READY;
                }
            } else {
                if (p.current_sector == (p.sector_info.sectors_in_block - 4)) {
                    p.bm_running = false;
                }
            }
        }

        switch (p.state) {
            case STATE_IDLE:
                break;

            case STATE_START:
                p.current_sector = 0;
                if (dd_block_read_request()) {
                    p.state = STATE_BLOCK_READ_WAIT;
                }
                break;

            case STATE_BLOCK_READ_WAIT:
                if (p.block_ready) {
                    if (p.transfer_mode) {
                        if (p.block_valid) {
                            p.state = STATE_SECTOR_READ;
                            scr |= DD_SCR_BM_TRANSFER_DATA;
                        } else {
                            p.state = STATE_SECTOR_READ;
                            scr |= DD_SCR_BM_MICRO_ERROR;
                        }
                    } else {
                        p.state = STATE_IDLE;
                        if (p.block_valid) {
                            scr |= DD_SCR_BM_TRANSFER_DATA | DD_SCR_BM_READY;
                        } else {
                            scr |= DD_SCR_BM_MICRO_ERROR | DD_SCR_BM_READY;
                        }
                    }
                }
                break;

            case STATE_SECTOR_READ:
                fpga_mem_copy(
                    DD_BLOCK_BUFFER_ADDRESS + p.block_offset + (p.current_sector * (p.sector_info.sector_size + 1)),
                    DD_SECTOR_BUFFER_ADDRESS,
                    p.sector_info.sector_size + 1
                );
                p.state = STATE_IDLE;
                p.current_sector += 1;
                scr |= DD_SCR_BM_READY;
                break;

            case STATE_SECTOR_WRITE:
                fpga_mem_copy(
                    DD_SECTOR_BUFFER_ADDRESS,
                    DD_BLOCK_BUFFER_ADDRESS + p.block_offset + (p.current_sector * (p.sector_info.sector_size + 1)),
                    p.sector_info.sector_size + 1
                );
                p.current_sector += 1;
                if (p.current_sector < (p.sector_info.sectors_in_block - 4)) {
                    p.state = STATE_IDLE;
                    scr |= DD_SCR_BM_READY;
                } else {
                    p.state = STATE_BLOCK_WRITE;
                }
                break;

            case STATE_BLOCK_WRITE:
                if (dd_block_write_request()) {
                    p.state = STATE_BLOCK_WRITE_WAIT;
                }
                break;

            case STATE_BLOCK_WRITE_WAIT:
                if (p.block_ready) {
                    p.state = STATE_NEXT_BLOCK;
                }
                break;

            case STATE_NEXT_BLOCK:
                if (p.full_track_transfer) {
                    p.state = STATE_START;
                    p.full_track_transfer = false;
                    p.starting_block = !p.starting_block;
                    scr &= ~(DD_SCR_BM_TRANSFER_C2);
                } else {
                    if (p.transfer_mode) {
                        p.bm_running = false;
                    } else {
                        p.state = STATE_IDLE;
                        scr &= ~(DD_SCR_BM_TRANSFER_C2 | DD_SCR_BM_TRANSFER_DATA);
                        scr |= DD_SCR_BM_READY;
                    }
                }
                break;
        }
    }

    if (scr != starting_scr) {
        fpga_reg_set(REG_DD_SCR, scr);
    }
}
