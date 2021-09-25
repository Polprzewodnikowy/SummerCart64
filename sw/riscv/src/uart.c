#include "uart.h"
#include "rtc.h"


#ifdef DEBUG
static const char hex_char_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};
#endif

void uart_print (const char *text) {
#ifdef DEBUG
    while (*text != '\0') {
        while (!(UART->SCR & UART_SCR_TXE));
        UART->DR = *text++;
    }
#endif
}

void uart_print_02hex (uint8_t number) {
#ifdef DEBUG
    char buffer[3];
    buffer[0] = hex_char_map[(number >> 4) & 0x0F];
    buffer[1] = hex_char_map[number & 0x0F];
    buffer[2] = '\0';
    uart_print(buffer);
#endif
}

void uart_print_08hex (uint32_t number) {
#ifdef DEBUG
    uart_print_02hex((number >> 24) & 0xFF);
    uart_print_02hex((number >> 16) & 0xFF);
    uart_print_02hex((number >> 8) & 0xFF);
    uart_print_02hex((number >> 0) & 0xFF);
#endif
}

void uart_init (void) {
#ifdef DEBUG
    uart_print("App ready!\n");
#endif
}


void process_uart (void) {
#ifdef DEBUG
    rtc_time_t *time;

    if (UART->SCR & USB_SCR_RXNE) {
        switch (UART->DR) {
            case '/':
                uart_print("Bootloader reset...\n");
                reset_handler();
                break;

            case '\'':
                uart_print("App reset...\n");
                app_handler();
                break;

            case 't':
                time = rtc_get_time();
                uart_print("Current time: ");
                if (rtc_is_time_running()) {
                    uart_print("(running) ");
                }
                if (rtc_is_time_valid()) {
                    uart_print("(valid) ");
                }
                for (int i = 0; i < 7; i++) {
                    uart_print_02hex(((uint8_t *) (time))[i]);
                    uart_print(" ");
                }
                uart_print("\r\n");
        }
    }
#endif
}
