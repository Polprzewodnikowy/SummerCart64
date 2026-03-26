#pragma once

#include <trx/game/game_flow/types.h>

typedef enum {
    PHASE_ACTION_CONTINUE,
    PHASE_ACTION_NO_WAIT,
    PHASE_ACTION_END,
    PHASE_ACTION_END_FAST,
} PHASE_ACTION;

// Status returned upon every logical frame by the control routine.
//
// 1. To carry on executing current phase, .action member should be set to
//    either PHASE_ACTION_CONTINUE, which will let the cycle continue, draw the
//    phase and wait one frame before repeating the cycle, or
//    PHASE_ACTION_NO_WAIT which will immediately repeat the control routine
//    without drawing or waiting the current cycle. The latter is useful for
//    easier state switches.
//    The gf_cmd member is unused in this scenario.
//
// 2. To end the current phase and carry on continuing current game sequence,
//    .action member should be set to PHASE_ACTION_END, and .gf_cmd.action
//    member should be set to GF_NOOP.
//
// 3. To end the current phase and switch to another game sequence, .action
//    member should be set to PHASE_ACTION_END, and .gf_cmd.action member
//    should be set to the phase to switch to.
typedef struct {
    PHASE_ACTION action;
    GF_COMMAND gf_cmd;
} PHASE_CONTROL;
