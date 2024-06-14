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
#define RTC_ADDRESS_OSCTRIM         (0x08)
#define RTC_ADDRESS_SRAM_MAGIC      (0x20)
#define RTC_ADDRESS_SRAM_REGION     (0x24)
#define RTC_ADDRESS_SRAM_CENTURY    (0x25)
#define RTC_ADDRESS_SRAM_VERSION    (0x28)
#define RTC_ADDRESS_SRAM_SETTINGS   (0x2C)

#define RTC_RTCSEC_ST               (1 << 7)

#define RTC_RTCWKDAY_VBATEN         (1 << 3)
#define RTC_RTCWKDAY_OSCRUN         (1 << 5)

#define RTC_SETTINGS_VERSION        (1)

#define RTC_TIME_REFRESH_PERIOD_MS  (500)

#define RTC_FROM_BCD(x)             ((((((x) >> 4) & 0xF) % 10) * 10) + (((x) & 0xF) % 10))
#define RTC_TO_BCD(x)               (((((x) / 10) % 10) << 4) | ((x) % 10))


typedef struct {
    struct {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t weekday;
        uint8_t day;
        uint8_t month;
        uint8_t year;
    } time;
    struct {
        uint8_t value;
        uint8_t last_year;
    } century;
} rtc_raw_time_t;


static rtc_real_time_t rtc_time = {
    .second     = 0x00,
    .minute     = 0x00,
    .hour       = 0x12,
    .weekday    = 0x01,
    .day        = 0x01,
    .month      = 0x06,
    .year       = 0x24,
    .century    = 0x01,
};
static uint8_t rtc_region = 0xFF;
static rtc_settings_t rtc_settings = {
    .led_enabled    = true,
};


static bool rtc_read (uint8_t address, uint8_t *data, uint8_t length) {
    if (hw_i2c_trx(RTC_I2C_ADDRESS, &address, 1, data, length) != I2C_OK) {
        led_blink_error(LED_ERROR_RTC);
        return true;
    }

    return false;
}

static bool rtc_write (uint8_t address, uint8_t *data, uint8_t length) {
    uint8_t buffer[16];
    buffer[0] = address;

    for (int i = 0; i < length; i++) {
        buffer[i + 1] = data[i];
    }

    if (hw_i2c_trx(RTC_I2C_ADDRESS, buffer, length + 1, NULL, 0) != I2C_OK) {
        led_blink_error(LED_ERROR_RTC);
        return true;
    }

    return false;
}

static void rtc_osc_stop (void) {
    uint8_t tmp;

    rtc_read(RTC_ADDRESS_RTCSEC, &tmp, sizeof(tmp));

    tmp &= ~(RTC_RTCSEC_ST);

    rtc_write(RTC_ADDRESS_RTCSEC, &tmp, sizeof(tmp));

    while ((!rtc_read(RTC_ADDRESS_RTCWKDAY, &tmp, 1)) && (tmp & RTC_RTCWKDAY_OSCRUN));
}

static void rtc_sanitize_raw_time (rtc_raw_time_t *raw) {
    raw->time.second &= 0b01111111;
    raw->time.minute &= 0b01111111;
    raw->time.hour &= 0b00111111;
    raw->time.weekday &= 0b00000111;
    raw->time.day &= 0b00111111;
    raw->time.month &= 0b00011111;
    if (raw->time.weekday == 0) {
        raw->time.weekday = 7;
    }
    raw->century.value &= 0b00000111;
}

static int8_t rtc_calculate_backwards_day_offset (uint8_t century, uint8_t year, uint8_t month, uint8_t day) {
    int8_t day_offset = ((century + 2) / 4);
    if (((century % 4) == 1) && ((year > 0x00) || (month > 0x02))) {
        day_offset += 1;
    }
    return (-day_offset);
}

