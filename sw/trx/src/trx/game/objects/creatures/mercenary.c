#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>

// clang-format off
#define M_HITPOINTS   30
#define M_DAMAGE      28
#define M_RADIUS      (WALL_L / 10) // = 102
#define M_WALK_TURN   (DEG_1 * 5) // = 910
#define M_RUN_TURN    (DEG_1 * 10) // = 1820
#define M_RUN_RANGE   SQUARE(WALL_L * 2) // = 4194304
#define M_SHOOT_RANGE SQUARE(WALL_L * 3) // = 9437184
// clang-format on

typedef enum {
    // clang-format off
    M_ANIM_STOP         = 12,
    M_ANIM_WALK_TO_STOP = 17,
    M_ANIM_DEATH        = 19,
    // clang-format on
} M_ANIM;

typedef enum {
    M_STATE_EMPTY,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_RUN,
    M_STATE_WAIT,
    M_STATE_SHOOT_1,
    M_STATE_SHOOT_2,
    M_STATE_DEATH,
    M_STATE_AIM_1,
    M_STATE_AIM_2,
    M_STATE_AIM_3,
    M_STATE_SHOOT_3
} M_STATE;

static const CREATURE_GUN m_MercenaryGun = {
    .muzzle = { .pos = { 0, 300, 64 }, .mesh_num = 7 },
    .tr3_enemy_flash = true,
    .tr3_flash = { .pos = { 0, 300, 56 }, .mesh_num = 7 },
    .tr3_enemy_weapon_flags = 1,
    .tr3_flash_shade = 600,
    .tr3_flash_rot_x = -DEG_90,
};

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    Creature_Initialise(item_num);
    Item_SwitchToAnim(item, M_ANIM_STOP, 0);
    item->current_anim_state = M_STATE_STOP;
    item->goal_anim_state = M_STATE_STOP;
}

