// Original code sourced from https://github.com/jago85/UltraCIC_C

// MIT License

// Copyright (c) 2019 Jan Goldacker
// Copyright (c) 2022-2025 Mateusz Faderewski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdbool.h>

#include "cic.h"


static uint8_t cic_ram[32];

static const uint8_t cic_ram_init[2][16] = {{
    0xE0, 0x9A, 0x18, 0x5A, 0x13, 0xE1, 0x0D, 0xEC, 0x0B, 0x14, 0xF8, 0xB5, 0x7C, 0xD6, 0x1E, 0x98
}, {
    0xE0, 0x4F, 0x51, 0x21, 0x71, 0x98, 0x57, 0x5A, 0x0B, 0x12, 0x3F, 0x82, 0x71, 0x98, 0x11, 0x5C
}};


static void cic_die (cic_step_t reason) {
    CIC_SET_STEP(reason);

    while (CIC_IS_RUNNING());
}

static void cic_wait_power_on (void) {
    CIC_SET_STEP(CIC_STEP_POWER_OFF);

    CIC_INIT();

    while (!CIC_IS_RUNNING());
}

static uint8_t cic_load_config (void) {
    CIC_SET_STEP(CIC_STEP_LOAD_CONFIG);

    if (EXT->CONFIG.DISABLED) {
        cic_die(CIC_STEP_DIE_DISABLED);
    }

    return ((EXT->CONFIG.REGION) ? 1 : 0);
}

static uint8_t cic_read_bit (void) {
    uint8_t value;
    CIC_CLK_WAIT_LOW();
    value = (CIC_DQ_GET() ? 1 : 0);
    CIC_CLK_WAIT_HIGH();
    return value;
}

static void cic_write_bit (uint8_t value) {
    CIC_CLK_WAIT_LOW();
    CIC_DQ_SET(value);
    CIC_CLK_WAIT_HIGH();
    CIC_DQ_SET(1);
}

static uint8_t cic_read (int length) {
    uint8_t data = 0;
    for (int i = 0; i < length; i++) {
        data = ((data << 1) | cic_read_bit());
    }
    return data;
}

static void cic_write (uint8_t data, int length) {
    for (int i = (length - 1); i >= 0; i--) {
        cic_write_bit(data & (1 << i));
    }
}

static void cic_write_ram_nibbles (uint8_t index) {
    do {
        cic_write(cic_ram[index++], 4);
    } while ((index & 0x0F) != 0);
}

static void cic_encode_round (uint8_t index) {
    uint8_t data = cic_ram[index++];
    do {
        data = ((((data + 1) & 0x0F) + cic_ram[index]) & 0x0F);
        cic_ram[index++] = data;
    } while ((index & 0x0F) != 0);
}

static void cic_write_id (uint8_t region) {
    CIC_SET_STEP(CIC_STEP_ID);

    uint8_t is_64dd = EXT->CONFIG.TYPE;

    if (is_64dd) {
        CIC_CLK_WAIT_LOW();
        while (CIC_CLK_IS_LOW()) {
            if (!CIC_DQ_GET()) {
                cic_die(CIC_STEP_DIE_64DD);
            }
        }
    }

    uint8_t id = (
        (0b0 << 3) |        // CIC type: low = cartridge, high = 64DD
        (region << 2) |     // Region: low = NTSC/MPAL, high = PAL
        0b01                // Constant bit pattern
    );
    int length = (is_64dd ? 3 : 4);

    cic_write(id, length);
}

static void cic_write_seed (void) {
    CIC_SET_STEP(CIC_STEP_SEED);

    uint8_t seed = EXT->CONFIG.SEED;

    cic_ram[10] = 0x0B;
    cic_ram[11] = 0x05;
    cic_ram[12] = (seed >> 4);
    cic_ram[13] = seed;
    cic_ram[14] = (seed >> 4);
    cic_ram[15] = seed;
    cic_encode_round(10);
    cic_encode_round(10);

    CIC_TIMEOUT_START();

    cic_write_ram_nibbles(10);

    if (CIC_TIMEOUT_ELAPSED()) {
        CIC_NOTIFY_INVALID_REGION();
        cic_die(CIC_STEP_DIE_INVALID_REGION);
    }

    CIC_TIMEOUT_STOP();
}

static void cic_write_checksum (void) {
    CIC_SET_STEP(CIC_STEP_CHECKSUM);

    for (int i = 0; i < 4; i++) {
        cic_ram[i] = 0;
    }

    for (int i = 0; i < 6; i++) {
        uint8_t value = EXT->CONFIG.CHECKSUM[i];
        cic_ram[(i * 2) + 4] = ((value >> 4) & 0x0F);
        cic_ram[(i * 2) + 5] = (value & 0x0F);
    }

    cic_encode_round(0);
    cic_encode_round(0);
    cic_encode_round(0);
    cic_encode_round(0);

    cic_read(1);

    cic_write_ram_nibbles(0);
}

