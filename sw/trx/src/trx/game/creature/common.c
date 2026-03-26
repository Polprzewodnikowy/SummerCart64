#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/core/utils.h>
#include <trx/game/camera/vars.h>
#include <trx/game/collision/los.h>
#include <trx/game/creature.h>
#include <trx/game/game_flow.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/creatures/skidoo_driver.h>
#include <trx/game/objects/creatures/tribe_boss.h>
#include <trx/game/objects/vars.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

// clang-format off
#define M_FLOAT_SPEED      32
#define M_MAX_DISTANCE     (g_TRVersion < 3 ? WALL_L * 30 : STEP_L * 125) // = 30720 (TR1/2), 32000 (TR3)
#define M_ATTACK_RANGE     SQUARE(WALL_L * 3) // = 0x900000 = 9437184
#define M_ESCAPE_CHANCE    2048
#define M_RECOVER_CHANCE   256
#define M_TARGET_TOLERANCE 0x400000
#define M_MAX_TILT         (3 * DEG_1) // = 546
#define M_MAX_HEAD_CHANGE  (5 * DEG_1) // = 910
#define M_MAX_JOINT_CHANGE (5 * DEG_1) // = 910
#define M_HEAD_ARC         (g_TRVersion == 1 ? FRONT_ARC : 0x3000) // = 16384 (TR1), 12288 (TR2)
#define M_JOINT_ARC        0x3000
#define M_MAX_X_ROT        (20 * DEG_1) // = 3640
#define M_BITE_DISTANCE    (g_TRVersion < 3 ? STEP_L : STEP_L * 2)
// clang-format on

static const LARA_TRX_STATE m_CrouchShiftStates[] = {
    // clang-format off
    LS_CROUCH_IDLE,
    LS_CROUCH_ROLL,
    LS_CROUCH_TURN_LEFT,
    LS_CROUCH_TURN_RIGHT,
    LS_CRAWL_IDLE,
    LS_CRAWL_FORWARD,
    LS_CRAWL_TURN_LEFT,
    LS_CRAWL_TURN_RIGHT,
    LS_TRX_INVALID, // sentinel
    // clang-format on
};

static bool M_TestSwitchOrKill(
    const int16_t item_num, const OBJECT_ID target_id)
{
    if (Object_Get(target_id)->loaded) {
        return true;
    }

    LOG_WARNING(
        "Object %d is not loaded; item %d cannot be converted.", target_id,
        item_num);
    Item_Kill(item_num);
    return false;
}

static void M_GetBaddieTarget(const int16_t item_num, const bool goody)
{
    ITEM *const lara_item = Lara_GetItem();
    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    ITEM *best_item = nullptr;
    int32_t best_distance = INT32_MAX;
    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        const int16_t target_item_num = LOT_GetBaddieSlot(i)->item_num;
        if (target_item_num == NO_ITEM || target_item_num == item_num) {
            continue;
        }

        ITEM *const target = Item_Get(target_item_num);
        const OBJECT_ID obj_id = target->object_id;
        if (goody && !Creature_IsAllyTargetingEnemy(target)) {
            continue;
        } else if (!goody && !Creature_IsAlly(target)) {
            continue;
        }

        const int32_t dx = (target->pos.x - item->pos.x) >> 6;
        const int32_t dy = (target->pos.y - item->pos.y) >> 6;
        const int32_t dz = (target->pos.z - item->pos.z) >> 6;
        const int32_t distance = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
        if (distance < best_distance) {
            best_item = target;
            best_distance = distance;
        }
    }

    if (best_item == nullptr) {
        if (!goody || Creature_IsHostile(item)) {
            creature->enemy = lara_item;
        } else {
            creature->enemy = nullptr;
        }
        return;
    }

    if (!goody || Creature_IsHostile(item)) {
        const int32_t dx = (lara_item->pos.x - item->pos.x) >> 6;
        const int32_t dy = (lara_item->pos.y - item->pos.y) >> 6;
        const int32_t dz = (lara_item->pos.z - item->pos.z) >> 6;
        const int32_t distance = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
        if (distance < best_distance) {
            best_item = lara_item;
            best_distance = distance;
        }
    }

    const ITEM *const target = creature->enemy;
    if (target == nullptr || target->status != IS_ACTIVE) {
        creature->enemy = best_item;
    } else {
        const int32_t dx = (target->pos.x - item->pos.x) >> 6;
        const int32_t dy = (target->pos.y - item->pos.y) >> 6;
        const int32_t dz = (target->pos.z - item->pos.z) >> 6;
        const int32_t distance = SQUARE(dz) + SQUARE(dy) + SQUARE(dx);
        if (distance < best_distance + M_TARGET_TOLERANCE) {
            creature->enemy = best_item;
        }
    }
}

static ITEM *M_ChooseEnemy(const ITEM *const item)
{
    CREATURE *const creature = item->creature_data;
    if (Creature_IsAlly(item)) {
        M_GetBaddieTarget(creature->item_num, true);
    } else if (Creature_IsAllyTargetingEnemy(item)) {
        M_GetBaddieTarget(creature->item_num, false);
    } else {
        creature->enemy = Lara_GetItem();
    }

    if (creature->enemy != nullptr) {
        return creature->enemy;
    }
    return Lara_GetItem();
}

static bool M_SwitchToWater(
    const int16_t item_num, const int32_t wh, const HYBRID_INFO *const info)
{
    if (wh == NO_HEIGHT) {
        return false;
    }

    ITEM *const item = Item_Get(item_num);

    if (item->hit_points <= 0
        && item->current_anim_state == info->land.death_state) {
        // Dead land creatures should remain in their pose permanently.
        return false;
    }

    // Switch to the water creature, but only switch animations if the creature
    // is alive to avoid savegame reload issues.
    if (!M_TestSwitchOrKill(item_num, info->water.id)) {
        return false;
    }

    item->object_id = info->water.id;
    if (item->hit_points > 0) {
        Item_SwitchToAnim(item, info->water.active_anim, 0);
        item->current_anim_state = Item_GetAnim(item)->current_anim_state;
        item->goal_anim_state = item->current_anim_state;
    }
    item->pos.y = wh;

    return true;
}

