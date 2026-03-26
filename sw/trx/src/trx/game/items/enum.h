#pragma once

typedef enum {
    // clang-format off
    DS_CARRIED   = 0,
    DS_FALLING   = 1,
    DS_DROPPED   = 2,
    DS_COLLECTED = 3,
    // clang-format on
} DROP_STATUS;

typedef enum {
    // clang-format off
    IF_ONE_SHOT_SWITCH      = 0x0040,
    IF_ONE_SHOT_ANTITRIGGER = 0x0080,
    IF_ONE_SHOT             = 0x0100,
    IF_CODE_BITS            = 0x3E00,
    IF_REVERSE              = 0x4000,
    IF_INVISIBLE            = 0x0100,
    IF_KILLED               = 0x8000,
    // clang-format on
} ITEM_FLAG;

typedef enum {
    // clang-format off
    AI_GUARD    = 1 << 0,
    AI_AMBUSH   = 1 << 1,
    AI_PATROL_1 = 1 << 2,
    AI_MODIFY   = 1 << 3,
    AI_FOLLOW   = 1 << 4,
    // clang-format on
} AI_BITS;

typedef enum {
    // clang-format off
    IS_INACTIVE    = 0,
    IS_ACTIVE      = 1,
    IS_DEACTIVATED = 2,
    IS_INVISIBLE   = 3,
    // clang-format on
} ITEM_STATUS;
