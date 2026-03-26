#pragma once

#include <trx/core/math.h>
#include <trx/game/objects/ids.h>
#include <trx/game/types.h>

typedef struct {
    XYZ_32 pos;
    XYZ_16 rot;
    int16_t room_num;
    OBJECT_ID object_id;
    int16_t next_free;
    int16_t next_active;
    int16_t speed;
    int16_t fall_speed;
    int16_t frame_num;
    int16_t counter;
    int16_t shade;

    int32_t flag1, flag2;

    struct {
        struct {
            XYZ_32 pos;
            XYZ_16 rot;
        } result, prev;
    } interp;
} EFFECT;