static bool M_SwitchToLand(
    const int16_t item_num, const int32_t wh, const HYBRID_INFO *const info)
{
    if (wh != NO_HEIGHT) {
        return false;
    }

    if (!M_TestSwitchOrKill(item_num, info->land.id)) {
        return false;
    }

    ITEM *const item = Item_Get(item_num);

    // Switch to the land creature regardless of death state.
    item->object_id = info->land.id;
    item->rot.x = 0;

    if (item->hit_points > 0) {
        Item_SwitchToAnim(item, info->land.active_anim, 0);
        item->current_anim_state = Item_GetAnim(item)->current_anim_state;
        item->goal_anim_state = item->current_anim_state;

    } else {
        Item_SwitchToAnim(item, info->land.death_anim, -1);
        item->current_anim_state = info->land.death_state;
        item->goal_anim_state = item->current_anim_state;

        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
        item->floor = Room_GetHeight(sector, item->pos);
        item->pos.y = item->floor;
        Item_UpdateRoom(item_num, room_num);
    }

    return true;
}

const ITEM *M_GetBaddieOverlap(const int16_t item_num)
{
    const ITEM *item = Item_Get(item_num);
    if (item->speed == 0 || item->hit_points <= 0) {
        return nullptr;
    }

    const int32_t x = item->pos.x;
    const int32_t y = item->pos.y;
    const int32_t z = item->pos.z;
    int32_t radius = Object_Get(item->object_id)->radius;
    if (g_TRVersion < 3) {
        radius = SQUARE(radius);
    }

    int16_t link = Room_Get(item->room_num)->item_num;
    while (link != NO_ITEM && link != item_num) {
        item = Item_Get(link);
        if (item != Lara_GetItem() && item->status == IS_ACTIVE) {
            if (g_TRVersion >= 3 && item->hit_points > 0) {
                const int32_t dx = ABS(item->pos.x - x);
                const int32_t dz = ABS(item->pos.z - z);
                const int32_t distance =
                    dx > dz ? dx + (dz >> 1) : dz + (dx >> 1);
                const int32_t item_radius = Object_Get(item->object_id)->radius;
                if (distance < item_radius + radius) {
                    return item;
                }
            } else if (g_TRVersion < 3 && item->speed != 0) {
                const XYZ_32 delta = {
                    item->pos.x - x,
                    item->pos.y - y,
                    item->pos.z - z,
                };
                const int32_t distance =
                    SQUARE(delta.x) + SQUARE(delta.y) + SQUARE(delta.z);
                if (distance < radius) {
                    return item;
                }
            }
        }

        link = item->next_item;
    }

    return nullptr;
}

void Creature_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    // TODO: remove GF check once demo config reset is run before level load
    if (g_Config.gameplay.enable_enemy_rotation
        || GF_GetCurrentLevel()->type == GFL_DEMO) {
        item->rot.y += (Random_GetControl() - DEG_90) >> 1;
    }
    item->collidable = true;
    item->creature_data = nullptr;
    item->extra_rotations = nullptr;
}

bool Creature_Activate(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->status != IS_INVISIBLE) {
        return true;
    }

    if (!LOT_EnableBaddieAI(item_num, false)) {
        return false;
    }

    item->status = IS_ACTIVE;
    return true;
}

void Creature_AIInfo(ITEM *const item, AI_INFO *const info)
{
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    ITEM *enemy = g_TRVersion >= 3 ? creature->enemy : M_ChooseEnemy(item);
    if (enemy == nullptr) {
        enemy = Lara_GetItem();
        creature->enemy = enemy;
    }
    const int16_t *const zone = Box_GetLotZone(&creature->lot);
    const bool use_fixed_fly_zone =
        g_TRVersion == 3 && creature->lot.setup.fly != 0;

    {
        const ROOM *const room = Room_Get(item->room_num);
        item->box_num =
            Room_GetWorldSector(room, item->pos.x, item->pos.z)->box;
        info->zone_num =
            use_fixed_fly_zone ? BOX_FIXED_FLY_ZONE : zone[item->box_num];
    }

    {
        const ROOM *const room = Room_Get(enemy->room_num);
        enemy->box_num =
            Room_GetWorldSector(room, enemy->pos.x, enemy->pos.z)->box;
        info->enemy_zone_num =
            use_fixed_fly_zone ? BOX_FIXED_FLY_ZONE : zone[enemy->box_num];
    }

    const BOX_INFO *const enemy_box = Box_GetBox(enemy->box_num);
    // TODO: TR3 defines non-LOT creatures, like cobras and handles them
    // differently here and in LOT initialisation.
    if (((enemy_box->overlap_index & creature->lot.setup.block_mask) != 0)
        || (creature->lot.node[item->box_num].search_num
            == (creature->lot.search_num | BOX_BLOCKED_SEARCH))) {
        info->enemy_zone_num |= BOX_BLOCKED;
    }

    const OBJECT *const obj = Object_Get(item->object_id);
    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    const XZ_32 pivot = {
        .x = (obj->pivot_length * Math_Sin(item->rot.y)) >> W2V_SHIFT,
        .z = (obj->pivot_length * Math_Cos(item->rot.y)) >> W2V_SHIFT,
    };

    XZ_32 enemy_pos = {
        .x = enemy->pos.x,
        .z = enemy->pos.z,
    };
    if (g_TRVersion >= 3) {
        const int16_t enemy_angle =
            enemy == lara_item ? lara->move_angle : enemy->rot.y;
        enemy_pos.x += (14 * enemy->speed * Math_Sin(enemy_angle)) >> W2V_SHIFT;
        enemy_pos.z += (14 * enemy->speed * Math_Cos(enemy_angle)) >> W2V_SHIFT;
    }

    int32_t x = enemy_pos.x - pivot.x - item->pos.x;
    int32_t z = enemy_pos.z - pivot.z - item->pos.z;
    int32_t y = item->pos.y - enemy->pos.y; // sic, reversed

    if (enemy == lara_item && Lara_HasState(m_CrouchShiftStates)) {
        y -= STEP_L * 3 / 2;
    }

    const bool too_far = ABS(z) > M_MAX_DISTANCE || ABS(x) > M_MAX_DISTANCE;
    if (creature->enemy == nullptr || (g_TRVersion == 3 && too_far)) {
        info->distance = INT32_MAX;
    } else if (g_TRVersion < 3 && too_far) {
        info->distance = SQUARE(M_MAX_DISTANCE);
    } else {
        info->distance = SQUARE(x) + SQUARE(z);
    }

    const int16_t angle = Math_Atan(z, x);
    info->angle = angle - item->rot.y;
    info->enemy_facing = angle - enemy->rot.y + DEG_180;
    info->ahead = info->angle > -FRONT_ARC && info->angle < FRONT_ARC;
    info->bite = info->ahead
        && ABS(enemy->pos.y - item->pos.y) <= M_BITE_DISTANCE
        && (g_TRVersion == 1 || enemy->hit_points > 0);

    x = ABS(x);
    z = ABS(z);
    if (x > z) {
        info->x_angle = Math_Atan(x + (z >> 1), y);
    } else {
        info->x_angle = Math_Atan(z + (x >> 1), y);
    }

    if (g_TRVersion == 3) {
        if (!creature->hurt_by_lara && creature->enemy == lara_item
            && !Creature_IsHostile(item) && Creature_IsAlly(item)) {
            creature->enemy = nullptr;
            info->ahead = false;
            info->bite = false;
            info->distance = INT32_MAX;
        }
    }
}