static void M_CalculateTarget(ITEM *const item)
{
    CREATURE *const mercenary = item->creature_data;
    const ITEM *const lara_item = Lara_GetItem();
    if (mercenary->hurt_by_lara) {
        mercenary->enemy = (ITEM *)lara_item;
        return;
    }

    int32_t best_distance = INT32_MAX;
    mercenary->enemy = nullptr;

    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        const CREATURE *const creature = LOT_GetBaddieSlot(i);
        if (creature->item_num == NO_ITEM || creature == mercenary) {
            continue;
        }

        const ITEM *const candidate = Item_Get(creature->item_num);
        if (candidate == lara_item || candidate->object_id == item->object_id) {
            continue;
        }

        const XYZ_32 delta = {
            .x = candidate->pos.x - item->pos.x,
            .y = 0,
            .z = candidate->pos.z - item->pos.z,
        };
        const int32_t distance = XYZ_32_GetLength2(delta);
        if (distance < best_distance) {
            mercenary->enemy = (ITEM *)candidate;
            best_distance = distance;
        }
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    int16_t tilt = 0;
    int16_t angle = 0;
    int16_t head = 0;
    int16_t torso_x = 0;
    int16_t torso_y = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
            creature->flags = (Random_GetControl() & 3) == 0 ? 1 : 0;
        }
        goto finish;
    }

    if (item->ai_bits != 0) {
        Creature_GetAITarget(creature);
    } else {
        M_CalculateTarget(item);
    }

    AI_INFO info = {};
    Creature_AIInfo(item, &info);

    int32_t enemy_dist;
    int32_t enemy_angle;
    const ITEM *const lara_item = Lara_GetItem();
    if (creature->enemy == lara_item) {
        enemy_dist = info.distance;
        enemy_angle = info.angle;
    } else {
        const int32_t dx = lara_item->pos.x - item->pos.x;
        const int32_t dz = lara_item->pos.z - item->pos.z;
        enemy_angle = Math_Atan(dz, dx) - item->rot.y;
        enemy_dist = XYZ_32_GetLength2((XYZ_32) { dx, 0, dz });
    }

    Creature_Mood(item, &info, creature->enemy != lara_item);
    angle = Creature_Turn(item, creature->maximum_turn);

    if (item->hit_status) {
        if (!creature->alerted) {
            Sound_Effect(SFX_AMERICAN_HOY, &item->pos, SPM_NORMAL);
        }
        Creature_AlertAllGuards(item_num);
    }

    switch (item->current_anim_state) {
    case M_STATE_STOP:
        creature->flags = 0;
        creature->maximum_turn = 0;
        head = enemy_angle;

        if (Item_TestAnimEqual(item, M_ANIM_WALK_TO_STOP)) {
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
            if ((Random_GetControl() & 0xFF) == 0) {
                if (item->current_anim_state == M_STATE_STOP) {
                    item->goal_anim_state = M_STATE_WAIT;
                } else {
                    item->goal_anim_state = M_STATE_STOP;
                }
            }
        } else if ((item->ai_bits & AI_PATROL_1) != 0) {
            item->goal_anim_state = M_STATE_WALK;
            head = 0;
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (Creature_CanTargetEnemy(item, &info)) {
            if (info.distance >= M_SHOOT_RANGE
                && info.zone_num == info.enemy_zone_num) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (Random_GetControl() < 0x4000) {
                item->goal_anim_state = M_STATE_AIM_1;
            } else {
                item->goal_anim_state = M_STATE_AIM_3;
            }
        } else if (
            (!creature->alerted && creature->mood == MOOD_BORED)
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal || enemy_dist > M_RUN_RANGE))) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (
            creature->mood != MOOD_BORED && info.distance > M_RUN_RANGE) {
            item->goal_anim_state = M_STATE_RUN;
        } else {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_WALK:
        head = enemy_angle;
        creature->flags = 0;
        creature->maximum_turn = M_WALK_TURN;

        if ((item->ai_bits & AI_PATROL_1) != 0) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (
            (item->ai_bits & AI_GUARD) != 0
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal || enemy_dist > M_RUN_RANGE))) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (Creature_CanTargetEnemy(item, &info)) {
            if (info.distance < M_SHOOT_RANGE
                || info.zone_num != info.enemy_zone_num) {
                item->goal_anim_state = M_STATE_STOP;
            } else {
                item->goal_anim_state = M_STATE_AIM_2;
            }
        } else if (creature->mood == MOOD_BORED) {
            if (info.ahead) {
                item->goal_anim_state = M_STATE_STOP;
            }
        } else if (info.distance > M_RUN_RANGE) {
            item->goal_anim_state = M_STATE_RUN;
        }
        break;

    case M_STATE_RUN:
        if (info.ahead) {
            head = info.angle;
        }
        creature->maximum_turn = M_RUN_TURN;
        tilt = angle / 2;

        if ((item->ai_bits & AI_GUARD) != 0
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal || enemy_dist > M_RUN_RANGE))) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (creature->mood == MOOD_ESCAPE) {
            break;
        } else if (
            Creature_CanTargetEnemy(item, &info)
            || creature->mood == MOOD_BORED) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (
            creature->mood == MOOD_STALK && (item->ai_bits & AI_FOLLOW) == 0
            && info.distance < M_RUN_RANGE) {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_WAIT:
        head = enemy_angle;
        creature->flags = 0;
        creature->maximum_turn = 0;

        if ((item->ai_bits & AI_GUARD) != 0) {
            head = Creature_AIGuard(creature);
            if ((Random_GetControl() & 0xFF) == 0) {
                item->goal_anim_state = M_STATE_STOP;
            }
        } else if (Creature_CanTargetEnemy(item, &info)) {
            item->goal_anim_state = M_STATE_SHOOT_1;
        } else if (creature->mood != MOOD_BORED || !info.ahead) {
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_SHOOT_1:
    case M_STATE_SHOOT_2:
    case M_STATE_SHOOT_3:
        if (item->current_anim_state == M_STATE_SHOOT_3
            && item->goal_anim_state != M_STATE_STOP
            && (creature->mood == MOOD_ESCAPE || info.distance > M_SHOOT_RANGE
                || !Creature_CanTargetEnemy(item, &info))) {
            item->goal_anim_state = M_STATE_STOP;
        }

        if (info.ahead) {
            torso_x = info.x_angle;
            torso_y = info.angle;
        }

        if (creature->flags != 0) {
            creature->flags--;
        } else if (creature->enemy != nullptr) {
            Creature_Shoot(item, &info, &m_MercenaryGun, torso_y, M_DAMAGE);
            creature->flags = 5;
        }
        break;

    case M_STATE_AIM_1:
    case M_STATE_AIM_2:
    case M_STATE_AIM_3:
        creature->flags = 0;
        if (!info.ahead) {
            break;
        }

        torso_x = info.x_angle;
        torso_y = info.angle;
        if (Creature_CanTargetEnemy(item, &info)) {
            if (item->current_anim_state == M_STATE_AIM_1) {
                item->goal_anim_state = M_STATE_SHOOT_1;
            } else if (item->current_anim_state == M_STATE_AIM_2) {
                item->goal_anim_state = M_STATE_SHOOT_2;
            } else {
                item->goal_anim_state = M_STATE_SHOOT_3;
            }
        } else {
            item->goal_anim_state = item->current_anim_state == M_STATE_AIM_2
                ? M_STATE_WALK
                : M_STATE_STOP;
        }
        break;

    default:
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

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = M_HITPOINTS;
    obj->radius = M_RADIUS;
    obj->pivot_length = 0;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.x = true;
    Object_GetBone(obj, 0)->rot.y = true;
    Object_GetBone(obj, 7)->rot.y = true;
}

REGISTER_OBJECT(O_STHPAC_MERCENARY, M_Setup)
