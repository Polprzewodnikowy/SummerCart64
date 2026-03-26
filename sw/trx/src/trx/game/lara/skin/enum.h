#pragma once

#include <stdint.h>

typedef int32_t LARA_SKIN_TYPE;

#define LARA_SKIN_TYPE_DEFAULT (-1)

typedef enum {
    // clang-format off
    BRAID_MODE_NONE,          // No body adjustments needed
    BRAID_MODE_TR1_HEAD_ONLY, // Head replacement only (no backpack present)
    BRAID_MODE_TR1_FULL,      // Head and torso replacement
    BRAID_MODE_TR1_MAULED,    // Head and mauled torso replacement
    BRAID_MODE_TR1_GOLD,      // Gold head and torso replacement
    NUM_BRAID_MODES,
    // clang-format on
} LARA_SKIN_BRAID_MODE;

typedef enum {
    EXTRA_MESH_TR1_BRAID_DEFAULT_HEAD,
    EXTRA_MESH_TR1_BRAID_COMBAT_HEAD,
    EXTRA_MESH_TR1_BRAID_DEFAULT_TORSO,
    EXTRA_MESH_TR1_BRAID_MAULED_TORSO,
    EXTRA_MESH_TR1_BRAID_GOLD_HEAD,
    EXTRA_MESH_TR1_BRAID_GOLD_TORSO,
    EXTRA_MESH_DAGGER_HAND,
    EXTRA_MESH_DAGGER_HIPS,
    EXTRA_MESH_OAR,
    EXTRA_MESH_SPANNER,
    EXTRA_MESH_DRINK_CAN,
    EXTRA_MESH_GLASSES_OPAQUE,
    EXTRA_MESH_GLASSES_TRANSPARENT,
    NUM_EXTRA_MESHES,
} LARA_SKIN_EXTRA_MESH;

typedef enum {
    // clang-format off
    EQUIPMENT_TYPE_NONE   = 0,
    EQUIPMENT_TYPE_WEAPON = 1,
    EQUIPMENT_TYPE_EXTRA  = 2,
    // clang-format on
} LARA_SKIN_EQUIPMENT_TYPE;