bool Creature_EnsureHabitat(
    const int16_t item_num, int32_t *const wh, const HYBRID_INFO *const info)
{
    // Test the environment for a hybrid creature. Record the water height and
    // return whether or not a type conversion has taken place.
    const ITEM *const item = Item_Get(item_num);
    const int32_t water_height = Room_GetWaterHeight(item->pos, item->room_num);
    if (wh != nullptr) {
        *wh = water_height;
    }
    if (item->status == IS_INACTIVE) {
        return false;
    }

    return item->object_id == info->land.id
        ? M_SwitchToWater(item_num, water_height, info)
        : M_SwitchToLand(item_num, water_height, info);
}

void Creature_Mood(
    const ITEM *const item, const AI_INFO *const info, const bool violent)
{
    Creature_UpdateMood(item, info, violent);
    Creature_ApplyMood(item, info, violent);
}

void Creature_UpdateMood(
    const ITEM *const item, const AI_INFO *const info, const bool violent)
{
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    LOT_INFO *const lot = &creature->lot;
    const ITEM *enemy = creature->enemy;
    if (lot->node[item->box_num].search_num
        == (lot->search_num | BOX_BLOCKED_SEARCH)) {
        lot->required_box = NO_BOX;
    }

    if (creature->mood != MOOD_ATTACK && lot->required_box != NO_BOX
        && !Box_ValidBox(item, info->zone_num, lot->target_box)) {
        if (info->zone_num == info->enemy_zone_num) {
            creature->mood = MOOD_BORED;
        }
        lot->required_box = NO_BOX;
    }

    const MOOD_TYPE mood = creature->mood;
    if (enemy == nullptr) {
        creature->mood = MOOD_BORED;
        enemy = Lara_GetItem();
    } else if (
        enemy->hit_points <= 0
        && (g_TRVersion < 3 || enemy == Lara_GetItem())) {
        creature->mood = MOOD_BORED;
    } else if (violent) {
        switch (mood) {
        case MOOD_BORED:
        case MOOD_STALK:
            if (info->zone_num == info->enemy_zone_num) {
                creature->mood = MOOD_ATTACK;
            } else if (item->hit_status) {
                creature->mood = MOOD_ESCAPE;
            }
            break;

        case MOOD_ATTACK:
            if (info->zone_num != info->enemy_zone_num) {
                creature->mood = MOOD_BORED;
            }
            break;

        case MOOD_ESCAPE:
            if (info->zone_num == info->enemy_zone_num) {
                creature->mood = MOOD_ATTACK;
            }
            break;
        }
    } else {
        switch (mood) {
        case MOOD_BORED:
        case MOOD_STALK:
            if (g_TRVersion >= 3 && creature->alerted
                && info->zone_num != info->enemy_zone_num) {
                creature->mood =
                    info->distance > WALL_L * 3 ? MOOD_STALK : MOOD_BORED;
            } else if (
                g_TRVersion < 3 && item->hit_status
                && (Random_GetControl() < M_ESCAPE_CHANCE
                    || info->zone_num != info->enemy_zone_num)) {
                creature->mood = MOOD_ESCAPE;
            } else if (info->zone_num == info->enemy_zone_num) {
                if (info->distance < M_ATTACK_RANGE
                    || (creature->mood == MOOD_STALK
                        && lot->required_box == NO_BOX)) {
                    creature->mood = MOOD_ATTACK;
                } else {
                    creature->mood = MOOD_STALK;
                }
            }
            break;

        case MOOD_ATTACK:
            if (item->hit_status
                && (Random_GetControl() < M_ESCAPE_CHANCE
                    || info->zone_num != info->enemy_zone_num)) {
                creature->mood = g_TRVersion < 3 ? MOOD_ESCAPE : MOOD_STALK;
            } else if (
                info->zone_num != info->enemy_zone_num
                && (g_TRVersion < 3 || info->distance > 6 * WALL_L)) {
                creature->mood = MOOD_BORED;
            }
            break;

        case MOOD_ESCAPE:
            if (info->zone_num == info->enemy_zone_num
                && Random_GetControl() < M_RECOVER_CHANCE) {
                creature->mood = MOOD_STALK;
            }
            break;
        }
    }

    if (mood != creature->mood) {
        if (mood == MOOD_ATTACK) {
            Box_TargetBox(lot, lot->target_box);
        }
        lot->required_box = NO_BOX;
    }
}

