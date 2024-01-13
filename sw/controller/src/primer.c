#include <stdbool.h>
#include <string.h>
#include "hw.h"
#include "vendor.h"


static const uint8_t cmd_token[3] = { 'C', 'M', 'D' };
static const uint8_t rsp_token[3] = { 'R', 'S', 'P' };
static const uint8_t err_token[3] = { 'E', 'R', 'R' };


static void primer_get_and_calculate_crc32 (uint8_t *buffer, uint8_t rx_length, uint32_t *crc32) {
    hw_uart_read(buffer, rx_length);
    *crc32 = hw_crc32_calculate(buffer, rx_length);
}

static uint8_t primer_get_command (uint8_t *buffer, uint8_t *rx_length) {
    uint32_t calculated_crc32;
    uint32_t received_crc32;
    uint8_t token[4];

    while (1) {
        hw_crc32_reset();

        primer_get_and_calculate_crc32(token, 4, &calculated_crc32);
        if (memcmp(token, cmd_token, 3) != 0) {
            continue;
        }

        primer_get_and_calculate_crc32(rx_length, 1, &calculated_crc32);

        if (*rx_length > 0) {
            primer_get_and_calculate_crc32(buffer, *rx_length, &calculated_crc32);
        }

        hw_uart_read((uint8_t *) (&received_crc32), sizeof(received_crc32));
        if (calculated_crc32 == received_crc32) {
            break;
        }
    }

    return token[3];
}

static void primer_send_and_calculate_crc32 (uint8_t *buffer, uint8_t tx_length, uint32_t *crc32) {
    hw_uart_write(buffer, tx_length);
    *crc32 = hw_crc32_calculate(buffer, tx_length);
}

static void primer_send_response (uint8_t cmd, uint8_t *buffer, uint8_t tx_length, bool error) {
    uint32_t calculated_crc32;
    uint8_t *token = (uint8_t *) (error ? err_token : rsp_token);
    uint8_t length = (error ? 0 : tx_length);

    hw_crc32_reset();

    primer_send_and_calculate_crc32(token, 3, &calculated_crc32);
    primer_send_and_calculate_crc32(&cmd, 1, &calculated_crc32);

    primer_send_and_calculate_crc32(&length, 1, &calculated_crc32);

    if ((!error) && (tx_length > 0)) {
        primer_send_and_calculate_crc32(buffer, tx_length, &calculated_crc32);
    }

    hw_uart_write((uint8_t *) (&calculated_crc32), sizeof(calculated_crc32));
}


void primer (void) {
    hw_primer_init();

    vendor_initial_configuration(primer_get_command, primer_send_response);

    hw_uart_write_wait_busy();

    hw_reset(NULL);
}
