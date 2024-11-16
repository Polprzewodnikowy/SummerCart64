#include <stddef.h>
#include <stdlib.h>
#include "display.h"
#include "error.h"
#include "fatfs/ff.h"
#include "init.h"
#include "io.h"
#include "sc64.h"
#include "test.h"


#define TEST_BUFFER_SIZE    (128 * 1024)

#define SD_TEST_FILE_SIZE   (16 * 1024 * 1024)

#define SDRAM_ADDRESS       (0x10000000)
#define SDRAM_SIZE          (64 * 1024 * 1024)


typedef struct  {
    char *name;
    void (*fill) (uint32_t *buffer, int size, uint32_t offset, uint32_t pattern);
    uint32_t pattern;
    int fade;
} sdram_test_t;


static uint32_t random_seed = 0;

static uint32_t w_buffer[TEST_BUFFER_SIZE / sizeof(uint32_t)] __attribute__((aligned(8)));
static uint32_t r_buffer[TEST_BUFFER_SIZE / sizeof(uint32_t)] __attribute__((aligned(8)));


static void fill_own_address (uint32_t *buffer, int size, uint32_t pattern, uint32_t offset) {
    for (int i = 0; i < (size / sizeof(uint32_t)); i++) {
        uint32_t value = offset + (i * sizeof(uint32_t));
        value ^= pattern;
        buffer[i] = value;
    }
}

static void fill_pattern (uint32_t *buffer, int size, uint32_t pattern, uint32_t offset) {
    for (int i = 0; i < (size / sizeof(uint32_t)); i++) {
        buffer[i] = pattern;
    }
}

static void fill_random (uint32_t *buffer, int size, uint32_t pattern, uint32_t offset) {
    for (int i = 0; i < (size / sizeof(uint32_t)); i++) {
        buffer[i] = (rand() << 31) | rand();
    }
}


