#include <stddef.h>
#include <stdlib.h>
#include "display.h"
#include "error.h"
#include "io.h"
#include "sc64.h"
#include "test.h"


static void test_sc64_cfg (void) {
    sc64_error_t error;
    uint32_t identifier;
    uint16_t major;
    uint16_t minor;
    uint32_t revision;

    if ((error = sc64_get_identifier(&identifier)) != SC64_OK) {
        error_display("Command IDENTIFIER_GET failed: %d", error);
        return;
    }
    
    if ((error = sc64_get_version(&major, &minor, &revision)) != SC64_OK) {
        error_display("Command VERSION_GET failed: %d", error);
        return;
    }

    display_printf("Identifier: 0x%08X\n\n", identifier);

    display_printf("SC64 firmware version: %d.%d.%d\n", major, minor, revision);
}

static void test_rtc (void) {
    sc64_error_t error;
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

    if ((error = sc64_get_time(&t)) != SC64_OK) {
        error_display("Command TIME_GET failed: %d", error);
    }

    display_printf("RTC current time:\n ");
    display_printf("%04d-%02d-%02d", 2000 + FROM_BCD(t.year), FROM_BCD(t.month), FROM_BCD(t.day));
    display_printf("T");
    display_printf("%02d:%02d:%02d", FROM_BCD(t.hour), FROM_BCD(t.minute), FROM_BCD(t.second));
    display_printf(" (%s)", weekdays[FROM_BCD(t.weekday)]);
    display_printf("\n");
}

