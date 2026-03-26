#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_RADIUS          (WALL_L / 10)          // = 102
#define M_HIT_POINTS      28
#define M_BOX_DAMAGE      20
#define M_DAMAGE          32
#define M_ALERT_DIST      SQUARE(WALL_L)         // = 1048576
#define M_WALK_DIST       SQUARE(WALL_L * 3 / 2) // = 2359296
#define M_RUN_DIST        SQUARE(WALL_L * 2)     // = 4194304
#define M_WALK_TURN       (DEG_1 * 6)            // = 1092
#define M_RUN_TURN        (DEG_1 * 10)           // = 1820
#define M_DUCK_TURN       (DEG_1 * 1)            // = 182
#define M_SHOOT_1_CHANCE  0x2000
#define M_SHOOT_2_CHANCE  0x4000
#define M_DUCK_CHANCE     0x3
#define M_DUCK_END_CHANCE 0x1F
// clang-format on

typedef enum {
    M_STATE_NULL,
    M_STATE_WAIT,
    M_STATE_WALK,
    M_STATE_RUN,
    M_STATE_AIM_1,
    M_STATE_SHOOT_1,
    M_STATE_AIM_2,
    M_STATE_SHOOT_2,
    M_STATE_SHOOT_3A,
    M_STATE_SHOOT_3B,
    M_STATE_SHOOT_4A,
    M_STATE_AIM_3,
    M_STATE_AIM_4,
    M_STATE_DEATH,
    M_STATE_SHOOT_4B,
    M_STATE_DUCK_START,
    M_STATE_DUCKED,
    M_STATE_DUCK_AIM,
    M_STATE_DUCK_SHOOT,
    M_STATE_DUCK_WALK,
    M_STATE_DUCK_END,
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_SHOOT_1    = 1,
    M_ANIM_AIM_1      = 12,
    M_ANIM_DEATH      = 14,
    M_ANIM_WALK_STOP  = 17,
    M_ANIM_AIM_4A     = 18,
    M_ANIM_AIM_4B     = 19,
    M_ANIM_RUN_STOP_1 = 27,
    M_ANIM_RUN_STOP_2 = 28,
    // clang-format on
} M_ANIM;

static const CREATURE_GUN m_Gun = {
    .muzzle = { .pos = { 0, 160, 40 }, .mesh_num = 13 },
    .tr3_enemy_flash = true,
    .tr3_flash = { .pos = { 0, 192, 40 }, .mesh_num = 13 },
    .tr3_enemy_weapon_flags = 0,
    .tr3_flash_shade = 600,
    .tr3_flash_rot_x = -DEG_90,
};

static void M_FireFinalShot(
    ITEM *const item, int16_t *const head, int16_t *const torso_y)
{
    if (!Item_TestFrameEqual(item, 1)) {
        return;
    }

    AI_INFO info = {};
    Creature_AIInfo(item, &info);
    if (!Creature_CanTargetEnemy(item, &info) || ABS(info.angle) >= DEG_45) {
        return;
    }

    *head = info.angle;
    *torso_y = info.angle;
    Creature_Shoot(item, &info, &m_Gun, info.angle, M_DAMAGE);
    Sound_Effect(SFX_LONDON_SWAT_FIRE, &item->pos, SPM_NORMAL);
}

static void M_CalculateEnemy(ITEM *const item)
{
    CREATURE *const mp = item->creature_data;
    const ITEM *const lara_item = Lara_GetItem();
    mp->enemy = (ITEM *)lara_item;

    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    int32_t best_distance = XYZ_32_GetLength2((XYZ_32) { dx, 0, dz });

    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        const CREATURE *const creature = LOT_GetBaddieSlot(i);
        if (creature->item_num == NO_ITEM || creature == mp) {
            continue;
        }

        const ITEM *const candidate = Item_Get(creature->item_num);
        if (candidate != lara_item && candidate->object_id != O_PRISONER) {
            continue;
        }

        const XYZ_32 delta = {
            .x = candidate->pos.x - item->pos.x,
            .y = 0,
            .z = candidate->pos.z - item->pos.z,
        };
        const int32_t distance = XYZ_32_GetLength2(delta);
        if (distance < best_distance) {
            mp->enemy = (ITEM *)candidate;
            best_distance = distance;
        }
    }
}

