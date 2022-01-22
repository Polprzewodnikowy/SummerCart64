#ifndef USB_H__
#define USB_H__


#include "sys.h"


typedef enum {
    EVENT_ID_FSD_READ = 0xF0,
    EVENT_ID_FSD_WRITE = 0xF1,
    EVENT_ID_FSD_LOAD = 0xF2,
    EVENT_ID_FSD_STORE = 0xF3,
    EVENT_ID_DD_BLOCK = 0xF4,
    EVENT_ID_IS_VIEWER = 0xF5,
} usb_event_id_t;

typedef enum {
    CALLBACK_NONE = 0,
    CALLBACK_SDRAM_WRITE = 1,
    CALLBACK_SDRAM_READ = 2,
    CALLBACK_BUFFER_WRITE = 3,
    CALLBACK_BUFFER_READ = 4,
} usb_event_callback_t;

typedef struct {
    usb_event_id_t id;
    usb_event_callback_t trigger;
    void (*callback)(void);
} usb_event_t;


bool usb_put_event (usb_event_t *event, void *data, uint32_t length);
void usb_init (void);
void process_usb (void);


#endif