static void cic_init_ram (uint8_t region) {
    CIC_SET_STEP(CIC_STEP_INIT_RAM);

    for (int i = 0; i < 16; i++) {
        uint8_t value = cic_ram_init[region][i];
        cic_ram[(i * 2)] = ((value >> 4) & 0x0F);
        cic_ram[(i * 2) + 1] = (value & 0x0F);
    }

    cic_ram[1] = cic_read(4);
    cic_ram[17] = cic_read(4);
}

static cic_command_t cic_get_command (void) {
    CIC_SET_STEP(CIC_STEP_COMMAND);

    return cic_read(2);
}

static void cic_exchange_bytes (uint8_t *a, uint8_t *b) {
    uint8_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void cic_round (uint8_t *m) {
    uint8_t a, b, x;

    x = m[15];
    a = x;

    do {
        b = 1;
        a += (m[b] + 1);
        m[b] = a;
        b++;
        a += (m[b] + 1);
        cic_exchange_bytes(&a, &m[b]);
        m[b] = ~(m[b]);
        b++;
        a &= 0x0F;
        a += ((m[b] & 0x0F) + 1);
        if (a < 16) {
            cic_exchange_bytes(&a, &m[b]);
            b++;
        }
        a += m[b];
        m[b] = a;
        b++;
        a += m[b];
        cic_exchange_bytes(&a, &m[b]);
        b++;
        a &= 0x0F;
        a += 8;
        if (a < 16) {
            a += m[b];
        }
        cic_exchange_bytes(&a, &m[b]);
        b++;
        do {
            a += (m[b] + 1);
            m[b] = a;
            b++;
            b &= 0x0F;
        } while (b != 0);
        a = (x + 15);
        x = (a & 0x0F);
    } while (x != 15);
}

static void cic_compare_mode (uint8_t region) {
    CIC_SET_STEP(CIC_STEP_COMPARE);

    cic_round(&cic_ram[16]);
    cic_round(&cic_ram[16]);
    cic_round(&cic_ram[16]);

    uint8_t index = (cic_ram[23] & 0x0F);
    if (index == 0) {
        index = 1;
    }
    index |= (1 << 4);

    do {
        cic_read(1);
        cic_write(cic_ram[index], 1);
        if (region) {
            index -= 1;
        } else {
            index += 1;
        }
    } while (index & 0x0F);
}

static void cic_x105_algorithm (uint8_t *payload) {
    uint8_t a = 5;
    uint8_t carry = 1;

    for (int i = 0; i < 30; ++i) {
        if (!(payload[i] & (1 << 0))) {
            a += 8;
        }
        if (!(a & (1 << 1))) {
            a += 4;
        }
        a = ((a + payload[i]) & 0x0F);
        payload[i] = a;
        if (!carry) {
            a += 7;
        }
        a = ((a + payload[i]) & 0x0F);
        a = (a + payload[i] + carry);
        if (a >= 16) {
            carry = 1;
            a -= 16;
        } else {
            carry = 0;
        }
        a = (~(a) & 0x0F);
        payload[i] = a;
    }
}

static void cic_x105_mode (void) {
    CIC_SET_STEP(CIC_STEP_X105);

    uint8_t payload[30];

    cic_write(0xAA, 8);

    for (int i = 0; i < 30; i++) {
        payload[i] = cic_read(4);
    }

    cic_x105_algorithm(payload);

    cic_write(0, 1);

    for (int i = 0; i < 30; i++) {
        cic_write(payload[i], 4);
    }
}

static void cic_soft_reset (void) {
    CIC_SET_STEP(CIC_STEP_RESET_BUTTON);

    CIC_CLK_WAIT_LOW();

    CIC_TIMEOUT_START();
    while ((!CIC_TIMEOUT_ELAPSED()) && CIC_IS_RUNNING());
    CIC_TIMEOUT_STOP();

    cic_write(0, 1);
}


__attribute__((naked)) void cic_main (void) {
    while (true) {
        cic_wait_power_on();

        uint8_t region = cic_load_config();

        cic_write_id(region);

        cic_write_seed();

        cic_write_checksum();

        cic_init_ram(region);

        while (CIC_IS_RUNNING()) {
            switch (cic_get_command()) {
                case CIC_COMMAND_COMPARE:
                    cic_compare_mode(region);
                    break;

                case CIC_COMMAND_X105:
                    cic_x105_mode();
                    break;

                case CIC_COMMAND_SOFT_RESET:
                    cic_soft_reset();
                    break;

                default:
                    cic_die(CIC_STEP_DIE_COMMAND);
                    break;
            }
        }
    }
}