static void test_sc64_cfg (void) {
    sc64_error_t error;
    uint32_t button_state;
    uint32_t identifier;
    uint16_t major;
    uint16_t minor;
    uint32_t revision;
    uint32_t tmp;

    display_printf("Waiting for the button to be released... ");

    do {
        if ((error = sc64_get_config(CFG_ID_BUTTON_STATE, &button_state)) != SC64_OK) {
            error_display("Command CONFIG_GET [BUTTON_STATE] failed\n (%08X) - %s", error, sc64_error_description(error));
        }
    } while (button_state != 0);

    display_printf("done\n\n");

    if ((error = sc64_get_identifier(&identifier)) != SC64_OK) {
        error_display("Command IDENTIFIER_GET failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    if ((error = sc64_get_version(&major, &minor, &revision)) != SC64_OK) {
        error_display("Command VERSION_GET failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    if ((error = sc64_get_diagnostic(DIAGNOSTIC_ID_VOLTAGE_TEMPERATURE, &tmp)) != SC64_OK) {
        error_display("Command DIAGNOSTIC_GET failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    uint16_t voltage = (uint16_t) (tmp >> 16);
    int16_t temperature = (int16_t) (tmp & 0xFFFF);

    display_printf("Identifier: 0x%08X\n", identifier);
    display_printf("SC64 firmware version: %d.%d.%d\n\n", major, minor, revision);

    display_printf("Voltage: %d.%03d V\n", (voltage / 1000), (voltage % 1000));
    display_printf("Temperature: %d.%01d `C\n\n", (temperature / 10), (temperature % 10));

    const char *weekdays[8] = {
        "Unknown day",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
        "Sunday",
    };

    sc64_rtc_time_t t;

    if ((error = sc64_get_time(&t)) != SC64_OK) {
        error_display("Command TIME_GET failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    display_printf("RTC current time:\n ");
    display_printf("%04d-%02d-%02d", 1900 + (t.century * 100) + FROM_BCD(t.year), FROM_BCD(t.month), FROM_BCD(t.day));
    display_printf("T");
    display_printf("%02d:%02d:%02d", FROM_BCD(t.hour), FROM_BCD(t.minute), FROM_BCD(t.second));
    display_printf(" (%s)", weekdays[FROM_BCD(t.weekday)]);
    display_printf("\n");

    int count = 65536;

    for (int i = 0; i < count; i++) {
        if ((i % (count / 64)) == 0) {
            display_printf(".");
        }
        if ((error = sc64_get_identifier(&identifier)) != SC64_OK) {
            error_display("Command IDENTIFIER_GET failed\n (%08X) - %s", error, sc64_error_description(error));
        }
        if (identifier != 0x53437632) {
            error_display("Invalid identifier received: 0x%08X", identifier);
        }
    }

    display_printf("\n");
}

static void test_pi (void) {
    int count = 16384;
    int size = sizeof(SC64_BUFFERS->BUFFER);

    srand(random_seed);

    display_printf("Testing %d write/read cycles of %dkiB to the SC64 buffer\n\n", count, size / 1024);

    for (int i = 0; i < count; i++) {
        fill_random(w_buffer, size, 0, 0);

        if ((i % (count / 256)) == 0) {
            display_printf(".");
        }

        pi_dma_write((io32_t *) (SC64_BUFFERS->BUFFER), w_buffer, size);

        pi_dma_read((io32_t *) (SC64_BUFFERS->BUFFER), r_buffer, size);

        for (int i = 0; i < size / sizeof(uint32_t); i++) {
            if (w_buffer[i] != r_buffer[i]) {
                error_display(
                    "PI test failed:\n"
                    " > Mismatch error at address 0x%08lX\n"
                    " > 0x%08lX (W) != 0x%08lX (R)",
                    (uint32_t) (SC64_BUFFERS->BUFFER) + (i * sizeof(uint32_t)),
                    w_buffer[i],
                    r_buffer[i]
                );
            }
        }
    }

    random_seed += c0_count();

    display_printf("\n");
}

static void test_sd_card_io (void) {
    sc64_error_t error;
    sc64_sd_card_status_t card_status;
    uint8_t card_info[32] __attribute__((aligned(8)));
    uint8_t sector[512] __attribute__((aligned(8)));

    if ((error = sc64_sd_card_get_status(&card_status)) != SC64_OK) {
        error_display("Could not get SD card status\n (%08X) - %s", error, sc64_error_description(error));
    }

    if (card_status & SD_CARD_STATUS_INSERTED) {
        display_printf("SD card is inserted\n");
    } else {
        display_printf("SD card is not inserted\n");
    }

    if ((error = sc64_sd_card_deinit()) != SC64_OK) {
        return display_printf("SD card deinit error, skipping test\n (%08X) - %s", error, sc64_error_description(error));
    }

    if ((error = sc64_sd_card_init()) != SC64_OK) {
        return display_printf("SD card init error, skipping test\n (%08X) - %s\n", error, sc64_error_description(error));
    }

    if ((error = sc64_sd_card_get_status(&card_status)) != SC64_OK) {
        error_display("Could not get SD card info\n (%08X) - %s", error, sc64_error_description(error));
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
        error_display("SD card get info error\n (%08X) - %s", error, sc64_error_description(error));
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
        return display_printf("SD card read sector 0 error\n (%08X) - %s\n", error, sc64_error_description(error));
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

static void test_sd_card_fatfs (void) {
    sc64_error_t error;
    FRESULT fresult;
    FATFS fs;
    TCHAR label[23];
    DWORD vsn;
    FIL fil;
    UINT left;
    UINT bytes;

    if ((error = sc64_sd_card_deinit()) != SC64_OK) {
        return display_printf("SD card deinit error, skipping test\n (%08X) - %s", error, sc64_error_description(error));
    }

    if ((fresult = f_mount(&fs, "", 1)) != FR_OK) {
        display_printf("f_mount error: %d\n", fresult);
    }

    if ((fresult = f_getlabel("", label, &vsn)) != FR_OK) {
        display_printf("f_getlabel error: %d\n", fresult);
    } else {
        display_printf("Volume [%s] / [%08lX]\n\n", label, vsn);
    }

    if ((fresult = f_open(&fil, "sc64test.bin", FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK) {
        display_printf("f_open (write) error: %d\n", fresult);
    }

    srand(random_seed);

    left = SD_TEST_FILE_SIZE;

    while (left) {
        if ((left % (SD_TEST_FILE_SIZE / 64)) == 0) {
            display_printf("w");
        }

        UINT block_size = (left > TEST_BUFFER_SIZE) ? TEST_BUFFER_SIZE : left;

        fill_random(w_buffer, block_size, 0, 0);

        if ((fresult = f_write(&fil, w_buffer, block_size, &bytes)) != FR_OK) {
            display_printf("\nf_write error: %d", fresult);
            break;
        }

        if (bytes != block_size) {
            display_printf("\nf_write (%ld!=%ld) error: %d", bytes, block_size, fresult);
            break;
        }

        left -= block_size;
    }

    display_printf("\n");

    if ((fresult = f_close(&fil)) != FR_OK) {
        display_printf("f_close (write) error: %d\n", fresult);
    }

    if ((fresult = f_open(&fil, "sc64test.bin", FA_OPEN_EXISTING | FA_READ)) != FR_OK) {
        display_printf("f_open (read) error: %d\n", fresult);
    }

    if (f_size(&fil) != SD_TEST_FILE_SIZE) {
        display_printf("f_size (%d!=%d) error\n", f_size(&fil), SD_TEST_FILE_SIZE);
    }

    srand(random_seed);

    left = SD_TEST_FILE_SIZE;

    while (left) {
        if ((left % (SD_TEST_FILE_SIZE / 64)) == 0) {
            display_printf("r");
        }

        UINT block_size = (left > TEST_BUFFER_SIZE) ? TEST_BUFFER_SIZE : left;

        fill_random(w_buffer, block_size, 0, 0);

        if ((fresult = f_read(&fil, r_buffer, block_size, &bytes)) != FR_OK) {
            display_printf("\nf_read error: %d", fresult);
            break;
        }

        if (bytes != block_size) {
            display_printf("\nf_read (%ld!=%ld) error: %d", bytes, block_size, fresult);
            break;
        }

        for (UINT i = 0; i < (block_size / sizeof(uint32_t)); i++) {
            if (w_buffer[i] != r_buffer[i]) {
                error_display(
                    "SD card (FatFs) test failed:\n"
                    " > Mismatch error at file offset 0x%08lX\n"
                    " > 0x%08lX (W) != 0x%08lX (R)",
                    f_tell(&fil) + (i * sizeof(uint32_t)),
                    w_buffer[i],
                    r_buffer[i]
                );
            }
        }

        left -= block_size;
    }

    display_printf("\n");

    if ((fresult = f_close(&fil)) != FR_OK) {
        display_printf("f_close (read) error: %d\n", fresult);
    }

    if ((fresult = f_unlink("sc64test.bin")) != FR_OK) {
        display_printf("f_unlink error: %d\n", fresult);
    }

    if ((fresult = f_unmount("")) != FR_OK) {
        display_printf("f_unmount error: %d\n", fresult);
    }

    random_seed += c0_count();
}

static void test_sdram (void) {
    static int phase = 0;

    sdram_test_t phase_0_tests[] = {
        { .name = "Own address:  ", .fill = fill_own_address, .pattern = 0x00000000, .fade = 0  },
        { .name = "Own address~: ", .fill = fill_own_address, .pattern = 0xFFFFFFFF, .fade = 0  },
        { .name = "All zeros:    ", .fill = fill_pattern,     .pattern = 0x00000000, .fade = 0  },
        { .name = "All ones:     ", .fill = fill_pattern,     .pattern = 0xFFFFFFFF, .fade = 0  },
        { .name = "0xAAAA5555:   ", .fill = fill_pattern,     .pattern = 0xAAAA5555, .fade = 0  },
        { .name = "0x5555AAAA:   ", .fill = fill_pattern,     .pattern = 0x5555AAAA, .fade = 0  },
        { .name = "Random (1/4): ", .fill = fill_random,      .pattern = 0x00000000, .fade = 0  },
        { .name = "Random (2/4): ", .fill = fill_random,      .pattern = 0x00000000, .fade = 0  },
        { .name = "Random (3/4): ", .fill = fill_random,      .pattern = 0x00000000, .fade = 0  },
        { .name = "Random (4/4): ", .fill = fill_random,      .pattern = 0x00000000, .fade = 0  },
        { .name = NULL },
    };

    sdram_test_t phase_1_tests[] = {
        { .name = "0x00010001:   ", .fill = fill_pattern,     .pattern = 0x00010001, .fade = 0  },
        { .name = "0xFFFEFFFE:   ", .fill = fill_pattern,     .pattern = 0xFFFEFFFE, .fade = 0  },
        { .name = "0x00020002:   ", .fill = fill_pattern,     .pattern = 0x00020002, .fade = 0  },
        { .name = "0xFFFDFFFD:   ", .fill = fill_pattern,     .pattern = 0xFFFDFFFD, .fade = 0  },
        { .name = "0x00040004:   ", .fill = fill_pattern,     .pattern = 0x00040004, .fade = 0  },
        { .name = "0xFFFBFFFB:   ", .fill = fill_pattern,     .pattern = 0xFFFBFFFB, .fade = 0  },
        { .name = "0x00080008:   ", .fill = fill_pattern,     .pattern = 0x00080008, .fade = 0  },
        { .name = "0xFFF7FFF7:   ", .fill = fill_pattern,     .pattern = 0xFFF7FFF7, .fade = 0  },
        { .name = NULL },
    };

    sdram_test_t phase_2_tests[] = {
        { .name = "0x00100010:   ", .fill = fill_pattern,     .pattern = 0x00100010, .fade = 0  },
        { .name = "0xFFEFFFEF:   ", .fill = fill_pattern,     .pattern = 0xFFEFFFEF, .fade = 0  },
        { .name = "0x00200020:   ", .fill = fill_pattern,     .pattern = 0x00200020, .fade = 0  },
        { .name = "0xFFDFFFDF:   ", .fill = fill_pattern,     .pattern = 0xFFDFFFDF, .fade = 0  },
        { .name = "0x00400040:   ", .fill = fill_pattern,     .pattern = 0x00400040, .fade = 0  },
        { .name = "0xFFBFFFBF:   ", .fill = fill_pattern,     .pattern = 0xFFBFFFBF, .fade = 0  },
        { .name = "0x00800080:   ", .fill = fill_pattern,     .pattern = 0x00800080, .fade = 0  },
        { .name = "0xFF7FFF7F:   ", .fill = fill_pattern,     .pattern = 0xFF7FFF7F, .fade = 0  },
        { .name = NULL },
    };

    sdram_test_t phase_3_tests[] = {
        { .name = "0x01000100:   ", .fill = fill_pattern,     .pattern = 0x01000100, .fade = 0  },
        { .name = "0xFEFFFEFF:   ", .fill = fill_pattern,     .pattern = 0xFEFFFEFF, .fade = 0  },
        { .name = "0x02000200:   ", .fill = fill_pattern,     .pattern = 0x02000200, .fade = 0  },
        { .name = "0xFDFFFDFF:   ", .fill = fill_pattern,     .pattern = 0xFDFFFDFF, .fade = 0  },
        { .name = "0x04000400:   ", .fill = fill_pattern,     .pattern = 0x04000400, .fade = 0  },
        { .name = "0xFBFFFBFF:   ", .fill = fill_pattern,     .pattern = 0xFBFFFBFF, .fade = 0  },
        { .name = "0x08000800:   ", .fill = fill_pattern,     .pattern = 0x08000800, .fade = 0  },
        { .name = "0xF7FFF7FF:   ", .fill = fill_pattern,     .pattern = 0xF7FFF7FF, .fade = 0  },
        { .name = NULL },
    };

    sdram_test_t phase_4_tests[] = {
        { .name = "0x10001000:   ", .fill = fill_pattern,     .pattern = 0x10001000, .fade = 0  },
        { .name = "0xEFFFEFFF:   ", .fill = fill_pattern,     .pattern = 0xEFFFEFFF, .fade = 0  },
        { .name = "0x20002000:   ", .fill = fill_pattern,     .pattern = 0x20002000, .fade = 0  },
        { .name = "0xDFFFDFFF:   ", .fill = fill_pattern,     .pattern = 0xDFFFDFFF, .fade = 0  },
        { .name = "0x40004000:   ", .fill = fill_pattern,     .pattern = 0x40004000, .fade = 0  },
        { .name = "0xBFFFBFFF:   ", .fill = fill_pattern,     .pattern = 0xBFFFBFFF, .fade = 0  },
        { .name = "0x80008000:   ", .fill = fill_pattern,     .pattern = 0x80008000, .fade = 0  },
        { .name = "0x7FFF7FFF:   ", .fill = fill_pattern,     .pattern = 0x7FFF7FFF, .fade = 0  },
        { .name = NULL },
    };

    sdram_test_t phase_5_tests[] = {
        { .name = "Fadeout (0):  ", .fill = fill_pattern,     .pattern = 0x00000000, .fade = 60 },
        { .name = "Fadeout (1):  ", .fill = fill_pattern,     .pattern = 0xFFFFFFFF, .fade = 60 },
        { .name = NULL },
    };

    sdram_test_t *test = NULL;

    switch (phase) {
        case 0: test = phase_0_tests; break;
        case 1: test = phase_1_tests; break;
        case 2: test = phase_2_tests; break;
        case 3: test = phase_3_tests; break;
        case 4: test = phase_4_tests; break;
        case 5: test = phase_5_tests; break;
    }

    while (test->name != NULL) {
        display_printf("%s ", test->name);

        srand(random_seed);

        for (int offset = 0; offset < SDRAM_SIZE; offset += TEST_BUFFER_SIZE) {
            if ((offset % (SDRAM_SIZE / 16)) == 0) {
                display_printf("w");
            }

            test->fill(w_buffer, TEST_BUFFER_SIZE, test->pattern, offset);

            pi_dma_write((io32_t *) (SDRAM_ADDRESS + offset), w_buffer, TEST_BUFFER_SIZE);
        }

        for (int fade = test->fade; fade > 0; fade--) {
            display_printf(" %3ds", fade);
            delay_ms(1000);
            display_printf("\b\b\b\b\b");
        }

        srand(random_seed);

        for (int offset = 0; offset < SDRAM_SIZE; offset += TEST_BUFFER_SIZE) {
            if ((offset % (SDRAM_SIZE / 16)) == 0) {
                display_printf("r");
            }

            test->fill(w_buffer, TEST_BUFFER_SIZE, test->pattern, offset);

            pi_dma_read((io32_t *) (SDRAM_ADDRESS + offset), r_buffer, TEST_BUFFER_SIZE);

            uint64_t *test_data = (uint64_t *) (w_buffer);
            uint64_t *check_data = (uint64_t *) (r_buffer);

            for (int i = 0; i < TEST_BUFFER_SIZE / sizeof(uint64_t); i++) {
                if (test_data[i] != check_data[i]) {
                    error_display(
                        "SDRAM test failed, %s\n"
                        " > Mismatch error at address 0x%08lX\n"
                        " > 0x%016llX (W) != 0x%016llX (R)",
                        test->name,
                        SDRAM_ADDRESS + offset + (i * sizeof(uint64_t)),
                        test_data[i],
                        check_data[i]
                    );
                }
            }
        }

        random_seed += c0_count();

        display_printf(" OK\n");

        test += 1;
    }

    phase += 1;

    if (phase > 5) {
        phase = 0;
    }
}


bool test_check (void) {
    sc64_error_t error;
    uint32_t button_state;

    if (__reset_type == INIT_RESET_TYPE_WARM) {
        return false;
    }

    if ((error = sc64_get_config(CFG_ID_BUTTON_STATE, &button_state)) != SC64_OK) {
        error_display("Command CONFIG_GET [BUTTON_STATE] failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    return (button_state != 0);
}

static struct {
    const char *title;
    void (*fn) (void);
} tests[] = {
    { "SC64 CFG", test_sc64_cfg },
    { "PI", test_pi },
    { "SD card (I/O)", test_sd_card_io },
    { "SD card (FatFs)", test_sd_card_fatfs },
    { "SDRAM (1/6)", test_sdram },
    { "SDRAM (2/6)", test_sdram },
    { "SDRAM (3/6)", test_sdram },
    { "SDRAM (4/6)", test_sdram },
    { "SDRAM (5/6)", test_sdram },
    { "SDRAM (6/6)", test_sdram },
};

void test_execute (void) {
    sc64_error_t error;

    const int test_count = sizeof(tests) / sizeof(tests[0]);
    int current = 0;

    display_init(NULL);
    display_printf("SC64 Test suite (%d / %d)\n\n", 0, test_count);

    display_printf("Initializing...\n");

    pi_io_config(0x0F, 0x05, 0x0C, 0x02);

    sc64_cmd_irq_enable(true);
    sc64_usb_irq_enable(true);
    sc64_aux_irq_enable(true);

    if ((error = sc64_set_config(CFG_ID_ROM_WRITE_ENABLE, true)) != SC64_OK) {
        error_display("Command CONFIG_SET [ROM_WRITE_ENABLE] failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    if ((error = sc64_set_config(CFG_ID_ROM_SHADOW_ENABLE, false)) != SC64_OK) {
        error_display("Command CONFIG_SET [ROM_SHADOW_ENABLE] failed\n (%08X) - %s", error, sc64_error_description(error));
    }

    random_seed = __entropy + c0_count();

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

        display_printf("Next test [ %s ] starts in: ", tests[current].title);
        for (int delay = 10; delay > 0; delay--) {
            display_printf("%2ds", delay);
            delay_ms(1000);
            display_printf("\b\b\b", delay);
        }
    }
}