static void test_sd_card (void) {
    sc64_error_t error;
    sc64_sd_card_status_t card_status;
    uint8_t card_info[32] __attribute__((aligned(8)));
    uint8_t sector[512] __attribute__((aligned(8)));

    if ((error = sc64_sd_card_get_status(&card_status)) != SC64_OK) {
        error_display("Could not get SD card info: %d", error);
    }

    if (card_status & SD_CARD_STATUS_INSERTED) {
        display_printf("SD card is inserted\n");
    } else {
        display_printf("SD card is not inserted\n");
    }

    if ((error = sc64_sd_card_init()) != SC64_OK) {
        display_printf("SD card init error: %d\n", error);
        return;
    }

    if ((error = sc64_sd_card_get_status(&card_status)) != SC64_OK) {
        error_display("Could not get SD card info: %d", error);
    }

    if (card_status & SD_CARD_STATUS_INITIALIZED) {
        display_printf("SD card is initialized\n");
    }
    if (card_status & SD_CARD_STATUS_TYPE_BLOCK) {
        display_printf("SD card type is block\n");
    }
    if (card_status & SD_CARD_STATUS_50MHZ_MODE) {
        display_printf("SD card runs at 50 MHz clock speed\n");
    }
    if (card_status & SD_CARD_STATUS_BYTE_SWAP) {
        display_printf("SD card read byte swap is enabled\n");
    }

    if ((error = sc64_sd_card_get_info((uint32_t *) (SC64_BUFFERS->BUFFER))) != SC64_OK) {
        display_printf("SD card get info error: %d\n", error);
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

    if ((error = sc64_sd_read_sectors((void *) (SC64_BUFFERS->BUFFER), 0, 1)) != SC64_OK) {
        display_printf("SD card read sector 0 error: %d\n", error);
        return;
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
    display_printf(" Boot signature: 0x%02X%02X\n", sector[510], sector[511]);
}

#define SDRAM_ADDRESS   (0x10000000)
#define SDRAM_SIZE      (64 * 1024 * 1024)
#define BUFFER_SIZE     (128 * 1024)

static uint32_t w_buffer[BUFFER_SIZE / sizeof(uint32_t)] __attribute__((aligned(8)));
static uint32_t r_buffer[BUFFER_SIZE / sizeof(uint32_t)] __attribute__((aligned(8)));

static void test_sdram (void) {
    sc64_error_t error;

    if ((error = sc64_set_config(CFG_ID_ROM_WRITE_ENABLE, true))) {
        error_display("Command CONFIG_SET [ROM_WRITE_ENABLE] failed: %d", error);
        return;
    }

    if ((error = sc64_set_config(CFG_ID_ROM_SHADOW_ENABLE, false))) {
        error_display("Command CONFIG_SET [ROM_SHADOW_ENABLE] failed: %d", error);
        return;
    }

    pi_io_config(0x0B, 0x05, 0x0C, 0x02);
    display_printf("PI config - PGS: 0x0B, LAT: 0x05, PWD: 0x0C, RLS: 0x02\n");

    const struct patterns_s {
        bool constant;
        uint32_t value;
    } patterns[] = {
        { .constant = true, .value = 0x00000000 },
        { .constant = true, .value = 0xFFFFFFFF },
        { .constant = true, .value = 0xFFFF0000 },
        { .constant = true, .value = 0x0000FFFF },
        { .constant = true, .value = 0xF0F0F0F0 },
        { .constant = true, .value = 0x0F0F0F0F },
        { .constant = true, .value = 0xAAAAAAAA },
        { .constant = true, .value = 0x55555555 },
        { .constant = true, .value = 0xA5A5A5A5 },
        { .constant = true, .value = 0x5A5A5A5A },
        { .constant = false },
        { .constant = false },
    };

    srand(c0_count());

    for (int pattern = 0; pattern < sizeof(patterns) / sizeof(patterns[0]); pattern++) {
        if (patterns[pattern].constant) {
            display_printf("Pattern: 0x%08X ", patterns[pattern].value);
            for (int i = 0; i < BUFFER_SIZE / sizeof(uint32_t); i++) {
                w_buffer[i] = patterns[pattern].value;
            }
        } else {
            display_printf("Pattern: random     ");
        }

        for (int offset = 0; offset < SDRAM_SIZE; offset += BUFFER_SIZE) {
            if (!patterns[pattern].constant) {
                for (int i = 0; i < BUFFER_SIZE / sizeof(uint32_t); i++) {
                    *UNCACHED(&w_buffer[i]) = (rand() << 31) | rand();
                }
            }

            pi_dma_write((io32_t *) (SDRAM_ADDRESS + offset), w_buffer, BUFFER_SIZE);

            pi_dma_read((io32_t *) (SDRAM_ADDRESS + offset), r_buffer, BUFFER_SIZE);

            for (int i = 0; i < BUFFER_SIZE / sizeof(uint32_t); i++) {
                if (*UNCACHED(&w_buffer[i]) != *UNCACHED(&r_buffer[i])) {
                    display_printf(
                        "\nMISMATCH: [0x%08X]: 0x%08X (R) != 0x%08X (W)\n",
                        SDRAM_ADDRESS + offset,
                        *UNCACHED(&r_buffer[i]),
                        *UNCACHED(&w_buffer[i])
                    );
                    while (true);
                }
            }

            if ((offset % (SDRAM_SIZE / 32)) == 0) {
                display_printf(".");
            }
        }

        display_printf(" OK\n");
    }
}


bool test_check (void) {
    sc64_error_t error;
    uint32_t button_state;

    if (OS_INFO->reset_type != OS_INFO_RESET_TYPE_COLD) {
        return false;
    }

    if ((error = sc64_get_config(CFG_ID_BUTTON_STATE, &button_state)) != SC64_OK) {
        error_display("Command CONFIG_GET [BUTTON_STATE] failed: %d", error);
    }

    return button_state != 0;
}

static struct {
    const char *title;
    void (*fn) (void);
} tests[] = {
    { "SC64 CFG", test_sc64_cfg },
    { "RTC", test_rtc },
    { "SD card", test_sd_card },
    { "SDRAM", test_sdram },
};

void test_execute (void) {
    const int test_count = sizeof(tests) / sizeof(tests[0]);
    int current = 0;

    while (true) {
        display_init(NULL);
        display_printf("SC64 Test suite (%d / %d)\n\n", current + 1, test_count);

        display_printf("[ %s tests ]\n\n", tests[current].title);
        tests[current].fn();
        display_printf("\n");

        current += 1;
        if (current == test_count) {
            current = 0;
        }

        display_printf("Next test [ %s ] starts in:  ", tests[current].title);
        for (int delay = 5; delay > 0; delay--) {
            display_printf("\b%d", delay);
            delay_ms(1000);
        }
    }
}
