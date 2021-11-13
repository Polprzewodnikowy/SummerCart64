#include "rtc.h"
#include "i2c.h"


enum rtc_regs {
    RTCSEC,
    RTCMIN,
    RTCHOUR,
    RTCWKDAY,
    RTCDATE,
    RTCMTH,
    RTCYEAR,
};

#define RTC_I2C_ADDR    (0xDE)

#define RTCSEC_ST       (1 << 7)
#define RTCWKDAY_OSCRUN (1 << 5)
#define RTCWKDAY_VBAT   (1 << 3)


enum rtc_phase {
    RTC_PHASE_READ_START,
    RTC_PHASE_READ_READY,
    RTC_PHASE_STOP,
    RTC_PHASE_WAIT_STOP,
    RTC_PHASE_UPDATE,
    RTC_PHASE_START,
    RTC_PHASE_WAIT_START,
};

enum i2c_phase {
    I2C_PHASE_IDLE,
    I2C_PHASE_ADDR,
    I2C_PHASE_DATA,
    I2C_PHASE_READY,
};


struct process {
    enum rtc_phase rtc_phase;
    uint8_t data[7];
    bool running;
    rtc_time_t time;
    bool time_valid;
    bool new_time_valid;

    enum i2c_phase i2c_phase;
    bool i2c_pending;
    bool i2c_write;
    uint8_t i2c_address;
    uint8_t i2c_length;
    bool i2c_first_read_done;
};

static struct process p;


static const uint8_t rtc_regs_bit_mask[7] = {
    0b01111111,
    0b01111111,
    0b00111111,
    0b00000111,
    0b00111111,
    0b00011111,
    0b11111111
};

static void sanitize_time (uint8_t *data) {
    for (int i = 0; i < 7; i++) {
        data[i] &= rtc_regs_bit_mask[i];
    }
}


rtc_time_t *rtc_get_time (void) {
    return &p.time;
}

bool rtc_is_time_valid (void) {
    return p.time_valid;
}

bool rtc_is_time_running (void) {
    return p.running;
}

void rtc_set_time (rtc_time_t *time) {
    p.time.second = time->second;
    p.time.minute = time->minute;
    p.time.hour = time->hour;
    p.time.weekday = time->weekday;
    p.time.day = time->day;
    p.time.month = time->month;
    p.time.year = time->year;
    p.new_time_valid = true;
}


void rtc_init (void) {
    p.rtc_phase = RTC_PHASE_READ_START;
    p.running = false;
    p.time_valid = false;
    p.new_time_valid = false;

    p.i2c_phase = I2C_PHASE_IDLE;
    p.i2c_pending = false;
}


void process_rtc (void) {
    if (p.i2c_phase == I2C_PHASE_IDLE) {
        switch (p.rtc_phase) {
            case RTC_PHASE_READ_START:
                p.i2c_pending = true;
                p.i2c_write = false;
                p.i2c_address = RTCSEC;
                p.i2c_length = sizeof(p.data);
                p.rtc_phase = RTC_PHASE_READ_READY;
                break;

            case RTC_PHASE_READ_READY:
                p.time_valid = (!i2c_failed());
                if (p.new_time_valid) {
                    p.rtc_phase = RTC_PHASE_STOP;
                    break;
                } else if (p.time_valid) {
                    p.running = p.data[RTCSEC] & RTCSEC_ST;
                    sanitize_time(p.data);
                    p.time.second = p.data[RTCSEC];
                    p.time.minute = p.data[RTCMIN];
                    p.time.hour = p.data[RTCHOUR];
                    p.time.weekday = p.data[RTCWKDAY];
                    p.time.day = p.data[RTCDATE];
                    p.time.month = p.data[RTCMTH];
                    p.time.year = p.data[RTCYEAR];
                }
                p.rtc_phase = RTC_PHASE_READ_START;
                break;

            case RTC_PHASE_STOP:
                p.i2c_pending = true;
                p.i2c_write = true;
                p.i2c_length = 2;
                p.i2c_first_read_done = false;
                p.data[0] = RTCSEC;
                p.data[1] = 0x00;
                p.rtc_phase = RTC_PHASE_WAIT_STOP;
                break;

            case RTC_PHASE_WAIT_STOP:
                if (p.i2c_first_read_done) {
                    if (!(p.data[0] & RTCWKDAY_OSCRUN)) {
                        p.rtc_phase = RTC_PHASE_UPDATE;
                        break;
                    }
                }
                p.i2c_pending = true;
                p.i2c_write = false;
                p.i2c_address = RTCWKDAY;
                p.i2c_length = 1;
                p.i2c_first_read_done = true;
                break;

            case RTC_PHASE_UPDATE:
                sanitize_time((uint8_t *)(&p.time));
                p.i2c_pending = true;
                p.i2c_write = true;
                p.i2c_length = 7;
                p.data[0] = RTCMIN;
                p.data[1] = p.time.minute;
                p.data[2] = p.time.hour;
                p.data[3] = p.time.weekday | RTCWKDAY_VBAT;
                p.data[4] = p.time.day;
                p.data[5] = p.time.month;
                p.data[6] = p.time.year;
                p.rtc_phase = RTC_PHASE_START;
                break;

            case RTC_PHASE_START:
                p.i2c_pending = true;
                p.i2c_write = true;
                p.i2c_length = 2;
                p.i2c_first_read_done = false;
                p.data[0] = RTCSEC;
                p.data[1] = p.time.second | RTCSEC_ST;
                p.rtc_phase = RTC_PHASE_WAIT_START;
                break;

            case RTC_PHASE_WAIT_START:
                if (p.i2c_first_read_done) {
                    if (p.data[0] & RTCWKDAY_OSCRUN) {
                        p.new_time_valid = false;
                        p.rtc_phase = RTC_PHASE_READ_START;
                        break;
                    }
                }
                p.i2c_pending = true;
                p.i2c_write = false;
                p.i2c_address = RTCWKDAY;
                p.i2c_length = 1;
                p.i2c_first_read_done = true;
                break;
        }
    }

    if (!i2c_busy()) {
        switch (p.i2c_phase) {
            case I2C_PHASE_IDLE:
                if (p.i2c_pending) {
                    p.i2c_pending = false;
                    p.i2c_phase = p.i2c_write ? I2C_PHASE_DATA : I2C_PHASE_ADDR;
                }
                break;

            case I2C_PHASE_ADDR:
                i2c_trx(RTC_I2C_ADDR, &p.i2c_address, 1, true, false);
                p.i2c_phase = I2C_PHASE_DATA;
                break;

            case I2C_PHASE_DATA:
                i2c_trx(RTC_I2C_ADDR, p.data, p.i2c_length, p.i2c_write, true);
                p.i2c_phase = I2C_PHASE_READY;
                break;

            case I2C_PHASE_READY:
                p.i2c_phase = I2C_PHASE_IDLE;
                break;
        }
    }
}
