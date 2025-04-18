#ifndef JOYBUS_H__
#define JOYBUS_H__


#include <stdbool.h>
#include <stdint.h>


typedef struct {
    uint16_t id;
    uint8_t flags;
} joybus_controller_info_t;

typedef struct {
    bool a;
    bool b;
    bool z;
    bool start;
    bool up;
    bool down;
    bool left;
    bool right;
    bool reset;
    bool l;
    bool r;
    bool c_up;
    bool c_down;
    bool c_left;
    bool c_right;
    int8_t x;
    int8_t y;
} joybus_controller_state_t;


bool joybus_get_controller_info (int port, joybus_controller_info_t *info, bool reset);
bool joybus_get_controller_state (int port, joybus_controller_state_t *state);
bool joybus_set_controller_player (int port);


#endif
