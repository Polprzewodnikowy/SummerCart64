#pragma once

#include <trx/game/const.h>

// clang-format off
#define NO_CAMERA                (-1)
#define CAMERA_DEFAULT_DISTANCE  (WALL_L * 3 / 2) // = 1536
#define CAMERA_MAX_HEAD_ROTATION (50 * DEG_1) // = 9100
#define CAMERA_MIN_HEAD_ROTATION (-CAMERA_MAX_HEAD_ROTATION) // = -9100
#define CAMERA_MAX_HEAD_TILT     (85 * DEG_1) // = 15470
#define CAMERA_MIN_HEAD_TILT     (-CAMERA_MAX_HEAD_TILT) // = -15470
#define CAMERA_HEAD_TURN         (4 * DEG_1) // = 728
#define CAMERA_COMBAT_SPEED      8
#define CAMERA_LOOK_SPEED        4
// clang-format on