void Creature_ApplyMood(
    const ITEM *const item, const AI_INFO *const info, const bool violent)
{
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    LOT_INFO *const lot = &creature->lot;
    const ITEM *enemy = creature->enemy;

    switch (creature->mood) {
    case MOOD_BORED: {
        const int16_t box_num =
            lot->node[lot->zone_count * Random_GetControl() / 0x7FFF].box_num;
        if (Box_ValidBox(item, info->zone_num, box_num)) {
            if (Box_StalkBox(item, enemy, box_num) && creature->enemy != nullptr
                && enemy->hit_points > 0) {
                Box_TargetBox(lot, box_num);
                if (g_TRVersion < 3) {
                    creature->mood = MOOD_STALK;
                }
            } else if (lot->required_box == NO_BOX) {
                Box_TargetBox(lot, box_num);
            }
        }
        break;
    }

    case MOOD_ATTACK: {
        const int32_t smartness = Object_Get(item->object_id)->smartness;
        if (smartness < 0 || Random_GetControl() < smartness) {
            lot->target = enemy->pos;
            lot->required_box = enemy->box_num;
            if (lot->setup.fly != 0
                && Lara_GetLaraInfo()->water_status == LWS_ABOVE_WATER) {
                lot->target.y += Item_GetBestFrame(enemy)->bounds.min.y;
            }
        }
        break;
    }

    case MOOD_ESCAPE: {
        const int16_t box_num =
            lot->node[lot->zone_count * Random_GetControl() / 0x7FFF].box_num;
        if (Box_ValidBox(item, info->zone_num, box_num)
            && lot->required_box == NO_BOX) {
            if (Box_EscapeBox(item, enemy, box_num)) {
                Box_TargetBox(lot, box_num);
            } else if (
                info->zone_num == info->enemy_zone_num
                && Box_StalkBox(item, enemy, box_num)
                && (g_TRVersion < 3 || !violent)) {
                Box_TargetBox(lot, box_num);
                creature->mood = MOOD_STALK;
            }
        }
        break;
    }

    case MOOD_STALK: {
        if (lot->required_box == NO_BOX
            || !Box_StalkBox(item, enemy, lot->required_box)) {
            const int16_t box_num =
                lot->node[lot->zone_count * Random_GetControl() / 0x7FFF]
                    .box_num;
            if (Box_ValidBox(item, info->zone_num, box_num)) {
                if (Box_StalkBox(item, enemy, box_num)) {
                    Box_TargetBox(lot, box_num);
                } else if (lot->required_box == NO_BOX) {
                    Box_TargetBox(lot, box_num);
                    if (info->zone_num != info->enemy_zone_num) {
                        creature->mood = MOOD_BORED;
                    }
                }
            }
        }
        break;
    }
    }

    if (lot->target_box == NO_BOX) {
        Box_TargetBox(lot, item->box_num);
    }
    Box_CalculateTarget(&creature->target, item, lot);
}

int16_t Creature_Turn(ITEM *const item, int16_t max_turn)
{
    const CREATURE *const creature = item->creature_data;
    if (creature == nullptr || max_turn == 0) {
        return 0;
    }
    if (item->speed == 0 && g_TRVersion < 3) {
        return 0;
    }

    const int32_t dx = creature->target.x - item->pos.x;
    const int32_t dz = creature->target.z - item->pos.z;

    int16_t angle = Math_Atan(dz, dx) - item->rot.y;
    if (angle > FRONT_ARC || angle < -FRONT_ARC) {
        const int32_t range = (item->speed * (1 << 14)) / max_turn;
        if (SQUARE(dx) + SQUARE(dz) < SQUARE(range)) {
            max_turn /= 2;
        }
    }

    CLAMP(angle, -max_turn, max_turn);
    item->rot.y += angle;
    return angle;
}

void Creature_Tilt(ITEM *const item, int16_t angle)
{
    angle = angle * 4 - item->rot.z;
    CLAMP(angle, -M_MAX_TILT, M_MAX_TILT);
    item->rot.z += angle;
}

void Creature_Head(ITEM *const item, const int16_t required)
{
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    int16_t change = required - creature->head_rotation;
    CLAMP(change, -M_MAX_HEAD_CHANGE, M_MAX_HEAD_CHANGE);

    creature->head_rotation += change;
    CLAMP(creature->head_rotation, -M_HEAD_ARC, M_HEAD_ARC);
}

void Creature_Neck(ITEM *const item, const int16_t required)
{
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    int16_t change = required - creature->neck_rotation;
    CLAMP(change, -M_MAX_HEAD_CHANGE, M_MAX_HEAD_CHANGE);

    creature->neck_rotation += change;
    CLAMP(creature->neck_rotation, -M_HEAD_ARC, M_HEAD_ARC);
}

void Creature_Joint(
    ITEM *const item, const int16_t joint, const int16_t required)
{
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    int16_t change = required - creature->joint_rotation[joint];
    CLAMP(change, -M_MAX_JOINT_CHANGE, M_MAX_JOINT_CHANGE);

    creature->joint_rotation[joint] += change;
    CLAMP(creature->joint_rotation[joint], -M_JOINT_ARC, M_JOINT_ARC);
}

void Creature_Float(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    item->hit_points = 0;
    item->rot.x = 0;

    const int32_t wh = Room_GetWaterHeight(item->pos, item->room_num);
    if (item->pos.y > wh) {
        item->pos.y -= M_FLOAT_SPEED;
    }
    CLAMPL(item->pos.y, wh);

    Item_Animate(item);

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    item->floor = Room_GetHeight(sector, item->pos);
    Item_UpdateRoom(item_num, room_num);
}

void Creature_Underwater(ITEM *const item, const int32_t depth)
{
    const int32_t wh = Room_GetWaterHeight(item->pos, item->room_num);
    if (item->pos.y >= wh + depth) {
        return;
    }

    item->pos.y = wh + depth;
    if (item->rot.x > 2 * DEG_1) {
        item->rot.x -= 2 * DEG_1;
    } else {
        CLAMPG(item->rot.x, 0);
    }
}

bool Creature_CanSeeEnemy(const ITEM *const item, const AI_INFO *const info)
{
    // XXX(Dash): I don't understand the need for this function,
    // when there's CanTargetEnemy().

    const CREATURE *const creature = item->creature_data;
    const ITEM *const enemy =
        creature->enemy != nullptr ? creature->enemy : Lara_GetItem();

    if (enemy == nullptr || enemy->hit_points <= 0
        || (enemy != Lara_GetItem() && enemy->creature_data == nullptr)
        || info->angle - creature->joint_rotation[2] <= -DEG_90
        || info->angle - creature->joint_rotation[2] >= DEG_90
        || info->distance >= CREATURE_SHOOT_RANGE) {
        return false;
    }

    GAME_VECTOR start = {
        .x = item->pos.x,
        .y = item->pos.y - STEP_L * 3,
        .z = item->pos.z,
        .room_num = item->room_num,
    };

    const BOUNDS_16 *const bounds = &Item_GetBestFrame(enemy)->bounds;
    GAME_VECTOR target = {
        .x = enemy->pos.x,
        .y = enemy->pos.y + ((3 * bounds->min.y + bounds->max.y) >> 2),
        .z = enemy->pos.z,
        .room_num = enemy->room_num,
    };

    return LOS_Check(&start, &target, true);
}

