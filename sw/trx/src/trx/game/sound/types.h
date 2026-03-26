#pragma once

#include <trx/game/sound/enum.h>

#include <stdint.h>

typedef struct {
    int16_t number;
    int16_t volume;
    int32_t range;
    int32_t randomness;
    int8_t pitch;
    union {
        struct {
            uint16_t mode_bits : 2;
            uint16_t num_samples : 4;
            uint16_t reserved : 6;
            uint16_t no_pan : 1;
            uint16_t randomize_pitch : 1;
            uint16_t randomize_volume : 1;
            uint16_t : 1;
        };
        uint16_t all;
    } flags;
    SAMPLE_MODE mode;
} SAMPLE_INFO;
