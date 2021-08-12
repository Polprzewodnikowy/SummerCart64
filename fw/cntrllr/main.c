#include "sys.h"
#include "rtc.h"


volatile int counter = 0;


void print (const char *text) {
    while (*text != '\0') {
        while (!(UART_SR & UART_SR_TXE) && (!(UART_SR & UART_SR_RXNE)));
        UART_TX = *text++;
    }
}

const char hex_char_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

void print_02hex (unsigned char number) {
    char buffer[3];
    buffer[0] = hex_char_map[number >> 4];
    buffer[1] = hex_char_map[number & 0x0F];
    buffer[2] = '\0';
    print(buffer);
}

const char *weekday_names[7] = {
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Sunday"
};

void print_nice_date(uint8_t *date) {
    print_02hex(date[4]);
    print(".");
    print_02hex(date[5]);
    print(".20");
    print_02hex(date[6]);
    print(" (");
    print(weekday_names[date[3] - 1]);
    print(") ");
    print_02hex(date[2]);
    print(":");
    print_02hex(date[1]);
    print(":");
    print_02hex(date[0]);
}

__NAKED__ int main (void) {
    uint8_t rtc_data[7];
    uint8_t rtc_new_data[7];
    int index = 0;

    GPIO_OE = (1 << 0);
    GPIO_O = (1 << 0);

    rtc_init();

    while (1) {
        GPIO_O = (1 << 0);

        print("\033[2J\033[H\r\n --- Hello --- \r\n\r\n");
        print(" RTC Data:\r\n\r\n ");

        rtc_get_time(rtc_data);

        print_nice_date(rtc_data);

        print("\r\n");
        GPIO_O = 0x00;

        while (counter++ < 0x0003FFFF);
        counter = 0;

        if (UART_SR & UART_SR_RXNE) {
            rtc_new_data[index++] = UART_RX;
            if (index == 7) {
                index = 0;
                rtc_set_time(rtc_new_data);
            }
        }
    }
}
