#include "sys.h"
#include "rtc.h"


volatile int counter = 0;


void print (const char *text) {
    while (*text != '\0') {
        while (!(UART_SCR & UART_SCR_TXE));
        UART_DR = *text++;
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

const char CMD[3] = {'C', 'M', 'D'};
const char CMD_R = 'R';
const char CMD_W = 'W';

const char CMP[3] = {'C', 'M', 'P'};

__NAKED__ int main (void) {
    uint8_t data;
    uint8_t cmd = '-';
    uint32_t arg1, arg2;
    char tmp[2];

    tmp[1] = 0;

    GPIO_OE = (1 << 0);
    GPIO_O = 0; // (1 << 0);

    DMA_SCR = DMA_SCR_STOP;
    USB_SCR = USB_SCR_FLUSH_TX | USB_SCR_FLUSH_TX;

    print("CPU START\r\n");

    while (1) {
        arg1 = 0;
        arg2 = 0;


        if (USB_SCR & USB_SCR_RXNE) {
            for (int i = 0; i < 4; i++) {
                while (!(USB_SCR & USB_SCR_RXNE));
                data = USB_DR;
                if (i < 3 && data != CMD[i]) {
                    i = 0;
                    print("Wrong data ");
                    print_02hex(data);
                    print("\r\n");
                } else {
                    cmd = data;
                }
            }

            print("Received CMD");
            tmp[0] = cmd;
            print(tmp);
            print("\r\n");

            for (int i = 0; i < 4; i++) {
                while (!(USB_SCR & USB_SCR_RXNE));
                arg1 = (arg1 << 8) | USB_DR;
            }

            print("Received ARG_1 0x");
            for (int i = 0; i < 4; i++) {
                print_02hex((uint8_t) (arg1 >> ((3 - i) * 8)));
            }
            print("\r\n");

            for (int i = 0; i < 4; i++) {
                while (!(USB_SCR & USB_SCR_RXNE));
                arg2 = (arg2 << 8) | USB_DR;
            }

            print("Received ARG_2 0x");
            for (int i = 0; i < 4; i++) {
                print_02hex((uint8_t) (arg2 >> ((3 - i) * 8)));
            }
            print("\r\n");

            DMA_MADDR = arg1;
            DMA_ID_LEN = arg2;
            DMA_SCR = (cmd == CMD_W ? DMA_SCR_DIR : 0) | DMA_SCR_START;

            print("Started DMA\r\n");

            while (DMA_SCR & DMA_SCR_BUSY);

            print("Finished DMA\r\n");

            for (int i = 0; i < 4; i++) {
                while (!(USB_SCR & USB_SCR_TXE));
                if (i < 3) {
                    USB_DR = CMP[i];
                } else {
                    USB_DR = cmd;
                }
            }

            print("Sent response CMP");
            tmp[0] = cmd;
            print(tmp);
            print("\r\n\r\n");
        } else if (CFG_SCR & CFG_SCR_CPU_BUSY) {
            uint8_t cmd = CFG_COMMAND;
            arg1 = CFG_ARG_1;
            arg2 = CFG_ARG_2;
            print("Received N64 CMD");
            tmp[0] = cmd;
            print(tmp);
            print("\r\n");
            if (cmd == 'S') {
                if (arg1) {
                    CFG_SCR |= CFG_SCR_SDRAM_SWITCH;
                } else {
                    CFG_SCR &= ~CFG_SCR_SDRAM_SWITCH;
                }
            }
            CFG_RESPONSE = 0;
        }
    }
}

// __NAKED__ int main (void) {
    // uint8_t rtc_data[7];
    // uint8_t rtc_new_data[7];
    // int index = 0;

    // GPIO_OE = (1 << 0);
    // GPIO_O = (1 << 0);

    // rtc_init();

    // while (!(USB_SR & USB_SR_TXE));
    // USB_DR = 0x55;
    // USB_DR = 0xAA;
    // USB_DR = 0x00;
    // USB_DR = 'D';
    // USB_DR = 'E';
    // USB_DR = 'A';
    // USB_DR = 'D';
    // USB_DR = 0xFF;
    // USB_DR = 0xAA;
    // USB_DR = 0x55;

    // while (1) {
        // GPIO_O = (1 << 0);

        // print("\033[2J\033[H\r\n --- Hello --- \r\n\r\n");
        // print(" RTC Data:\r\n\r\n ");

        // rtc_get_time(rtc_data);

        // print_nice_date(rtc_data);

        // print("\r\n");
        // GPIO_O = 0x00;

        // while (counter++ < 0x0003FFFF);
        // counter = 0;

        // if (USB_SR & USB_SR_RXNE) {
        //     rtc_new_data[index++] = USB_DR;
        //     if (index == 7) {
        //         index = 0;
        //         rtc_set_time(rtc_new_data);
        //     }
        // }
    // }
// }