bool Creature_CanTargetEnemy(const ITEM *const item, const AI_INFO *const info)
{
    const CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return false;
    }

    const ITEM *const enemy =
        creature->enemy != nullptr ? creature->enemy : Lara_GetItem();
    if (enemy == nullptr || !info->ahead
        || info->distance >= CREATURE_SHOOT_RANGE) {
        return false;
    }

    GAME_VECTOR start = { .pos = item->pos, .room_num = item->room_num };
    GAME_VECTOR target = { .pos = enemy->pos, .room_num = enemy->room_num };

    if (g_TRVersion == 3) {
        if (enemy->hit_points <= 0
            || (enemy != Lara_GetItem() && enemy->creature_data == nullptr)) {
            return false;
        }

        const BOUNDS_16 *const bounds1 = &Item_GetBestFrame(item)->bounds;
        const BOUNDS_16 *const bounds2 = &Item_GetBestFrame(enemy)->bounds;

        start.pos.y += (bounds1->max.y + 3 * bounds1->min.y) >> 2;
        target.pos.y += (bounds2->max.y + 3 * bounds2->min.y) >> 2;
    } else {
        start.pos.y -= STEP_L * 3;
        target.pos.y -= STEP_L * 3;
    }

    return LOS_Check(&start, &target, true);
}

void Creature_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!Lara_TestBoundsCollide(item, coll->radius)) {
        return;
    }
    if (!Collide_TestCollision(item, lara_item)) {
        return;
    }

    if (!coll->enable_baddie_push) {
        return;
    }

    if (lara->water_status == LWS_UNDERWATER
        || lara->water_status == LWS_SURFACE) {
        return;
    }

    Lara_Col_ItemPush(
        item, coll,
        (g_TRVersion >= 2 || item->hit_points > 0) ? coll->enable_hit : false,
        false);
}