static int8_t rtc_calculate_forwards_day_offset (uint8_t century, uint8_t year, uint8_t month, uint8_t day) {
    int8_t day_offset = ((century + 2) / 4);
    if (((century % 4) == 1) && ((year > 0x00) || (month > 0x02) || ((month == 0x02) && (RTC_FROM_BCD(day) >= (29 - day_offset))))) {
        day_offset += 1;
    }
    return day_offset;
}

static uint8_t rtc_get_days_in_month (uint8_t century, uint8_t year, uint8_t month) {
    const uint8_t DAYS_IN_MONTH[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month == 0) {
        month = 12;
    } else if (month > 12) {
        month = (((month - 1) % 12) + 1);
    }

    uint8_t days = DAYS_IN_MONTH[month - 1];

    if (month == 2) {
        if (((century % 4) == 1) || ((year > 0) && ((year % 4) == 0))) {
            days += 1;
        }
    }

    return days;
}

static void rtc_adjust_date (uint8_t *century, uint8_t *year, uint8_t *month, uint8_t *day, int8_t day_offset) {
    if (day_offset == 0) {
        return;
    }

    int8_t dec_century = RTC_FROM_BCD(*century);
    int8_t dec_year = RTC_FROM_BCD(*year);
    int8_t dec_month = RTC_FROM_BCD(*month);
    int8_t dec_day = RTC_FROM_BCD(*day);

    int8_t century_offset = 0;
    int8_t year_offset = 0;
    int8_t month_offset = 0;

    dec_day += day_offset;
    if (dec_day < 1) {
        month_offset = -1;
    } else {
        uint8_t current_month_days = rtc_get_days_in_month(dec_century, dec_year, dec_month);
        if (dec_day > current_month_days) {
            dec_day %= current_month_days;
            month_offset = 1;
        }
    }

    if (month_offset != 0) {
        dec_month += month_offset;
        if (dec_month < 1) {
            dec_month = 12;
            year_offset = -1;
        } else if (dec_month > 12) {
            dec_month = 1;
            year_offset = 1;
        }
        *month = RTC_TO_BCD(dec_month);
    }

    if (year_offset != 0) {
        dec_year += year_offset;
        if (dec_year < 0) {
            dec_year = 99;
            century_offset = -1;
        } else if (dec_year > 99) {
            dec_year = 0;
            century_offset = 1;
        }
        *year = RTC_TO_BCD(dec_year);
    }

    if (century_offset != 0) {
        dec_century += century_offset;
        if (dec_century < 0) {
            dec_century = 7;
        } else if (dec_century > 7) {
            dec_century = 0;
        }
        *century = RTC_TO_BCD(dec_century);
    }

    if (day_offset != 0) {
        if (dec_day <= 0) {
            dec_day = (rtc_get_days_in_month(dec_century, dec_year, dec_month) + dec_day);
        }
        *day = RTC_TO_BCD(dec_day);
    }
}

static void rtc_raw_to_real (rtc_raw_time_t *raw, rtc_real_time_t *real) {
    real->century = raw->century.value;
    real->year = raw->time.year;
    real->month = raw->time.month;
    real->day = raw->time.day;
    real->weekday = raw->time.weekday;
    real->hour = raw->time.hour;
    real->minute = raw->time.minute;
    real->second = raw->time.second;

    int8_t day_offset = rtc_calculate_backwards_day_offset(real->century, real->year, real->month, real->day);

    rtc_adjust_date(&real->century, &real->year, &real->month, &real->day, day_offset);
}

static void rtc_real_to_raw (rtc_raw_time_t *raw, rtc_real_time_t *real) {
    raw->century.value = real->century;
    raw->time.year = real->year;
    raw->time.month = real->month;
    raw->time.day = real->day;
    raw->time.weekday = real->weekday;
    raw->time.hour = real->hour;
    raw->time.minute = real->minute;
    raw->time.second = real->second;

    int8_t day_offset = rtc_calculate_forwards_day_offset(raw->century.value, raw->time.year, raw->time.month, raw->time.day);

    rtc_adjust_date(&raw->century.value, &raw->time.year, &raw->time.month, &raw->time.day, day_offset);
}

