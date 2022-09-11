// Original code from https://github.com/jago85/UltraCIC_C licensed under the MIT License

#include <stdbool.h>
#include "cic.h"
#include "hw.h"
#include "led.h"
#include "rtc.h"
#include "task.h"


typedef enum {
    REGION_NTSC,
    REGION_PAL,
    __REGION_MAX
} cic_region_t;


static volatile bool cic_enabled = false;
static volatile bool cic_detect_enabled;

static volatile uint8_t cic_next_rd;
static volatile uint8_t cic_next_wr;

static volatile bool cic_dd_mode = false;
static volatile uint8_t cic_seed = 0x3F;
static volatile uint8_t cic_checksum[6] = { 0xA5, 0x36, 0xC0, 0xF1, 0xD8, 0x59 };

static uint8_t cic_ram[32];
static uint8_t cic_x105_ram[30];

static const uint8_t cic_ram_init[2][32] = {{
    0x0E, 0x00, 0x09, 0x0A, 0x01, 0x08, 0x05, 0x0A, 0x01, 0x03, 0x0E, 0x01, 0x00, 0x0D, 0x0E, 0x0C,
    0x00, 0x0B, 0x01, 0x04, 0x0F, 0x08, 0x0B, 0x05, 0x07, 0x0C, 0x0D, 0x06, 0x01, 0x0E, 0x09, 0x08
}, {
    0x0E, 0x00, 0x04, 0x0F, 0x05, 0x01, 0x02, 0x01, 0x07, 0x01, 0x09, 0x08, 0x05, 0x07, 0x05, 0x0A,
    0x00, 0x0B, 0x01, 0x02, 0x03, 0x0F, 0x08, 0x02, 0x07, 0x01, 0x09, 0x08, 0x01, 0x01, 0x05, 0x0C
}};


static void cic_irq_reset_falling (void) {
    cic_enabled = false;
    hw_gpio_set(GPIO_ID_N64_CIC_DQ);
    led_clear_error(LED_ERROR_CIC);
}

static void cic_irq_reset_rising (void) {
    cic_enabled = true;
    task_set_ready_and_reset(TASK_ID_CIC);
}

static void cic_irq_clk_falling (void) {
    if (cic_enabled) {
        if (!cic_next_wr) {
            hw_gpio_reset(GPIO_ID_N64_CIC_DQ);
        }
        cic_next_rd = hw_gpio_get(GPIO_ID_N64_CIC_DQ) ? 1 : 0;
        task_set_ready(TASK_ID_CIC);
    }
}

static void cic_irq_clk_rising (void) {
    hw_gpio_set(GPIO_ID_N64_CIC_DQ);
    if (cic_detect_enabled) {
        cic_detect_enabled = false;
        if (!hw_gpio_get(GPIO_ID_N64_CIC_DQ)) {
            cic_enabled = false;
        }
    }
}

static uint8_t cic_read (void) {
    cic_next_wr = 1;
    task_yield();
    return cic_next_rd;
}

static void cic_write (uint8_t bit) {
    cic_next_wr = bit;
    task_yield();
}

static void cic_start_detect (void) {
    cic_detect_enabled = cic_dd_mode;
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

static void cic_write_id (cic_region_t region) {
    cic_start_detect();
    cic_write(cic_dd_mode ? 1 : 0);
    cic_write(region == REGION_PAL ? 1 : 0);
    cic_write(0);
    cic_write(1);
}

static void cic_write_id_failed (void) {
    uint8_t current_region = rtc_get_region();
    uint8_t next_region = (current_region == REGION_NTSC) ? REGION_PAL : REGION_NTSC;
    rtc_set_region(next_region);
    led_blink_error(LED_ERROR_CIC);
}

static void cic_write_seed (void) {
    cic_ram[0x0A] = 0x0B;
    cic_ram[0x0B] = 0x05;
    cic_ram[0x0C] = (cic_seed >> 4);
    cic_ram[0x0D] = cic_seed;
    cic_ram[0x0E] = (cic_seed >> 4);
    cic_ram[0x0F] = cic_seed;
    cic_encode_round(0x0A);
    cic_encode_round(0x0A);
    cic_write_ram_nibbles(0x0A);
}

static void cic_write_checksum (void) {
    for (int i = 0; i < 4; i++) {
        cic_ram[i] = 0x00;
    }
    for (int i = 0; i < 6; i++) {
        cic_ram[(i * 2) + 4] = ((cic_checksum[i] >> 4) & 0x0F);
        cic_ram[(i * 2) + 5] = (cic_checksum[i] & 0x0F);
    }
    cic_encode_round(0x00);
    cic_encode_round(0x00);
    cic_encode_round(0x00);
    cic_encode_round(0x00);
    cic_write(0);
    cic_write_ram_nibbles(0x00);
}

static void cic_init_ram (cic_region_t region) {
    if (region < __REGION_MAX) {
        for (int i = 0; i < 32; i++) {
            cic_ram[i] = cic_ram_init[region][i];
        }
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

static void cic_compare_mode (cic_region_t region) {
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
        if (region == REGION_PAL) {
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

static void cic_soft_reset_timeout (void) {
    hw_gpio_reset(GPIO_ID_N64_CIC_DQ);
    task_set_ready(TASK_ID_CIC);
}

static void cic_soft_reset (void) {
    cic_read();
    hw_tim_setup(TIM_ID_CIC, 550, cic_soft_reset_timeout);
    task_yield();
}


void cic_set_parameters (uint32_t *args) {
    cic_dd_mode = (args[0] >> 24) & 0x01;
    cic_seed = (args[0] >> 16) & 0xFF;
    cic_checksum[0] = (args[0] >> 8) & 0xFF;
    cic_checksum[1] = args[0] & 0xFF;
    cic_checksum[2] = (args[1] >> 24) & 0xFF;
    cic_checksum[3] = (args[1] >> 16) & 0xFF;
    cic_checksum[4] = (args[1] >> 8) & 0xFF;
    cic_checksum[5] = args[1] & 0xFF;
}

void cic_hw_init (void) {
    hw_gpio_irq_setup(GPIO_ID_N64_RESET, GPIO_IRQ_FALLING, cic_irq_reset_falling);
    hw_gpio_irq_setup(GPIO_ID_N64_RESET, GPIO_IRQ_RISING, cic_irq_reset_rising);
    hw_gpio_irq_setup(GPIO_ID_N64_CIC_CLK, GPIO_IRQ_FALLING, cic_irq_clk_falling);
    hw_gpio_irq_setup(GPIO_ID_N64_CIC_CLK, GPIO_IRQ_RISING, cic_irq_clk_rising);
}

void cic_task (void) {
    while (!hw_gpio_get(GPIO_ID_N64_RESET)) {
        task_yield();
    }

    cic_region_t region = rtc_get_region();
    if (region >= __REGION_MAX) {
        region = REGION_NTSC;
        rtc_set_region(region);
    }

    cic_write_id(region);

    hw_tim_setup(TIM_ID_CIC, 1000, cic_write_id_failed);
    cic_write_seed();
    hw_tim_stop(TIM_ID_CIC);

    cic_write_checksum();
    cic_init_ram(region);

    while(1) {
        uint8_t cmd = 0;
        cmd |= (cic_read() << 1);
        cmd |= cic_read();

        switch (cmd) {
            case 0: {
                cic_compare_mode(region);
                break;
            }

            case 2: {
                cic_x105_mode();
                break;
            }

            case 3: {
                cic_soft_reset();
                break;
            }

            case 1:
            default: {
                while (1) {
                    task_yield();
                }
                break;
            }
        }
    }
}
