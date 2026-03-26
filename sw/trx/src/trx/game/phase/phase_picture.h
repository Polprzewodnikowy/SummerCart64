#pragma once

#include <trx/game/phase/types.h>

typedef struct {
    const char *file_name;
    double display_time;
    double fade_in_time;
    double fade_out_time;
    bool display_time_includes_fades;
    bool loading_pic;
    bool block_cross_fade_in;
} PHASE_PICTURE_ARGS;

PHASE *Phase_Picture_Create(PHASE_PICTURE_ARGS args);
void Phase_Picture_Destroy(PHASE *phase);
