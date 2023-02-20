#include <stddef.h>
#include "display.h"
#include "io.h"
#include "sc64.h"
#include "test.h"


static void test_rtc (void) {
    sc64_rtc_time_t t;
    const char *weekdays[8] = {
        "",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
        "Sunday",
    };

    sc64_get_time(&t);

    display_printf("RTC current time:\n ");
    display_printf("%04d-%02d-%02d", 2000 + FROM_BCD(t.year), FROM_BCD(t.month), FROM_BCD(t.day));
    display_printf("T");
    display_printf("%02d:%02d:%02d", FROM_BCD(t.hour), FROM_BCD(t.minute), FROM_BCD(t.second));
    display_printf(" (%s)", weekdays[FROM_BCD(t.weekday)]);
    display_printf("\n");
}

static void test_sd_card (void) {
    sc64_sd_card_status_t card_status;
    uint8_t card_info[32] __attribute__((aligned(8)));
    uint8_t sector[512] __attribute__((aligned(8)));

    card_status = sc64_sd_card_get_status();

    if (card_status & SD_CARD_STATUS_INSERTED) {
        display_printf("SD card is inserted\n");
    } else {
        display_printf("SD card is not inserted\n");
    }

    if (sc64_sd_card_init()) {
        display_printf("SD card init error!\n");
        return;
    }

    card_status = sc64_sd_card_get_status();

    if (card_status & SD_CARD_STATUS_INITIALIZED) {
        display_printf("SD card is initialized\n");
    }
    if (card_status & SD_CARD_STATUS_TYPE_BLOCK) {
        display_printf("SD card type is block\n");
    }
    if (card_status & SD_CARD_STATUS_50MHZ_MODE) {
        display_printf("SD card runs at 50 MHz clock speed\n");
    }

    if (sc64_sd_card_get_info((uint32_t *) (SC64_BUFFERS->BUFFER))) {
        display_printf("SD card get info error!\n");
        return;
    }

    pi_dma_read((io32_t *) (SC64_BUFFERS->BUFFER), card_info, sizeof(card_info));

    display_printf("SD Card registers:\n CSD: 0x");
    for (int i = 0; i < 16; i++) {
        display_printf("%02X ", card_info[i]);
    }
    display_printf("\n CID: 0x");
    for (int i = 16; i < 32; i++) {
        display_printf("%02X ", card_info[i]);
    }
    display_printf("\n        ");
    for (int i = 16; i < 32; i++) {
        display_printf(" %c ", card_info[i] >= ' ' ? card_info[i] : 0xFF);
    }
    display_printf("\n");

    if (sc64_sd_read_sectors((void *) (SC64_BUFFERS->BUFFER), 0, 1)) {
        display_printf("SD card read sector 0 error!\n");
    }

    pi_dma_read((io32_t *) (SC64_BUFFERS->BUFFER), sector, sizeof(sector));

    display_printf("Sector 0 (0x1BE-0x1DD), partition entry 1/2:\n      0x");
    for (int i = 0; i < 16; i++) {
        display_printf("%02X ", sector[0x1BE + i]);
    }
    display_printf("\n      0x");
    for (int i = 0; i < 16; i++) {
        display_printf("%02X ", sector[0x1CE + i]);
    }
    display_printf("\n");
    display_printf(" Boot signature: 0x%02X%02X", sector[510], sector[511]);
}


bool test_check (void) {
    if (OS_INFO->reset_type != OS_INFO_RESET_TYPE_COLD) {
        return false;
    }
    return sc64_get_config(CFG_ID_BUTTON_STATE);
}

void test_execute (void) {
    display_init(NULL);
    display_printf("SC64 Test suite\n\n");

    display_printf("[ RTC tests ]\n");
    test_rtc();
    display_printf("\n");

    display_printf("[ SD card tests ]\n");
    test_sd_card();
    display_printf("\n");

    while (1);
}
