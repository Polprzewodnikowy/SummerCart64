#pragma once

typedef enum {
    LS_INVALID = -1,
} LARA_STATE;

typedef enum {
    LS_TRX_INVALID = -1,
#define X_CATALOG_ID(enum_value) enum_value,
#include <trx/game/catalog/lara_states.def>
#undef X_CATALOG_ID
    LS_NUMBER_OF,
} LARA_TRX_STATE;

typedef enum {
    LA_INVALID = -1,
} LARA_ANIMATION;

typedef enum {
    LA_TRX_INVALID = -1,
#define X_CATALOG_ID(enum_value) enum_value,
#include <trx/game/catalog/lara_anims.def>
#undef X_CATALOG_ID
    LA_NUMBER_OF,
} LARA_TRX_ANIMATION;

// clang-format off
typedef enum {
    LWS_ABOVE_WATER  = 0,
    LWS_UNDERWATER   = 1,
    LWS_SURFACE      = 2,
    LWS_CHEAT        = 3,
    LWS_WADE         = 4,
} LARA_WATER_STATE;

typedef enum {
    LGS_ARMLESS    = 0,
    LGS_HANDS_BUSY = 1,
    LGS_DRAW       = 2,
    LGS_UNDRAW     = 3,
    LGS_READY      = 4,
    LGS_SPECIAL    = 5,
} LARA_GUN_STATE;

typedef enum {
    LM_HIPS      = 0,
    LM_THIGH_L   = 1,
    LM_CALF_L    = 2,
    LM_FOOT_L    = 3,
    LM_THIGH_R   = 4,
    LM_CALF_R    = 5,
    LM_FOOT_R    = 6,
    LM_TORSO     = 7,
    LM_UARM_R    = 8,
    LM_LARM_R    = 9,
    LM_HAND_R    = 10,
    LM_UARM_L    = 11,
    LM_LARM_L    = 12,
    LM_HAND_L    = 13,
    LM_HEAD      = 14,
    LM_FIRST     = LM_HIPS,
    LM_NUMBER_OF = 15,
} LARA_MESH;
// clang-format on

// clang-format off
typedef enum {
    LS_EXTRA_BREATH         = 0,
    LS_EXTRA_TREX_KILL      = 1,
    LS_EXTRA_SCION_PICKUP_1 = 2,
    LS_EXTRA_USE_MIDAS      = 3,
    LS_EXTRA_MIDAS_KILL     = 4,
    LS_EXTRA_SCION_PICKUP_2 = 5,
    LS_EXTRA_TORSO_KILL     = 6,
    LS_EXTRA_PLUNGER        = 7,
    LS_EXTRA_START_ANIM     = 8,
    LS_EXTRA_AIRLOCK        = 9,
    LS_EXTRA_SHARK_KILL     = 10,
    LS_EXTRA_YETI_KILL      = 11,
    LS_EXTRA_GONG_BONG      = 12,
    LS_EXTRA_GUARD_KILL     = 13,
    LS_EXTRA_PULL_DAGGER    = 14,
    LS_EXTRA_START_HOUSE    = 15,
    LS_EXTRA_END_HOUSE      = 16,
    LS_EXTRA_SHIVA_KILL     = 17,
    LS_EXTRA_RAPIDS_DROWN   = 18,
    LS_EXTRA_TRAIN_KILL     = 19,
    LS_EXTRA_JAIL_WAKE_UP   = 20,
    LS_EXTRA_WILLARD_KILL   = 21,
    LS_EXTRA_NUMBER_OF,
} LARA_EXTRA_STATE;
// clang-format on

// clang-format off
typedef enum {
    LGT_UNKNOWN      = -1, // for legacy saves
    LGT_UNARMED      = 0,
    LGT_PISTOLS      = 1,
    LGT_MAGNUMS      = 2,
    LGT_UZIS         = 3,
    LGT_SHOTGUN      = 4,
    LGT_M16          = 5,
    LGT_GRENADE      = 6,
    LGT_HARPOON      = 7,
    LGT_FLARE        = 8,
    LGT_SKIDOO       = 9,
    LGT_AUTOS        = 10,
    LGT_DESERT_EAGLE = 11,
    LGT_MP5          = 12,
    LGT_ROCKET       = 13,
    NUM_WEAPONS,
} LARA_GUN_TYPE;
// clang-format on

// clang-format off
typedef enum {
    LF_G_AIM_START    = 0,
    LF_G_AIM_BEND     = 1,
    LF_G_AIM_EXTEND   = 3,
    LF_G_AIM_END      = 4,
    LF_G_UNDRAW_START = 5,
    LF_G_UNDRAW_BEND  = 6,
    LF_G_UNDRAW_END   = 12,
    LF_G_DRAW_START   = 13,
    LF_G_DRAW_END     = 23,
    LF_G_RECOIL_START = 24,
    LF_G_RECOIL_END   = 32,
} LARA_GUN_ANIMATION_FRAME;
// clang-format on
