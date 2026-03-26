#pragma once

#include <trx/core/math/const.h>

#define LOGIC_FPS 30

#define STEP_L 256
#define WALL_L 1024 // = 1 << WALL_SHIFT
#define WALL_SHIFT 10

#define GRAVITY 6
#define FAST_FALL_SPEED 128

#define FOV_VALUE_PASSPORT 80
#define FOV_MODE_GAME FOV_MODE_PC
#define FOV_MODE_PASSPORT FOV_MODE_PS1
#define FOV_MODE_CUTSCENE (g_TRVersion == 1 ? FOV_MODE_VERTICAL : FOV_MODE_PC)