static void rtc_read_time (void) {
    rtc_raw_time_t raw;
    bool update_raw_century = false;

    if (rtc_read(RTC_ADDRESS_RTCSEC, (uint8_t *) (&raw.time), sizeof(raw.time))) {
        return;
    }

    if (rtc_read(RTC_ADDRESS_SRAM_CENTURY, (uint8_t *) (&raw.century), sizeof(raw.century))) {
        return;
    }

    rtc_sanitize_raw_time(&raw);

    if (raw.time.year < raw.century.last_year) {
        raw.century.value += 1;
        update_raw_century = true;
    }

    if (raw.time.year != raw.century.last_year) {
        raw.century.last_year = raw.time.year;
        update_raw_century = true;
    }

    if (update_raw_century) {
        rtc_write(RTC_ADDRESS_SRAM_CENTURY, (uint8_t *) (&raw.century), sizeof(raw.century));
    }

    rtc_raw_to_real(&raw, &rtc_time);
}

static void rtc_write_time (void) {
    rtc_raw_time_t raw;
    uint8_t raw_regs[7];

    rtc_real_to_raw(&raw, &rtc_time);

    rtc_sanitize_raw_time(&raw);

    raw_regs[0] = (raw.time.second | RTC_RTCSEC_ST);
    raw_regs[1] = raw.time.minute;
    raw_regs[2] = raw.time.hour;
    raw_regs[3] = (raw.time.weekday | RTC_RTCWKDAY_VBATEN);
    raw_regs[4] = raw.time.day;
    raw_regs[5] = raw.time.month;
    raw_regs[6] = raw.time.year;

    raw.century.last_year = raw.time.year;

    rtc_osc_stop();

    rtc_write(RTC_ADDRESS_SRAM_CENTURY, (uint8_t *) (&raw.century), sizeof(raw.century));
    rtc_write(RTC_ADDRESS_RTCMIN, &raw_regs[1], sizeof(raw_regs) - sizeof(raw_regs[0]));
    rtc_write(RTC_ADDRESS_RTCSEC, &raw_regs[0], sizeof(raw_regs[0]));
}

static void rtc_read_region (void) {
    rtc_read(RTC_ADDRESS_SRAM_REGION, &rtc_region, sizeof(rtc_region));
}

static void rtc_write_region (void) {
    rtc_write(RTC_ADDRESS_SRAM_REGION, &rtc_region, sizeof(rtc_region));
}

static void rtc_read_settings (void) {
    rtc_read(RTC_ADDRESS_SRAM_SETTINGS, (uint8_t *) (&rtc_settings), sizeof(rtc_settings));
}

static void rtc_write_settings (void) {
    rtc_write(RTC_ADDRESS_SRAM_SETTINGS, (uint8_t *) (&rtc_settings), sizeof(rtc_settings));
}

static void rtc_read_joybus_time (void) {
    uint32_t time[2];

    time[0] = fpga_reg_get(REG_RTC_TIME_0);
    time[1] = fpga_reg_get(REG_RTC_TIME_1);

    rtc_time.weekday = ((time[0] >> 24) & 0xFF) + 1;
    rtc_time.hour = ((time[0] >> 16) & 0xFF);
    rtc_time.minute = ((time[0] >> 8) & 0xFF);
    rtc_time.second = ((time[0] >> 0) & 0xFF);
    rtc_time.century = ((time[1] >> 24) & 0xFF);
    rtc_time.year = ((time[1] >> 16) & 0xFF);
    rtc_time.month = ((time[1] >> 8) & 0xFF);
    rtc_time.day = ((time[1] >> 0) & 0xFF);
}