bool Creature_Animate(
    const int16_t item_num, const int16_t angle, const int16_t tilt)
{
    ITEM *const item = Item_Get(item_num);
    const CREATURE *const creature = item->creature_data;
    const OBJECT *const obj = Object_Get(item->object_id);
    if (creature == nullptr) {
        return false;
    }

    const LOT_INFO *const lot = &creature->lot;
    const XYZ_32 old = item->pos;

    const int16_t *const zone = Box_GetLotZone(lot);

    if (g_TRVersion >= 2 && !Object_IsType(item->object_id, g_WaterObjects)) {
        int16_t room_num = item->room_num;
        Room_GetSector(item->pos, &room_num);
        Item_UpdateRoom(item_num, room_num);
    }

    Item_Animate(item);
    if (item->status == IS_DEACTIVATED) {
        Creature_Die(item_num, false);
        return false;
    }

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    int32_t y = item->pos.y + bounds->min.y;

    int16_t room_num = item->room_num;
    XYZ_32 sample_pos = { old.x, y, old.z };
    Room_GetSector(sample_pos, &room_num);
    sample_pos.x = item->pos.x;
    sample_pos.z = item->pos.z;
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);
    int32_t height = Box_GetBox(sector->box)->height;
    int16_t next_box = lot->node[sector->box].exit_box;
    int32_t next_height =
        next_box != NO_BOX ? Box_GetBox(next_box)->height : height;

    const bool fly_check = g_TRVersion < 3 || lot->setup.fly == 0;

    const int32_t box_height = Box_GetBox(item->box_num)->height;
    if (sector->box == NO_BOX
        || (fly_check && zone[item->box_num] != zone[sector->box])
        || box_height - height > lot->setup.step
        || box_height - height < lot->setup.drop) {
        const int32_t pos_x = item->pos.x >> WALL_SHIFT;
        const int32_t pos_z = g_TRVersion < 3
            ? pos_x
            : item->pos.z >> WALL_SHIFT; // TODO: OG bug in TR1/2?
        const int32_t shift_x = old.x >> WALL_SHIFT;
        const int32_t shift_z = old.z >> WALL_SHIFT;

        if (pos_x < shift_x) {
            item->pos.x = ROUND_TO_SECTOR(old.x);
        } else if (pos_x > shift_x) {
            item->pos.x = ROUND_TO_SECTOR_END(old.x);
        }

        if (pos_z < shift_z) {
            item->pos.z = ROUND_TO_SECTOR(old.z);
        } else if (pos_z > shift_z) {
            item->pos.z = ROUND_TO_SECTOR_END(old.z);
        }

        sample_pos.x = item->pos.x;
        sample_pos.y = y;
        sample_pos.z = item->pos.z;
        sector = Room_GetSector(sample_pos, &room_num);
        height = Box_GetBox(sector->box)->height;
        next_box = lot->node[sector->box].exit_box;
        next_height =
            next_box != NO_BOX ? Box_GetBox(next_box)->height : height;
    }

    const int32_t x = item->pos.x;
    const int32_t z = item->pos.z;
    const int32_t pos_x = x & (WALL_L - 1);
    const int32_t pos_z = z & (WALL_L - 1);
    int32_t shift_x = 0;
    int32_t shift_z = 0;
    const int32_t radius = obj->radius;

    if (pos_z < radius) {
        if (Box_BadFloor(
                x, y, z - radius, height, next_height, room_num, lot)) {
            shift_z = radius - pos_z;
        }

        if (pos_x < radius) {
            if (Box_BadFloor(
                    x - radius, y, z, height, next_height, room_num, lot)) {
                shift_x = radius - pos_x;
            } else if (
                shift_z == 0
                && Box_BadFloor(
                    x - radius, y, z - radius, height, next_height, room_num,
                    lot)) {
                if (item->rot.y > -DEG_135 && item->rot.y < DEG_45) {
                    shift_z = radius - pos_z;
                } else {
                    shift_x = radius - pos_x;
                }
            }
        } else if (pos_x > WALL_L - radius) {
            if (Box_BadFloor(
                    x + radius, y, z, height, next_height, room_num, lot)) {
                shift_x = WALL_L - radius - pos_x;
            } else if (
                shift_z == 0
                && Box_BadFloor(
                    x + radius, y, z - radius, height, next_height, room_num,
                    lot)) {
                if (item->rot.y > -DEG_45 && item->rot.y < DEG_135) {
                    shift_z = radius - pos_z;
                } else {
                    shift_x = WALL_L - radius - pos_x;
                }
            }
        }
    } else if (pos_z > WALL_L - radius) {
        if (Box_BadFloor(
                x, y, z + radius, height, next_height, room_num, lot)) {
            shift_z = WALL_L - radius - pos_z;
        }

        if (pos_x < radius) {
            if (Box_BadFloor(
                    x - radius, y, z, height, next_height, room_num, lot)) {
                shift_x = radius - pos_x;
            } else if (
                shift_z == 0
                && Box_BadFloor(
                    x - radius, y, z + radius, height, next_height, room_num,
                    lot)) {
                if (item->rot.y > -DEG_45 && item->rot.y < DEG_135) {
                    shift_x = radius - pos_x;
                } else {
                    shift_z = WALL_L - radius - pos_z;
                }
            }
        } else if (pos_x > WALL_L - radius) {
            if (Box_BadFloor(
                    x + radius, y, z, height, next_height, room_num, lot)) {
                shift_x = WALL_L - radius - pos_x;
            } else if (
                shift_z == 0
                && Box_BadFloor(
                    x + radius, y, z + radius, height, next_height, room_num,
                    lot)) {
                if (item->rot.y > -DEG_135 && item->rot.y < DEG_45) {
                    shift_x = WALL_L - radius - pos_x;
                } else {
                    shift_z = WALL_L - radius - pos_z;
                }
            }
        }
    } else if (pos_x < radius) {
        if (Box_BadFloor(
                x - radius, y, z, height, next_height, room_num, lot)) {
            shift_x = radius - pos_x;
        }
    } else if (pos_x > WALL_L - radius) {
        if (Box_BadFloor(
                x + radius, y, z, height, next_height, room_num, lot)) {
            shift_x = WALL_L - radius - pos_x;
        }
    }

    item->pos.x += shift_x;
    item->pos.z += shift_z;

    if (shift_x != 0 || shift_z != 0) {
        sample_pos.x = item->pos.x;
        sample_pos.y = y;
        sample_pos.z = item->pos.z;
        sector = Room_GetSector(sample_pos, &room_num);
        item->rot.y += angle;
        Creature_Tilt(item, tilt * 2);
    }

    if (g_TRVersion < 3
        || (item->object_id != O_TREX && item->object_id != O_TREX_ALPHA)) {
        const ITEM *const hit_item = M_GetBaddieOverlap(item_num);
        if (g_TRVersion < 3 && hit_item != nullptr) {
            item->pos = old;
            return true;
        }

        if (g_TRVersion >= 3 && hit_item != nullptr) {
            const int16_t item_angle = Math_Atan(
                                           hit_item->pos.z - item->pos.z,
                                           hit_item->pos.x - item->pos.x)
                - item->rot.y;
            if (item_angle != 0) {
                if (ABS(item_angle) < 2048) {
                    item->rot.y -= item_angle;
                } else if (item_angle > 0) {
                    item->rot.y -= 2048;
                } else {
                    item->rot.y += 2048;
                }
            }
            return true;
        }
    }

    if (lot->setup.fly != 0) {
        int32_t dy = creature->target.y - item->pos.y;
        CLAMP(dy, -lot->setup.fly, lot->setup.fly);

        height =
            Room_GetHeight(sector, (XYZ_32) { item->pos.x, y, item->pos.z });
        if (item->pos.y + dy > height) {
            if (item->pos.y <= height) {
                dy = 0;
                item->pos.y = height;
            } else {
                dy = -lot->setup.fly;
                item->pos.x = old.x;
                item->pos.z = old.z;
            }
        } else if (
            fly_check || Object_IsType(item->object_id, g_WaterObjects)) {
            const int32_t ceiling = Room_GetCeiling(
                sector, (XYZ_32) { item->pos.x, y, item->pos.z });
            int32_t min_y = bounds->min.y;
            switch (item->object_id) {
            case O_ALLIGATOR:
                min_y = 0;
                break;
            case O_SHARK:
                // TODO
                // case O_WHALE:
                min_y = 128;
                break;
            default:
                break;
            }
            if (item->pos.y + min_y + dy < ceiling) {
                if (item->pos.y + min_y < ceiling) {
                    item->pos.x = old.x;
                    item->pos.z = old.z;
                    dy = lot->setup.fly;
                } else {
                    dy = 0;
                }
            }
        } else {
            sample_pos = (XYZ_32) { item->pos.x, y + STEP_L, item->pos.z };
            Room_GetSector(sample_pos, &room_num);
            const ROOM *const room = Room_Get(room_num);
            if (room->flags.underwater || room->flags.swamp) {
                dy = -lot->setup.fly;
            }
        }

        item->pos.y += dy;
        sample_pos = (XYZ_32) { item->pos.x, y, item->pos.z };
        sector = Room_GetSector(sample_pos, &room_num);
        item->floor = Room_GetHeight(sector, sample_pos);

        int16_t item_angle = item->speed != 0 ? Math_Atan(item->speed, -dy) : 0;
        if (g_TRVersion >= 2) {
            CLAMP(item_angle, -M_MAX_X_ROT, M_MAX_X_ROT);
        }

        if (item_angle < item->rot.x - DEG_1) {
            item->rot.x -= DEG_1;
        } else if (item_angle > item->rot.x + DEG_1) {
            item->rot.x += DEG_1;
        } else {
            item->rot.x = item_angle;
        }
    } else {
        sector = Room_GetSector(item->pos, &room_num);
        if (g_TRVersion == 3) {
            const int32_t ceiling = Room_GetCeiling(sector, item->pos);
            int32_t min_y = bounds->min.y;
            switch (item->object_id) {
            case O_TREX:
            case O_TREX_ALPHA:
            case O_SHIVA:
                // TODO
                // case O_MUTANT_2:
                min_y = STEP_L * 3;
                break;
            default:
                break;
            }

            if (item->pos.y + min_y < ceiling) {
                item->pos = old;
                sector = Room_GetSector(item->pos, &room_num);
            }
        }

        item->floor = Room_GetHeight(sector, item->pos);

        if (item->pos.y > item->floor) {
            item->pos.y = item->floor;
        } else if (item->floor - item->pos.y > STEP_L / 4) {
            item->pos.y += STEP_L / 4;
        } else if (item->pos.y < item->floor) {
            item->pos.y = item->floor;
        }
        item->rot.x = 0;
    }

    if (!Object_IsType(item->object_id, g_WaterObjects)) {
        // Get the room just above the enemy so that if it is in one-click high
        // water, its effects behave still as though in a dry room.
        Room_GetSector(
            (XYZ_32) { item->pos.x, item->pos.y - (STEP_L * 2), item->pos.z },
            &room_num);
        if (g_TRVersion >= 2 && Room_Get(room_num)->flags.underwater) {
            item->hit_points = 0;
        }
    }

    Item_UpdateRoom(item_num, room_num);
    return true;
}

