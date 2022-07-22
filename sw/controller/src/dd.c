#include <stdbool.h>
#include <stdint.h>
#include "fpga.h"
#include "rtc.h"


#define DD_DRIVE_ID_RETAIL          (0x0003)
#define DD_DRIVE_ID_DEVELOPMENT     (0x0004)
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


void dd_init (void) {
    fpga_reg_set(REG_DD_SCR, 0);
    fpga_reg_set(REG_DD_HEAD_TRACK, 0);
    fpga_reg_set(REG_DD_DRIVE_ID, DD_DRIVE_ID_RETAIL);
}


void dd_process (void) {
    uint32_t scr = fpga_reg_get(REG_DD_SCR);

    if (scr & DD_SCR_HARD_RESET) {
        fpga_reg_set(REG_DD_SCR, scr & ~(DD_SCR_DISK_CHANGED));
        fpga_reg_set(REG_DD_HEAD_TRACK, 0);
    }

    if (scr & DD_SCR_CMD_PENDING) {
        uint32_t cmd_data = fpga_reg_get(REG_DD_CMD_DATA);
        uint8_t cmd = (cmd_data >> 16) & 0xFF;
        uint16_t data = cmd_data & 0xFFFF;
        fpga_reg_set(REG_DD_CMD_DATA, data);

        switch (cmd) {
            case DD_CMD_CLEAR_DISK_CHANGE:
                fpga_reg_set(REG_DD_SCR, scr & ~(DD_SCR_DISK_CHANGED));
                break;

            case DD_CMD_CLEAR_RESET_STATE:
                fpga_reg_set(REG_DD_SCR, scr & ~(DD_SCR_DISK_CHANGED));
                fpga_reg_set(REG_DD_SCR, scr | DD_SCR_HARD_RESET_CLEAR);
                
                break;

            case DD_CMD_READ_VERSION:
                fpga_reg_set(REG_DD_CMD_DATA, DD_VERSION_RETAIL);
                break;

            case DD_CMD_SET_DISK_TYPE:
                break;

            case DD_CMD_REQUEST_STATUS:
                fpga_reg_set(REG_DD_CMD_DATA, 0);
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
                fpga_reg_set(REG_DD_CMD_DATA, (time.year << 8) | time.month);
                break;

            case DD_CMD_GET_RTC_DAY_HOUR:
                fpga_reg_set(REG_DD_CMD_DATA, (time.day << 8) | time.hour);
                break;

            case DD_CMD_GET_RTC_MINUTE_SECOND:
                rtc_get_time(&time);
                fpga_reg_set(REG_DD_CMD_DATA, (time.minute << 8) | time.second);
                break;

            case DD_CMD_READ_PROGRAM_VERSION:
                fpga_reg_set(REG_DD_CMD_DATA, 0);
                break;

            default:
                break;
        }

        fpga_reg_set(REG_DD_SCR, scr | DD_SCR_CMD_READY);
    }
}
