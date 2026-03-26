#pragma once

#include <trx/core/filesystem.h>
#include <trx/game/lara/enum.h>
#include <trx/game/savegame/enum.h>
#include <trx/game/stats/types.h>

typedef struct {
    uint8_t small_medipacks;
    uint8_t large_medipacks;
    uint16_t pistol_ammo;
    uint16_t magnum_ammo;
    uint16_t autos_ammo;
    uint16_t desert_eagle_ammo;
    uint16_t uzi_ammo;
    uint16_t shotgun_ammo;
    int32_t lara_hitpoints;
    LARA_GUN_STATE gun_status;
    LARA_GUN_TYPE equipped_gun_type;
    LARA_GUN_TYPE holsters_gun_type;
    LARA_GUN_TYPE back_gun_type;
    uint8_t num_scions;
    uint8_t num_quest_item_1;
    uint8_t num_quest_item_2;
    uint8_t num_quest_item_3;
    uint8_t num_quest_item_4;
    uint16_t m16_ammo;
    uint16_t mp5_ammo;
    uint16_t grenade_ammo;
    uint16_t rocket_ammo;
    uint16_t harpoon_ammo;
    uint16_t flares;

    struct {
        bool available;
        bool costume;
        bool has_pistols;
        bool has_magnums;
        bool has_autos;
        bool has_desert_eagle;
        bool has_uzis;
        bool has_shotgun;
        bool has_m16;
        bool has_mp5;
        bool has_grenade;
        bool has_rocket;
        bool has_harpoon;
    } flags;

    bool level_completed;
    int32_t prev_level;
    bool hurt_allies;

    LEVEL_STATS stats;
} RESUME_INFO;

typedef enum {
    SAVEGAME_SLOT_POOL_NORMAL = 0,
    SAVEGAME_SLOT_POOL_QUICK = 1,
    SAVEGAME_SLOT_POOL_NUMBER_OF,
} SAVEGAME_SLOT_POOL;

typedef struct {
    SAVEGAME_SLOT_POOL pool;
    int32_t index;
} SAVEGAME_SLOT_REF;

typedef struct {
    char *full_path;
    int32_t counter;
    int32_t level_num;
    char *level_title;
    int16_t initial_version;
    bool is_quick;
    struct {
        bool restart;
        bool select_level;
    } features;
} SAVEGAME_INFO;
