#include <string.h>
#include "fpga.h"
#include "hw.h"
#include "led.h"
#include "rtc.h"
#include "timer.h"


#define RTC_I2C_ADDRESS             (0xDE)

#define RTC_ADDRESS_RTCSEC          (0x00)
#define RTC_ADDRESS_RTCMIN          (0x01)
#define RTC_ADDRESS_RTCHOUR         (0x02)
#define RTC_ADDRESS_RTCWKDAY        (0x03)
#define RTC_ADDRESS_RTCDATE         (0x04)
#define RTC_ADDRESS_RTCMTH          (0x05)
#define RTC_ADDRESS_RTCYEAR         (0x06)
#define RTC_ADDRESS_CONTROL         (0x07)
#define RTC_ADDRESS_OSCTRIM         (0x08)
#define RTC_ADDRESS_SRAM_MAGIC      (0x20)
#define RTC_ADDRESS_SRAM_REGION     (0x24)
#define RTC_ADDRESS_SRAM_VERSION    (0x28)
#define RTC_ADDRESS_SRAM_SETTINGS   (0x2C)

#define RTC_RTCSEC_ST               (1 << 7)

#define RTC_RTCWKDAY_VBATEN         (1 << 3)
#define RTC_RTCWKDAY_OSCRUN         (1 << 5)

#define RTC_SETTINGS_VERSION        (1)

#define RTC_TIME_REFRESH_PERIOD_MS  (500)


static rtc_time_t rtc_time = {
    .second     = 0x00,
    .minute     = 0x00,
    .hour       = 0x12,
    .weekday    = 0x02,
    .day        = 0x01,
    .month      = 0x03,
    .year       = 0x22
};
static bool rtc_time_pending = false;

static uint8_t rtc_region = 0xFF;
static bool rtc_region_pending = false;

static rtc_settings_t rtc_settings = {
    .led_enabled    = true,
};
static bool rtc_settings_pending = false;

static const uint8_t rtc_regs_bit_mask[7] = {
    0b01111111,
    0b01111111,
    0b00111111,
    0b00000111,
    0b00111111,
    0b00011111,
    0b11111111
};


static void rtc_read (uint8_t address, uint8_t *data, uint8_t length) {
    uint8_t tmp = address;
    if (hw_i2c_trx(RTC_I2C_ADDRESS, &tmp, 1, data, length) != I2C_OK) {
        led_blink_error(LED_ERROR_RTC);
    }
}

static void rtc_write (uint8_t address, uint8_t *data, uint8_t length) {
    uint8_t tmp[16];
    tmp[0] = address;
    for (int i = 0; i < length; i++) {
        tmp[i + 1] = data[i];
    }
    if (hw_i2c_trx(RTC_I2C_ADDRESS, tmp, length + 1, NULL, 0) != I2C_OK) {
        led_blink_error(LED_ERROR_RTC);
    }
}

static void rtc_sanitize_time (uint8_t *regs) {
    for (int i = 0; i < 7; i++) {
        regs[i] &= rtc_regs_bit_mask[i];
    }
}

static void rtc_osc_stop (void) {
    uint8_t tmp = 0x00;

    rtc_write(RTC_ADDRESS_RTCSEC, &tmp, 1);

    do {
        rtc_read(RTC_ADDRESS_RTCWKDAY, &tmp, 1);
    } while (tmp & RTC_RTCWKDAY_OSCRUN);
}

static void rtc_read_time (void) {
    uint8_t regs[7];

    rtc_read(RTC_ADDRESS_RTCSEC, regs, 7);

    rtc_sanitize_time(regs);

    rtc_time.second = regs[0];
    rtc_time.minute = regs[1];
    rtc_time.hour = regs[2];
    rtc_time.weekday = regs[3];
    rtc_time.day = regs[4];
    rtc_time.month = regs[5];
    rtc_time.year = regs[6];
}

static void rtc_write_time (void) {
    uint8_t regs[7];

    rtc_osc_stop();

    regs[0] = rtc_time.second;
    regs[1] = rtc_time.minute;
    regs[2] = rtc_time.hour;
    regs[3] = rtc_time.weekday;
    regs[4] = rtc_time.day;
    regs[5] = rtc_time.month;
    regs[6] = rtc_time.year;

    rtc_sanitize_time(regs);

    regs[0] |= RTC_RTCSEC_ST;
    regs[3] |= RTC_RTCWKDAY_VBATEN;

    rtc_write(RTC_ADDRESS_RTCMIN, &regs[1], 6);
    rtc_write(RTC_ADDRESS_RTCSEC, &regs[0], 1);
}

static void rtc_read_region (void) {
    rtc_read(RTC_ADDRESS_SRAM_REGION, &rtc_region, 1);
}

static void rtc_write_region (void) {
    rtc_write(RTC_ADDRESS_SRAM_REGION, &rtc_region, 1);
}

static void rtc_read_settings (void) {
    rtc_read(RTC_ADDRESS_SRAM_SETTINGS, (uint8_t *) (&rtc_settings), sizeof(rtc_settings));
}