void Creature_SpecialKill(
    ITEM *const item, const int32_t kill_anim, const int32_t kill_state,
    const int32_t lara_kill_state)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    ITEM *const lara_item = Lara_GetItem();

    Item_SwitchToAnim(item, kill_anim, 0);
    item->current_anim_state = kill_state;

    lara_item->pos = item->pos;
    lara_item->rot = item->rot;
    lara_item->fall_speed = 0;
    lara_item->gravity = false;
    lara_item->speed = 0;
    lara->air = -1;
    lara->gun_type = LGT_UNARMED;

    int16_t room_num = item->room_num;
    Item_UpdateRoom(lara->item_num, room_num);

    Lara_SwitchToExtraState(lara_kill_state);

    g_Camera.pos.room_num = lara_item->room_num;
}

void Creature_Die(const int16_t item_num, const bool explode)
{
    ITEM *const item = Item_Get(item_num);

    switch (item->object_id) {
    case O_LIZARD:
        TribeBoss_SetLizardActive(false);
        break;

    case O_DRAGON_FRONT:
    case O_TORSO:
        item->hit_points = 0;
        return;

    case O_SKIDOO_ARMED:
        if (explode) {
            Item_Explode(item_num, -1, 0);
            ITEM *const vehicle_item = Item_Get(item_num);
            vehicle_item->hit_points = 0;
            vehicle_item->status = IS_INVISIBLE;
            return;
        }
        break;

    case O_SKIDOO_DRIVER:
        if (explode) {
            Item_Explode(item_num, -1, 0);
        }
        item->hit_points = 0;
        const int16_t vehicle_item_num = SkidooDriver_GetSkidooItemNum(item);
        if (vehicle_item_num == NO_ITEM) {
            return;
        }
        ITEM *const vehicle_item = Item_Get(vehicle_item_num);
        vehicle_item->hit_points = 0;
        vehicle_item->status = IS_INVISIBLE;
        return;

    default:
        break;
    }

    item->collidable = false;
    item->hit_points = 0;
    if (explode) {
        Item_Explode(item_num, -1, 0);
        Item_Kill(item_num);
    } else {
        Item_RemoveActive(item_num);
    }

    const OBJECT *const obj = Object_Get(item->object_id);
    if (obj->intelligent) {
        LOT_DisableBaddieAI(item_num);
    }
    item->flags |= IF_ONE_SHOT;

    if (item->clear_body) {
        item->next_active = Item_GetPrevActive();
        Item_SetPrevActive(item_num);
    }

    Carrier_TestItemDrops(item_num);
}

int32_t Creature_Vault(
    const int16_t item_num, const int16_t angle, int32_t vault,
    const int32_t shift)
{
    ITEM *const item = Item_Get(item_num);
    const int16_t room_num = item->room_num;
    const XYZ_32 old = item->pos;

    Creature_Animate(item_num, angle, 0);

    if (item->floor > old.y + STEP_L * 7 / 2) {
        vault = -4;
    } else if (
        item->floor > old.y + STEP_L * 5 / 2 && item->object_id == O_MONKEY) {
        vault = -3;
    } else if (
        item->floor > old.y + STEP_L * 3 / 2 && item->object_id == O_MONKEY) {
        vault = -2;
    } else if (item->pos.y > old.y - STEP_L * 3 / 2) {
        return 0;
    } else if (item->pos.y > old.y - STEP_L * 5 / 2) {
        vault = 2;
    } else if (item->pos.y > old.y - STEP_L * 7 / 2) {
        vault = 3;
    } else {
        vault = 4;
    }

    const int32_t old_x_sector = old.x >> WALL_SHIFT;
    const int32_t old_z_sector = old.z >> WALL_SHIFT;
    const int32_t x_sector = item->pos.x >> WALL_SHIFT;
    const int32_t z_sector = item->pos.z >> WALL_SHIFT;
    if (old_z_sector == z_sector) {
        if (old_x_sector == x_sector) {
            return 0;
        }

        if (old_x_sector >= x_sector) {
            item->rot.y = -DEG_90;
            item->pos.x = (old_x_sector * WALL_L) + shift;
        } else {
            item->rot.y = DEG_90;
            item->pos.x = (x_sector * WALL_L) - shift;
        }
    } else if (old_x_sector == x_sector) {
        if (old_z_sector >= z_sector) {
            item->rot.y = -DEG_180;
            item->pos.z = (old_z_sector * WALL_L) + shift;
        } else {
            item->rot.y = 0;
            item->pos.z = (z_sector * WALL_L) - shift;
        }
    }

    item->floor = old.y;
    item->pos.y = old.y;

    Item_UpdateRoom(item_num, room_num);
    return vault;
}

int16_t Creature_Effect(
    const ITEM *const item, const BITE *const bite,
    int16_t (*const spawn)(
        int32_t x, int32_t y, int32_t z, int16_t speed, int16_t y_rot,
        int16_t room_num))
{
    XYZ_32 pos = bite->pos;
    Collide_GetJointAbsPosition(item, &pos, bite->mesh_num);
    return spawn(pos.x, pos.y, pos.z, item->speed, item->rot.y, item->room_num);
}

int16_t Creature_AIGuard(CREATURE *const creature)
{
    if (Item_Get(creature->item_num)->ai_bits & AI_MODIFY) {
        return 0;
    }

    const int32_t rnd = Random_GetControl();
    if (rnd < 256) {
        creature->head_left = true;
        creature->head_right = true;
    } else if (rnd < 384) {
        creature->head_left = true;
        creature->head_right = false;
    } else if (rnd < 512) {
        creature->head_left = false;
        creature->head_right = true;
    }

    if (creature->head_left && creature->head_right) {
        return 0;
    }
    if (creature->head_left) {
        return -DEG_90;
    }
    if (creature->head_right) {
        return DEG_90;
    }
    return 0;
}

