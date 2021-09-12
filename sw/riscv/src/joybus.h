#ifndef JOYBUS_H__
#define JOYBUS_H__


#include "sys.h"


enum eeprom_type {
    EEPROM_NONE,
    EEPROM_4K,
    EEPROM_16K,
};


void joybus_init (void);
void joybus_set_eeprom (enum eeprom_type eeprom_type);
void process_joybus (void);


#endif