static void rtc_write_settings (void) {
    rtc_write(RTC_ADDRESS_SRAM_SETTINGS, (uint8_t *) (&rtc_settings), sizeof(rtc_settings));
}


void rtc_get_time (rtc_time_t *time) {
    time->second = rtc_time.second;
    time->minute = rtc_time.minute;
    time->hour = rtc_time.hour;
    time->weekday = rtc_time.weekday;
    time->day = rtc_time.day;
    time->month = rtc_time.month;
    time->year = rtc_time.year;
}

void rtc_set_time (rtc_time_t *time) {
    rtc_time.second = time->second;
    rtc_time.minute = time->minute;
    rtc_time.hour = time->hour;
    rtc_time.weekday = time->weekday;
    rtc_time.day = time->day;
    rtc_time.month = time->month;
    rtc_time.year = time->year;
    rtc_time_pending = true;
}


uint8_t rtc_get_region (void) {
    return rtc_region;
}

void rtc_set_region (uint8_t region) {
    rtc_region = region;
    rtc_region_pending = true;
}


rtc_settings_t *rtc_get_settings (void) {
    return (&rtc_settings);
}

void rtc_save_settings (void) {
    rtc_settings_pending = true;
}


void rtc_init (void) {
    bool uninitialized = false;
    const char *magic = "SC64";
    uint8_t buffer[4];
    uint32_t settings_version;

    memset(buffer, 0, 4);
    rtc_read(RTC_ADDRESS_SRAM_MAGIC, buffer, 4);

    for (int i = 0; i < 4; i++) {
        if (buffer[i] != magic[i]) {
            uninitialized = true;
            break;
        }
    }

    if (uninitialized) {
        buffer[0] = 0;
        rtc_write(RTC_ADDRESS_SRAM_MAGIC, (uint8_t *) (magic), 4);
        rtc_write(RTC_ADDRESS_CONTROL, buffer, 1);
        rtc_write(RTC_ADDRESS_OSCTRIM, buffer, 1);
        rtc_write_time();
        rtc_write_region();
    }

    rtc_read(RTC_ADDRESS_SRAM_VERSION, (uint8_t *) (&settings_version), 4);

    if (uninitialized || (settings_version != RTC_SETTINGS_VERSION)) {
        settings_version = RTC_SETTINGS_VERSION;
        rtc_write(RTC_ADDRESS_SRAM_VERSION, (uint8_t *) (&settings_version), 4);
        rtc_write_settings();
    }

    rtc_read_time();
    rtc_read_region();
    rtc_read_settings();

    timer_countdown_start(TIMER_ID_RTC, RTC_TIME_REFRESH_PERIOD_MS);
}


void rtc_process (void) {
    uint32_t scr = fpga_reg_get(REG_RTC_SCR);

    if ((scr & RTC_SCR_PENDING) && ((scr & RTC_SCR_MAGIC_MASK) == RTC_SCR_MAGIC)) {
        uint32_t data[2];

        data[0] = fpga_reg_get(REG_RTC_TIME_0);
        data[1] = fpga_reg_get(REG_RTC_TIME_1);

        rtc_time.weekday = ((data[0] >> 24) & 0xFF) + 1;
        rtc_time.hour = ((data[0] >> 16) & 0xFF);
        rtc_time.minute = ((data[0] >> 8) & 0xFF);
        rtc_time.second = ((data[0] >> 0) & 0xFF);
        rtc_time.year = ((data[1] >> 16) & 0xFF);
        rtc_time.month = ((data[1] >> 8) & 0xFF);
        rtc_time.day = ((data[1] >> 0) & 0xFF);
        rtc_time_pending = true;

        fpga_reg_set(REG_RTC_TIME_0, data[0]);
        fpga_reg_set(REG_RTC_TIME_1, data[1]);
        fpga_reg_set(REG_RTC_SCR, RTC_SCR_DONE);
    }

    if (rtc_time_pending) {
        rtc_time_pending = false;
        rtc_write_time();
    }

    if (rtc_region_pending) {
        rtc_region_pending = false;
        rtc_write_region();
    }

    if (rtc_settings_pending) {
        rtc_settings_pending = false;
        rtc_write_settings();
    }

    if (timer_countdown_elapsed(TIMER_ID_RTC)) {
        timer_countdown_start(TIMER_ID_RTC, RTC_TIME_REFRESH_PERIOD_MS);

        rtc_read_time();

        uint32_t data[2];

        data[0] = (
            ((rtc_time.weekday - 1) << 24) |
            (rtc_time.hour << 16) |
            (rtc_time.minute << 8) |
            (rtc_time.second << 0)
        );
        data[1] = (
            (rtc_time.year << 16) |
            (rtc_time.month << 8) |
            (rtc_time.day << 0)
        );

        fpga_reg_set(REG_RTC_TIME_0, data[0]);
        fpga_reg_set(REG_RTC_TIME_1, data[1]);
    }
}
