#pragma once

#include <trx/game/fader.h>
#include <trx/game/phase/control.h>

typedef struct PHASE PHASE;

typedef PHASE_CONTROL (*PHASE_START_FUNC)(PHASE *phase);
typedef void (*PHASE_END_FUNC)(PHASE *phase);
typedef void (*PHASE_SUSPEND_FUNC)(PHASE *phase);
typedef void (*PHASE_RESUME_FUNC)(PHASE *phase);
typedef PHASE_CONTROL (*PHASE_CONTROL_FUNC)(PHASE *phase);
typedef void (*PHASE_DRAW_FUNC)(PHASE *phase);
typedef bool (*PHASE_REQUEST_FADE_TO_BLACK_FUNC)(
    PHASE *phase, FADER_ARGS *out_args);
typedef bool (*PHASE_USES_CROSS_FADE_IN_FUNC)(PHASE *phase);

typedef struct PHASE {
    PHASE_START_FUNC start;
    PHASE_END_FUNC end;
    PHASE_SUSPEND_FUNC suspend;
    PHASE_RESUME_FUNC resume;
    PHASE_CONTROL_FUNC control;
    PHASE_DRAW_FUNC draw;
    PHASE_REQUEST_FADE_TO_BLACK_FUNC request_fade_to_black;
    PHASE_USES_CROSS_FADE_IN_FUNC uses_cross_fade_in;
    void *priv;
} PHASE;