static bool M_SameZone(const CREATURE *const creature, ITEM *const target_item)
{
    if (creature->lot.setup.fly != 0) {
        return true;
    }

    int16_t *const zone = Box_GetGroundZone(
        Room_GetFlipStatus(), (creature->lot.setup.step >> 8) - 1);
    ITEM *const item = Item_Get(creature->item_num);

    const ROOM *room = Room_Get(item->room_num);
    item->box_num = Room_GetWorldSector(room, item->pos.x, item->pos.z)->box;

    room = Room_Get(target_item->room_num);
    target_item->box_num =
        Room_GetWorldSector(room, target_item->pos.x, target_item->pos.z)->box;

    return zone[item->box_num] == zone[target_item->box_num];
}

void Creature_GetAITarget(CREATURE *const creature)
{
    ITEM *const lara_item = Lara_GetItem();
    ITEM *const item = Item_Get(creature->item_num);
    ITEM *const enemy = creature->enemy;
    const OBJECT_ID enemy_object_id =
        enemy != nullptr ? enemy->object_id : NO_OBJECT;

    uint8_t ai_bits = item->ai_bits;

    if (ai_bits & AI_GUARD) {
        creature->enemy = lara_item;

        if (creature->alerted) {
            item->ai_bits &= ~AI_GUARD;

            if (ai_bits & AI_AMBUSH) {
                item->ai_bits |= AI_MODIFY;
            }
        }
    } else if (ai_bits & AI_PATROL_1) {
        if (creature->alerted || creature->hurt_by_lara) {
            item->ai_bits &= ~AI_PATROL_1;

            if (ai_bits & AI_AMBUSH) {
                item->ai_bits |= AI_MODIFY;
            }
        } else if (!creature->patrol_2 && enemy_object_id != O_AI_PATROL_1) {
            for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
                ITEM *const target = Item_Get(i);

                if (target->object_id == O_AI_PATROL_1
                    && target->room_num != NO_ROOM
                    && M_SameZone(creature, target)
                    && target->rot.y == item->ai_tag) {
                    creature->enemy = target;
                    return;
                }
            }
        } else if (creature->patrol_2 && enemy_object_id != O_AI_PATROL_2) {
            for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
                ITEM *const target = Item_Get(i);

                if (target->object_id == O_AI_PATROL_2
                    && target->room_num != NO_ROOM
                    && M_SameZone(creature, target)
                    && target->rot.y == item->ai_tag) {
                    creature->enemy = target;
                    return;
                }
            }
        } else if (
            ABS(enemy->pos.x - item->pos.x) < 768
            && ABS(enemy->pos.y - item->pos.y) < 768
            && ABS(enemy->pos.z - item->pos.z) < 768) {
            Room_TestTriggers(enemy);
            creature->patrol_2 = !creature->patrol_2;
        }
    } else if (ai_bits & AI_AMBUSH) {
        if (ai_bits & AI_MODIFY || creature->hurt_by_lara) {
            if (enemy_object_id != O_AI_AMBUSH) {
                for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
                    ITEM *const target = Item_Get(i);

                    if (target->object_id == O_AI_AMBUSH
                        && target->room_num != NO_ROOM
                        && M_SameZone(creature, target)
                        && (target->rot.y == item->ai_tag
                            || item->object_id == O_MONKEY)) {
                        creature->enemy = target;
                        return;
                    }
                }
            } else if (item->object_id != O_MONKEY) {
                if (ABS(enemy->pos.x - item->pos.x) < 768
                    && ABS(enemy->pos.y - item->pos.y) < 768
                    && ABS(enemy->pos.z - item->pos.z) < 768) {
                    Room_TestTriggers(enemy);
                    creature->reached_goal = 1;
                    creature->enemy = lara_item;
                    item->ai_bits &= ~(AI_AMBUSH | AI_MODIFY);
                    item->ai_bits |= AI_GUARD;
                    creature->alerted = false;
                }
            }
        } else {
            creature->enemy = lara_item;
        }
    } else if (ai_bits & AI_FOLLOW) {
        if (creature->hurt_by_lara) {
            creature->enemy = lara_item;
            creature->alerted = true;
            item->ai_bits &= ~AI_FOLLOW;
        } else if (item->hit_status) {
            item->ai_bits &= ~AI_FOLLOW;
        } else if (enemy_object_id != O_AI_FOLLOW) {
            for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
                ITEM *const target = Item_Get(i);

                if (target->object_id == O_AI_FOLLOW
                    && target->room_num != NO_ROOM
                    && M_SameZone(creature, target)
                    && target->rot.y == item->ai_tag) {
                    creature->enemy = target;
                    return;
                }
            }
        } else if (
            ABS(enemy->pos.x - item->pos.x) < 768
            && ABS(enemy->pos.y - item->pos.y) < 768
            && ABS(enemy->pos.z - item->pos.z) < 768) {
            creature->reached_goal = 1;
            item->ai_bits &= ~AI_FOLLOW;
        }
    } else if (item->object_id == O_MONKEY && item->carried_item == nullptr) {
        if (creature->hurt_by_lara
            && g_Config.gameplay.fix_monkey_pickup_priority) {
            creature->enemy = lara_item;
            return;
        }
        if (item->ai_bits == AI_MODIFY) {
            if (enemy_object_id != O_KEY_ITEM_4) {
                for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
                    ITEM *const target = Item_Get(i);

                    if (target->object_id == O_KEY_ITEM_4
                        && target->room_num != NO_ROOM && !target->ai_bits
                        && target->status != IS_INVISIBLE && !target->clear_body
                        && M_SameZone(creature, target)) {
                        creature->enemy = target;
                        return;
                    }
                }
            }
        } else if (enemy_object_id != O_SMALL_MEDIPACK_ITEM) {
            for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
                ITEM *const target = Item_Get(i);
                if (target->object_id == O_SMALL_MEDIPACK_ITEM
                    && target->room_num != NO_ROOM && !target->ai_bits
                    && target->status != IS_INVISIBLE && !target->clear_body
                    && M_SameZone(creature, target)) {
                    creature->enemy = target;
                    return;
                }
            }
        }
    }
}
