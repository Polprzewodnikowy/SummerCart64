#include <stddef.h>
#include "display.h"
#include "io.h"
#include "sc64.h"
#include "test.h"


bool test_check (void) {
    if (sc64_get_config(CFG_ID_BUTTON_STATE)) {
        return true;
    }
    return false;
}

void test_execute (void) {
    sd_card_status_t card_status;
    uint8_t card_info[32] __attribute__((aligned(8)));

    display_init(NULL);

    display_printf("SC64 Test suite\n\n");

    if (sc64_sd_card_init()) {
        display_printf("SD card init error!\n");
        while (1);
    }

    card_status = sc64_sd_card_get_status();

    if (card_status & SD_CARD_STATUS_INITIALIZED) {
        display_printf("SD card initialized\n");
    }
    if (card_status & SD_CARD_STATUS_TYPE_BLOCK) {
        display_printf("SD card type block\n");
    }
    if (card_status & SD_CARD_STATUS_50MHZ_MODE) {
        display_printf("SD card 50 MHz clock mode\n");
    }

    if (sc64_sd_card_get_info((uint32_t *) (SC64_BUFFERS->BUFFER))) {
        display_printf("SD card get info error!\n");
        while (1);
    }

    pi_dma_read((io32_t *) (SC64_BUFFERS->BUFFER), card_info, sizeof(card_info));

    display_printf("SD Card registers:\n    CSD: 0x");
    for (int i = 0; i < 16; i++) {
        display_printf("%02X", card_info[i]);
    }
    display_printf("\n    CID: 0x");
    for (int i = 16; i < 32; i++) {
        display_printf("%02X", card_info[i]);
    }
    display_printf("\n           ");
    for (int i = 16; i < 32; i++) {
        display_printf("%c ", card_info[i] >= ' ' ? card_info[i] : 0xFF);
    }

    if (sc64_sd_card_deinit()) {
        display_printf("SD card deinit error!\n");
        while (1);
    }

    while (1);
}