static bool M_IsNearCover(const ITEM *const item, const AI_INFO *const info)
{
    const XYZ_32 pos =
        XYZ_32_OffsetYaw(item->pos, item->rot.y + info->angle, WALL_L);
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    const int32_t height = Room_GetHeight(sector, pos);
    return item->pos.y > height + STEPUP_HEIGHT
        && item->pos.y < height + STEPUP_HEIGHT * 3
        && info->distance > M_ALERT_DIST;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;

    int16_t angle = 0;
    int16_t head = 0;
    int16_t tilt = 0;
    int16_t torso_x = 0;
    int16_t torso_y = 0;

    if (item->box_num != NO_BOX
        && (Box_GetBox(item->box_num)->overlap_index & BOX_BLOCKED) != 0) {
        const XYZ_32 pos = {
            .x = item->pos.x,
            .y = item->pos.y - (Random_GetControl() & 0xFF) - 32,
            .z = item->pos.z,
        };
        Spawn_BloodBath(
            pos.x, pos.y, pos.z, (Random_GetControl() & 0x7F) + STEP_L / 2,
            Random_GetControl() << 1, item->room_num, 3);
        item->hit_points -= M_BOX_DAMAGE;
    }

    if (item->hit_points <= 0) {
        item->hit_points = 0;
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
        } else if ((Random_GetControl() & 3) == 0) {
            M_FireFinalShot(item, &head, &torso_y);
        }

        goto finish;
    }

    if (item->ai_bits != 0) {
        Creature_GetAITarget(creature);
    } else {
        M_CalculateEnemy(item);
    }

    AI_INFO info = {};
    Creature_AIInfo(item, &info);

    ITEM *const lara_item = Lara_GetItem();
    AI_INFO lara_info = {};
    if (creature->enemy == lara_item) {
        lara_info.distance = info.distance;
        lara_info.angle = info.angle;
    } else {
        const int32_t dx = lara_item->pos.x - item->pos.x;
        const int32_t dz = lara_item->pos.z - item->pos.z;
        lara_info.angle = Math_Atan(dz, dx) - item->rot.y;
        lara_info.distance = XYZ_32_GetLength2((XYZ_32) { dx, 0, dz });
    }

    Creature_Mood(item, &info, creature->enemy != lara_item);
    angle = Creature_Turn(item, creature->maximum_turn);
    const bool near_cover = M_IsNearCover(item, &lara_info);

    ITEM *const enemy = creature->enemy;
    creature->enemy = lara_item;
    if (item->hit_status || lara_info.distance < M_ALERT_DIST
        || Creature_CanSeeEnemy(item, &lara_info)) {
        if (!creature->alerted) {
            Sound_Effect(SFX_AMERICAN_HOY, &item->pos, SPM_NORMAL);
        }
        Creature_AlertAllGuards(item_num);
    }
    creature->enemy = enemy;

    const int16_t anim_idx = Item_GetRelativeAnim(item);
    const int16_t frame_idx = Item_GetRelativeFrame(item);
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    switch (item->current_anim_state) {
    case M_STATE_WAIT:
        head = lara_info.angle;
        creature->maximum_turn = 0;

        if (anim_idx == M_ANIM_WALK_STOP || anim_idx == M_ANIM_RUN_STOP_1
            || anim_idx == M_ANIM_RUN_STOP_2) {
            if (ABS(info.angle) < M_RUN_TURN) {
                item->rot.y += info.angle;
            } else if (info.angle < 0) {
                item->rot.y -= M_RUN_TURN;
            } else {
                item->rot.y += M_RUN_TURN;
            }
        }

        if ((item->ai_bits & AI_GUARD) != 0) {
            head = Creature_AIGuard(creature);
            item->goal_anim_state = M_STATE_WAIT;
        } else if ((item->ai_bits & AI_PATROL_1) != 0) {
            item->goal_anim_state = M_STATE_WALK;
            head = 0;
        } else if (near_cover && (lara->target == item || item->hit_status)) {
            item->goal_anim_state = M_STATE_DUCK_START;
        } else if (item->required_anim_state == M_STATE_DUCK_START) {
            item->goal_anim_state = M_STATE_DUCK_START;
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (Creature_CanTargetEnemy(item, &info)) {
            const int32_t rnd = Random_GetControl();
            if (rnd < M_SHOOT_1_CHANCE) {
                item->goal_anim_state = M_STATE_SHOOT_1;
            } else if (rnd < M_SHOOT_2_CHANCE) {
                item->goal_anim_state = M_STATE_SHOOT_2;
            } else {
                item->goal_anim_state = M_STATE_AIM_3;
            }
        } else if (
            creature->mood == MOOD_BORED
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal
                    || lara_info.distance > M_RUN_DIST))) {
            if (info.ahead) {
                item->goal_anim_state = M_STATE_WAIT;
            } else {
                item->goal_anim_state = M_STATE_WALK;
            }
        } else {
            item->goal_anim_state = M_STATE_RUN;
        }
        break;

    case M_STATE_WALK:
        head = lara_info.angle;
        creature->maximum_turn = M_WALK_TURN;

        if ((item->ai_bits & AI_PATROL_1) != 0) {
            item->goal_anim_state = M_STATE_WALK;
            head = 0;
        } else if (near_cover && (lara->target == item || item->hit_status)) {
            item->required_anim_state = M_STATE_DUCK_START;
            item->goal_anim_state = M_STATE_WAIT;
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (Creature_CanTargetEnemy(item, &info)) {
            if (info.distance > M_WALK_DIST
                && info.zone_num == info.enemy_zone_num) {
                item->goal_anim_state = M_STATE_AIM_4;
            } else {
                item->goal_anim_state = M_STATE_WAIT;
            }
        } else if (creature->mood != MOOD_BORED) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (info.ahead) {
            item->goal_anim_state = M_STATE_WALK;
        } else {
            item->goal_anim_state = M_STATE_WAIT;
        }
        break;

    case M_STATE_RUN:
        if (info.ahead) {
            head = info.angle;
        }
        creature->maximum_turn = M_RUN_TURN;
        tilt = angle / 2;

        if ((item->ai_bits & AI_GUARD) != 0) {
            item->goal_anim_state = M_STATE_WAIT;
        } else if (near_cover && (lara->target == item || item->hit_status)) {
            item->required_anim_state = M_STATE_DUCK_START;
            item->goal_anim_state = M_STATE_WAIT;
        } else if (creature->mood != MOOD_ESCAPE) {
            if (Creature_CanTargetEnemy(item, &info)
                || ((item->ai_bits & AI_FOLLOW) != 0
                    && (creature->reached_goal
                        || lara_info.distance > M_RUN_DIST))) {
                item->goal_anim_state = M_STATE_WAIT;
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = M_STATE_WALK;
            }
        }
        break;

    case M_STATE_AIM_1:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        if (anim_idx == M_ANIM_AIM_1
            || (anim_idx == M_ANIM_SHOOT_1 && frame_idx == 10)) {
            if (!Creature_Shoot(item, &info, &m_Gun, torso_y, M_DAMAGE)) {
                item->required_anim_state = M_STATE_WAIT;
            }
        } else if (
            item->hit_status && (Random_GetControl() & M_DUCK_CHANCE) == 0
            && near_cover) {
            item->required_anim_state = M_STATE_DUCK_START;
            item->goal_anim_state = M_STATE_WAIT;
        }
        break;

    case M_STATE_SHOOT_1:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        if (item->required_anim_state == M_STATE_WAIT) {
            item->goal_anim_state = M_STATE_WAIT;
        }
        break;

    case M_STATE_SHOOT_2:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        if (frame_idx == 0) {
            if (!Creature_Shoot(item, &info, &m_Gun, torso_y, M_DAMAGE)) {
                item->goal_anim_state = M_STATE_WAIT;
            }
        } else if (
            item->hit_status && (Random_GetControl() & M_DUCK_CHANCE) == 0
            && near_cover) {
            item->required_anim_state = M_STATE_DUCK_START;
            item->goal_anim_state = M_STATE_WAIT;
        }
        break;

    case M_STATE_SHOOT_3A:
    case M_STATE_SHOOT_3B:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        if (frame_idx == 0 || frame_idx == 11) {
            if (!Creature_Shoot(item, &info, &m_Gun, torso_y, M_DAMAGE)) {
                item->goal_anim_state = M_STATE_WAIT;
            }
        } else if (
            item->hit_status && (Random_GetControl() & M_DUCK_CHANCE) == 0
            && near_cover) {
            item->required_anim_state = M_STATE_DUCK_START;
            item->goal_anim_state = M_STATE_WAIT;
        }
        break;

    case M_STATE_AIM_4:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        if ((anim_idx == M_ANIM_AIM_4A && frame_idx == 17)
            || (anim_idx == M_ANIM_AIM_4B && frame_idx == 6)) {
            if (!Creature_Shoot(item, &info, &m_Gun, torso_y, M_DAMAGE)) {
                item->required_anim_state = M_STATE_WALK;
            }
        } else if (
            item->hit_status && (Random_GetControl() & M_DUCK_CHANCE) == 0
            && near_cover) {
            item->required_anim_state = M_STATE_DUCK_START;
            item->goal_anim_state = M_STATE_WAIT;
        }

        if (info.distance < M_WALK_DIST) {
            item->required_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_SHOOT_4A:
    case M_STATE_SHOOT_4B:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        if (item->required_anim_state == M_STATE_WALK) {
            item->goal_anim_state = M_STATE_WALK;
        }

        if (frame_idx == 16
            && !Creature_Shoot(item, &info, &m_Gun, torso_y, M_DAMAGE)) {
            item->goal_anim_state = M_STATE_WALK;
        }

        if (info.distance < M_WALK_DIST) {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_DUCKED:
        if (info.ahead) {
            head = info.angle;
        }
        creature->maximum_turn = 0;

        if (Creature_CanTargetEnemy(item, &info)) {
            item->goal_anim_state = M_STATE_DUCK_AIM;
        } else if (
            item->hit_status || !near_cover
            || (info.ahead && (Random_GetControl() & M_DUCK_END_CHANCE) == 0)) {
            item->goal_anim_state = M_STATE_DUCK_END;
        } else {
            item->goal_anim_state = M_STATE_DUCK_WALK;
        }
        break;

    case M_STATE_DUCK_AIM:
        if (info.ahead) {
            torso_y = info.angle;
        }
        creature->maximum_turn = M_DUCK_TURN;

        if (Creature_CanTargetEnemy(item, &info)) {
            item->goal_anim_state = M_STATE_DUCK_SHOOT;
        } else {
            item->goal_anim_state = M_STATE_DUCKED;
        }
        break;

    case M_STATE_DUCK_SHOOT:
        if (info.ahead) {
            torso_y = info.angle;
        }

        if (frame_idx == 0
            && (!Creature_Shoot(item, &info, &m_Gun, torso_y, M_DAMAGE)
                || (Random_GetControl() & 7) == 0)) {
            item->goal_anim_state = M_STATE_DUCKED;
        }
        break;

    case M_STATE_DUCK_WALK:
        if (info.ahead) {
            head = info.angle;
        }
        creature->maximum_turn = M_WALK_TURN;

        if (Creature_CanTargetEnemy(item, &info) || item->hit_status
            || !near_cover
            || (info.ahead && (Random_GetControl() & M_DUCK_END_CHANCE) == 0)) {
            item->goal_anim_state = M_STATE_DUCKED;
        }
        break;
    }

finish:
    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, torso_y);
    Creature_Joint(item, 1, torso_x);
    Creature_Joint(item, 2, head);
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->collision_func = Creature_Collision;
    obj->control_func = M_Control;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->radius = M_RADIUS;
    obj->hit_points = M_HIT_POINTS;

    obj->intelligent = true;
    obj->save_anim = true;
    obj->save_flags = true;
    obj->save_hitpoints = true;
    obj->save_position = true;

    Object_GetBone(obj, 6)->rot.x = true;
    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 13)->rot.y = true;
}

REGISTER_OBJECT(O_MP_2, M_Setup)
