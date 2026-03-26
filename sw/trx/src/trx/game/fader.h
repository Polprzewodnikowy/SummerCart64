#pragma once

#include <trx/game/clock/timer.h>

#include <stdint.h>

typedef struct {
    bool from_current;
    float initial;
    float target;

    // This value controls how much to keep the last frame after the animation
    // is done (1.0 = one second).
    float debuff;
    float duration;
} FADER_ARGS;

typedef struct {
    FADER_ARGS args;
    CLOCK_TIMER timer;
} FADER;

void Fader_InitTo(FADER *fader, float initial, float target, float duration);
void Fader_InitToHold(
    FADER *fader, float initial, float target, float duration, float debuff);
void Fader_InitFromCurrent(FADER *fader, float target, float duration);
void Fader_InitFromCurrentHold(
    FADER *fader, float target, float duration, float debuff);
bool Fader_IsActive(const FADER *fader);

float Fader_GetCurrentValue(const FADER *fader);