static void rtc_write_joybus_time (void) {
    uint32_t time[2] = {(
        ((rtc_time.weekday - 1) << 24) |
        (rtc_time.hour << 16) |
        (rtc_time.minute << 8) |
        (rtc_time.second << 0)
    ), (
        (rtc_time.century << 24) |
        (rtc_time.year << 16) |
        (rtc_time.month << 8) |
        (rtc_time.day << 0)
    )};

    fpga_reg_set(REG_RTC_TIME_0, time[0]);
    fpga_reg_set(REG_RTC_TIME_1, time[1]);
}


void rtc_get_time (rtc_real_time_t *time) {
    time->second = rtc_time.second;
    time->minute = rtc_time.minute;
    time->hour = rtc_time.hour;
    time->weekday = rtc_time.weekday;
    time->day = rtc_time.day;
    time->month = rtc_time.month;
    time->year = rtc_time.year;
    time->century = rtc_time.century;
}

void rtc_set_time (rtc_real_time_t *time) {
    rtc_time.second = time->second;
    rtc_time.minute = time->minute;
    rtc_time.hour = time->hour;
    rtc_time.weekday = time->weekday;
    rtc_time.day = time->day;
    rtc_time.month = time->month;
    rtc_time.year = time->year;
    rtc_time.century = time->century;
    rtc_write_time();
    rtc_read_time();
}


uint8_t rtc_get_region (void) {
    return rtc_region;
}

void rtc_set_region (uint8_t region) {
    rtc_region = region;
    rtc_write_region();
}


rtc_settings_t *rtc_get_settings (void) {
    return (&rtc_settings);
}

void rtc_save_settings (void) {
    rtc_write_settings();
}


void rtc_init (void) {
    bool uninitialized = false;
    const char *magic = "RTC1";
    uint8_t buffer[sizeof(magic)];
    uint8_t osc_trim = 0;
    uint32_t settings_version;

    for (int i = 0; i < sizeof(buffer) / sizeof(buffer[0]); i++) {
        buffer[i] = 0;
    }

    rtc_read(RTC_ADDRESS_SRAM_MAGIC, buffer, sizeof(buffer));
    rtc_read(RTC_ADDRESS_SRAM_VERSION, (uint8_t *) (&settings_version), sizeof(settings_version));

    for (int i = 0; i < sizeof(magic); i++) {
        if (buffer[i] != magic[i]) {
            uninitialized = true;
            break;
        }
    }

    if (uninitialized) {
        rtc_write(RTC_ADDRESS_OSCTRIM, &osc_trim, sizeof(osc_trim));
        rtc_write_time();
        rtc_write_region();
        rtc_write(RTC_ADDRESS_SRAM_MAGIC, (uint8_t *) (magic), sizeof(magic));
    }

    if (uninitialized || (settings_version != RTC_SETTINGS_VERSION)) {
        settings_version = RTC_SETTINGS_VERSION;
        rtc_write_settings();
        rtc_write(RTC_ADDRESS_SRAM_VERSION, (uint8_t *) (&settings_version), sizeof(settings_version));
    }

    rtc_read_time();
    rtc_read_region();
    rtc_read_settings();

    timer_countdown_start(TIMER_ID_RTC, RTC_TIME_REFRESH_PERIOD_MS);
}


void rtc_process (void) {
    uint32_t scr = fpga_reg_get(REG_RTC_SCR);

    if ((scr & RTC_SCR_PENDING) && ((scr & RTC_SCR_MAGIC_MASK) == RTC_SCR_MAGIC)) {
        rtc_read_joybus_time();
        rtc_write_time();
        rtc_read_time();
        rtc_write_joybus_time();
        fpga_reg_set(REG_RTC_SCR, RTC_SCR_DONE);
    }

    if (timer_countdown_elapsed(TIMER_ID_RTC)) {
        timer_countdown_start(TIMER_ID_RTC, RTC_TIME_REFRESH_PERIOD_MS);
        rtc_read_time();
        rtc_write_joybus_time();
    }
}
