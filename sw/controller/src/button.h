#ifndef BUTTON_H__
#define BUTTON_H__


#include <stdbool.h>


typedef enum {
    BUTTON_MODE_NONE,
    BUTTON_MODE_N64_IRQ,
    BUTTON_MODE_USB_PACKET,
    BUTTON_MODE_DD_DISK_SWAP,
} button_mode_t;


bool button_get_state (void);
bool button_set_mode (button_mode_t mode);
button_mode_t button_get_mode (void);

void button_init (void);

void button_process (void);


#endif
