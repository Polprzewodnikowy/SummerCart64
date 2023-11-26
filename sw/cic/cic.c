// Original code sourced from https://github.com/jago85/UltraCIC_C

// MIT License

// Copyright (c) 2019 Jan Goldacker
// Copyright (c) 2022-2023 Mateusz Faderewski

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
#include <stddef.h>
#include <stdint.h>


typedef struct {
    volatile uint32_t CIC_CONFIG[2];
    volatile uint32_t GPIO;
} ext_regs_t;

#define EXT ((ext_regs_t *) (0xC0000000UL))

#define CIC_DQ                      (1 << 0)
#define CIC_CLK                     (1 << 1)
#define CIC_RESET                   (1 << 2)
#define CIC_INVALID_REGION          (1 << 3)

#define CIC_IS_RUNNING()            (EXT->GPIO & CIC_RESET)
#define CIC_CLK_WAIT_LOW()          { while ((EXT->GPIO & (CIC_RESET | CIC_CLK)) == (CIC_RESET | CIC_CLK)); }
#define CIC_CLK_WAIT_HIGH()         { while ((EXT->GPIO & (CIC_RESET | CIC_CLK)) == CIC_RESET); }
#define CIC_DQ_GET()                (EXT->GPIO & CIC_DQ)
#define CIC_DQ_SET(v)               { EXT->GPIO = ((v) ? CIC_DQ : 0); }
#define CIC_CLK_GET()               (EXT->GPIO & (CIC_RESET | CIC_CLK))
#define CIC_NOTIFY_INVALID_REGION() { EXT->GPIO = (CIC_INVALID_REGION | CIC_DQ); }


typedef struct {
    bool cic_disabled;
    bool cic_64dd_mode;
    bool cic_region;
    uint8_t cic_seed;
    uint8_t cic_checksum[6];
} cic_config_t;

static cic_config_t config;

static uint8_t cic_ram[32];
static uint8_t cic_x105_ram[30];

static const uint8_t cic_ram_init[2][32] = {{
    0xE, 0x0, 0x9, 0xA, 0x1, 0x8, 0x5, 0xA, 0x1, 0x3, 0xE, 0x1, 0x0, 0xD, 0xE, 0xC,
    0x0, 0xB, 0x1, 0x4, 0xF, 0x8, 0xB, 0x5, 0x7, 0xC, 0xD, 0x6, 0x1, 0xE, 0x9, 0x8
}, {
    0xE, 0x0, 0x4, 0xF, 0x5, 0x1, 0x2, 0x1, 0x7, 0x1, 0x9, 0x8, 0x5, 0x7, 0x5, 0xA,
    0x0, 0xB, 0x1, 0x2, 0x3, 0xF, 0x8, 0x2, 0x7, 0x1, 0x9, 0x8, 0x1, 0x1, 0x5, 0xC
}};


static void cic_die (void) {
    while (CIC_IS_RUNNING());
}

static void cic_init (void) {
    CIC_DQ_SET(1);

    while (!CIC_IS_RUNNING());

    uint32_t cic_config[2];

    cic_config[0] = EXT->CIC_CONFIG[0];
    cic_config[1] = EXT->CIC_CONFIG[1];

    config.cic_disabled = (cic_config[0] & (1 << 26));
    config.cic_64dd_mode = (cic_config[0] & (1 << 25));
    config.cic_region = (cic_config[0] & (1 << 24));
    config.cic_seed = ((cic_config[0] >> 16) & 0xFF);
    config.cic_checksum[0] = ((cic_config[0] >> 8) & 0xFF);
    config.cic_checksum[1] = (cic_config[0] & 0xFF);
    config.cic_checksum[2] = ((cic_config[1] >> 24) & 0xFF);
    config.cic_checksum[3] = ((cic_config[1] >> 16) & 0xFF);
    config.cic_checksum[4] = ((cic_config[1] >> 8) & 0xFF);
    config.cic_checksum[5] = (cic_config[1] & 0xFF);

    if (config.cic_disabled) {
        cic_die();
    }
}

static uint8_t cic_read (void) {
    uint8_t value;
    CIC_CLK_WAIT_LOW();
    value = CIC_DQ_GET() ? 1 : 0;
    CIC_CLK_WAIT_HIGH();
    return value;
}

static void cic_write (uint8_t value) {
    CIC_CLK_WAIT_LOW();
    CIC_DQ_SET(value);
    CIC_CLK_WAIT_HIGH();
    CIC_DQ_SET(1);
}

static uint8_t cic_read_nibble (void) {
    uint8_t data = 0;
    for (int i = 0; i < 4; i++) {
        data = ((data << 1) | cic_read());
    }
    return data;
}

static void cic_write_nibble (uint8_t data) {
    cic_write(data & 0x08);
    cic_write(data & 0x04);
    cic_write(data & 0x02);
    cic_write(data & 0x01);
}

static void cic_write_ram_nibbles (uint8_t index) {
    do {
        cic_write_nibble(cic_ram[index++]);
    } while ((index & 0x0F) != 0);
}

