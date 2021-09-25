#include "i2c.h"


enum phase {
    PHASE_START,
    PHASE_ADDRESS,
    PHASE_DATA,
    PHASE_STOP,
};

struct process {
    enum phase phase;
    uint8_t address;
    uint8_t *data;
    uint8_t length;
    bool write;
    bool generate_stop;
    bool first_byte_transferred;
    bool busy;
    bool done;
    bool failed;
};

static struct process p;


bool i2c_busy (void) {
    return p.busy;
}

bool i2c_done (void) {
    return p.done;
}

bool i2c_failed (void) {
    return p.failed;
}

void i2c_trx (uint8_t address, uint8_t *data, uint8_t length, bool write, bool generate_stop) {
    p.phase = PHASE_START;
    p.address = address;
    p.data = data;
    p.length = length;
    p.write = write;
    p.generate_stop = generate_stop;
    p.first_byte_transferred = false;
    p.busy = true;
    p.done = false;
    p.failed = false;
}


void i2c_init (void) {
    I2C->SCR = I2C_SCR_STOP;
    p.busy = false;
    p.done = false;
    p.failed = false;
}


void process_i2c (void) {
    if (p.busy && (!(I2C->SCR & I2C_SCR_BUSY))) {
        switch (p.phase) {
            case PHASE_START:
                I2C->SCR = I2C_SCR_START;
                p.phase = PHASE_ADDRESS;
                break;

            case PHASE_ADDRESS:
                I2C->SCR = 0;
                I2C->DR = p.address | (p.write ? 0 : (1 << 0));
                p.phase = PHASE_DATA;
                break;

            case PHASE_DATA:
                if (p.write) {
                    p.failed |= (!(I2C->SCR & I2C_SCR_ACK));
                    I2C->DR = *p.data++;
                    if (p.length == 1) {
                        if (p.generate_stop) {
                            p.phase = PHASE_STOP;
                        } else {
                            p.busy = false;
                            p.done = true;
                        }
                    }
                } else {
                    if (p.first_byte_transferred) {
                        *(p.data++) = I2C->DR;
                    }
                    if (p.length >= 1) {
                        I2C->SCR = (p.length > 1) ? I2C_SCR_MACK : 0;
                        I2C->DR = 0xFF;
                        p.first_byte_transferred = true;
                    }
                    if (p.length == 0) {
                        p.phase = PHASE_STOP;
                    }
                }
                p.length -= 1;
                break;

            case PHASE_STOP:
                if (p.write) {
                    p.failed |= (!(I2C->SCR & I2C_SCR_ACK));
                }
                I2C->SCR = I2C_SCR_STOP;
                p.busy = false;
                p.done = true;
                break;
        }
    }
}
