#ifndef I2C_H__
#define I2C_H__


#include "sys.h"


bool i2c_busy (void);
bool i2c_done (void);
bool i2c_failed (void);
void i2c_trx (uint8_t address, uint8_t *data, uint8_t length, bool write, bool generate_stop);
void i2c_init (void);
void process_i2c (void);


#endif
