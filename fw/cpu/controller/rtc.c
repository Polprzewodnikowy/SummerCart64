#include "sys.h"
#include "rtc.h"


static const uint8_t rtc_bit_mask[7] = {
    0b01111111,
    0b01111111,
    0b00111111,
    0b00000111,
    0b00111111,
    0b00011111,
    0b11111111
};


static uint8_t i2c_is_busy (void) {
    return (I2C_SR & I2C_SR_BUSY);
}

static void i2c_wait_busy (void) {
    while (i2c_is_busy());
}

static uint8_t i2c_has_ack (void) {
    return (I2C_SR & I2C_SR_ACK);
}

static void i2c_start (void) {
    i2c_wait_busy();
    I2C_SR = I2C_SR_START;
}

static void i2c_stop (void) {
    i2c_wait_busy();
    I2C_SR = I2C_SR_STOP;
}

static uint8_t i2c_write (uint8_t data) {
    i2c_wait_busy();
    I2C_SR = 0;
    I2C_DR = data;
    i2c_wait_busy();
    return i2c_has_ack();
}

static void i2c_read (uint8_t *data, uint8_t cfg) {
    i2c_wait_busy();
    I2C_SR = cfg;
    I2C_DR = 0xFF;
    i2c_wait_busy();
    *data = I2C_DR;
}

static uint8_t i2c_tx (uint8_t address, uint8_t *data, size_t length) {
    uint8_t result = 0;

    i2c_start();
    result |= i2c_write(RTC_ADDR);
    result |= i2c_write(address);
    for (size_t i = 0; i < length; i++) {
        result |= i2c_write(*data++);
    }
    i2c_stop();

    return result;
}

static uint8_t i2c_rx (uint8_t address, uint8_t *data, size_t length) {
    uint8_t result = 0;

    i2c_start();
    result |= i2c_write(RTC_ADDR);
    result |= i2c_write(address);
    i2c_start();
    result |= i2c_write(RTC_ADDR | I2C_ADDR_READ);
    for (size_t i = 0; i < length; i++) {
        i2c_read(data++, (i == (length - 1)) ? 0 : I2C_SR_MACK);
    }
    i2c_stop();

    return result;
}

static void rtc_sanitize_data (uint8_t *rtc_data) {
    for (int i = 0; i < 7; i++) {
        rtc_data[i] &= rtc_bit_mask[i];
    }
}

uint8_t rtc_set_time (uint8_t *rtc_data) {
    uint8_t result = 0;
    uint8_t tmp;

    rtc_sanitize_data(rtc_data);

    rtc_data[RTC_RTCSEC] |= RTC_RTCSEC_ST;
    rtc_data[RTC_RTCWKDAY] |= RTC_RTCWKDAY_VBAT;

    result |= i2c_tx(RTC_RTCSEC, 0x00, 1);
    result |= i2c_tx(RTC_RTCMIN, rtc_data + 1, 6);
    result |= i2c_tx(RTC_RTCSEC, rtc_data, 1);

    do {
        result |= i2c_rx(RTC_RTCWKDAY, &tmp, 7);
    } while ((!(tmp & RTC_RTCWKDAY_OSCRUN)) || !result);

    return result;
}

uint8_t rtc_get_time (uint8_t *rtc_data) {
    uint8_t result = i2c_rx(RTC_RTCSEC, rtc_data, 7);

    rtc_sanitize_data(rtc_data);

    return result;
}

void rtc_init (void) {
    uint8_t result;
    uint8_t rtc_data[7];

    result = i2c_rx(RTC_RTCSEC, rtc_data, 1);

    if ((!(rtc_data[0] & RTC_RTCSEC_ST)) && (!result)) {
        rtc_get_time(rtc_data);
        rtc_set_time(rtc_data);
    }
}
