#pragma once

#include <trx/game/objects/ids.h>
#include <trx/game/stats/const.h>

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t max_pickup_secret_count;
    size_t max_kill_count;
    size_t max_kill_ally_count;
    size_t max_kill_non_ally_count;
    size_t max_pickup_count;
    size_t max_secret_count;
    uint32_t all_secrets_mask;

    struct {
        int32_t item_num;
        uint32_t secret_mask;
    } secret_item_masks[STATS_MAX_SECRETS];

    struct {
        bool taken;
        OBJECT_ID assigned_object_id;
        int32_t item_num;
    } secret_objects[STATS_MAX_SECRETS];
} LEVEL_MAX_STATS;

typedef struct STATS_COMMON {
    uint32_t timer;
    uint32_t kill_count;
    uint32_t ammo_used;
    uint32_t ammo_hits;
    uint32_t distance_travelled;
    double medipacks_used;
    uint16_t pickup_count;
    int32_t death_count;
    uint16_t secrets_mask;
    uint16_t secret_count;
} STATS_COMMON;

typedef struct {
    struct STATS_COMMON;
    uint16_t secret_flags;
} LEVEL_STATS;

typedef struct {
    STATS_COMMON stats;
    LEVEL_MAX_STATS max_stats;
} FINAL_STATS;