static void cic_encode_round (uint8_t index) {
    uint8_t data = cic_ram[index++];
    do {
        data = ((((data + 1) & 0x0F) + cic_ram[index]) & 0x0F);
        cic_ram[index++] = data;
    } while ((index & 0x0F) != 0);
}

static void cic_write_id (void) {
    if (config.cic_64dd_mode) {
        CIC_CLK_WAIT_LOW();
        while (CIC_CLK_GET() == CIC_RESET) {
            if (!CIC_DQ_GET()) {
                cic_die();
            }
        }
    } else {
        cic_write(0);
    }
    cic_write(config.cic_region ? 1 : 0);
    cic_write(0);
    cic_write(1);
}

static void cic_write_seed (void) {
    cic_ram[0x0A] = 0x0B;
    cic_ram[0x0B] = 0x05;
    cic_ram[0x0C] = (config.cic_seed >> 4);
    cic_ram[0x0D] = config.cic_seed;
    cic_ram[0x0E] = (config.cic_seed >> 4);
    cic_ram[0x0F] = config.cic_seed;
    cic_encode_round(0x0A);
    cic_encode_round(0x0A);

    uint32_t timeout = 100000;
    do {
        if (timeout == 0) {
            CIC_NOTIFY_INVALID_REGION();
            cic_die();
        }
    } while (timeout-- && (CIC_CLK_GET() == (CIC_RESET | CIC_CLK)));

    cic_write_ram_nibbles(0x0A);
}

static void cic_write_checksum (void) {
    for (int i = 0; i < 4; i++) {
        cic_ram[i] = 0x00;
    }
    for (int i = 0; i < 6; i++) {
        cic_ram[(i * 2) + 4] = ((config.cic_checksum[i] >> 4) & 0x0F);
        cic_ram[(i * 2) + 5] = (config.cic_checksum[i] & 0x0F);
    }
    cic_encode_round(0x00);
    cic_encode_round(0x00);
    cic_encode_round(0x00);
    cic_encode_round(0x00);
    cic_write(0);
    cic_write_ram_nibbles(0x00);
}

static void cic_init_ram (void) {
    for (int i = 0; i < 32; i++) {
        cic_ram[i] = cic_ram_init[config.cic_region ? 1 : 0][i];
    }
    cic_ram[0x01] = cic_read_nibble();
    cic_ram[0x11] = cic_read_nibble();
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
        a = (x + 0x0F);
        x = (a & 0x0F);
    } while (x != 0x0F);
}

static void cic_compare_mode (void) {
    cic_round(&cic_ram[0x10]);
    cic_round(&cic_ram[0x10]);
    cic_round(&cic_ram[0x10]);

    uint8_t index = (cic_ram[0x17] & 0x0F);
    if (index == 0) {
        index = 1;
    }
    index |= 0x10;

    do {
        cic_read();
        cic_write(cic_ram[index] & 0x01);
        if (config.cic_region) {
            index--;
        } else {
            index++;
        }
    } while (index & 0x0F);
}

static void cic_x105_algorithm (void) {
    uint8_t a = 5;
    uint8_t carry = 1;

    for (int i = 0; i < 30; ++i) {
        if (!(cic_x105_ram[i] & 0x01)) {
            a += 8;
        }
        if (!(a & 0x02)) {
            a += 4;
        }
        a = ((a + cic_x105_ram[i]) & 0x0F);
        cic_x105_ram[i] = a;
        if (!carry) {
            a += 7;
        }
        a = ((a + cic_x105_ram[i]) & 0x0F);
        a = (a + cic_x105_ram[i] + carry);
        if (a >= 0x10) {
            carry = 1;
            a -= 0x10;
        } else {
            carry = 0;
        }
        a = (~(a) & 0x0F);
        cic_x105_ram[i] = a;
    }
}

static void cic_x105_mode (void) {
    cic_write_nibble(0x0A);
    cic_write_nibble(0x0A);

    for (int i = 0; i < 30; i++) {
        cic_x105_ram[i] = cic_read_nibble();
    }

    cic_x105_algorithm();

    cic_write(0);

    for (int i = 0; i < 30; i++) {
        cic_write_nibble(cic_x105_ram[i]);
    }
}

static void cic_soft_reset (void) {
    volatile uint32_t timeout = 100000;
    CIC_CLK_WAIT_LOW();
    while ((timeout--) && CIC_IS_RUNNING());
    cic_write(0);
}


__attribute__((naked)) void cic_main (void) {
    while (true) {
        cic_init();

        cic_write_id();
        cic_write_seed();
        cic_write_checksum();
        cic_init_ram();

        while (CIC_IS_RUNNING()) {
            uint8_t cmd = 0;
            cmd |= (cic_read() << 1);
            cmd |= cic_read();

            if (cmd == 0) {
                cic_compare_mode();
            } else if (cmd == 2) {
                cic_x105_mode();
            } else if (cmd == 3) {
                cic_soft_reset();
            } else {
                cic_die();
            }
        }
    }
}
