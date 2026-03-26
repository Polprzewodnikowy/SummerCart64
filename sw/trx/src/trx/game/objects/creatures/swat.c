#include <trx/game/creature.h>
#include <trx/game/fx/laser.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_RADIUS       (WALL_L / 10)      // = 102
#define M_HIT_POINTS   45
#define M_BOX_DAMAGE   20
#define M_DAMAGE       28
#define M_ALERT_DIST   SQUARE(WALL_L)     // = 1048576
#define M_ALERT_HEIGHT (WALL_L * 2)       // = 2048
#define M_RUN_DIST     SQUARE(WALL_L * 2) // = 4194304
#define M_SHOOT_DIST   SQUARE(WALL_L * 3) // = 9437184
#define M_WALK_TURN    (DEG_1 * 6)        // = 1092
#define M_RUN_TURN     (DEG_1 * 9)        // = 1638
// clang-format on

typedef enum {
    M_STATE_NULL,
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
    M_STATE_SHOOT_3,
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_STAND    = 12,
    M_ANIM_WALK_END = 17,
    M_ANIM_DEATH    = 19,
    // clang-format on
} M_ANIM;

static const CREATURE_GUN m_SwatGun = {
    .muzzle = { .pos = { 0, 300, 64 }, .mesh_num = 7 },
    .tr3_enemy_flash = true,
    .tr3_flash = { .pos = { 0, 300, 56 }, .mesh_num = 7 },
    .tr3_enemy_weapon_flags = 1,
    .tr3_flash_shade = 600,
    .tr3_flash_rot_x = -DEG_90,
    .tr3_laser = {
        .bite = {
            .pos = { 0, 200, 106 },
            .mesh_num = 7,
        },
        .color = { 0xFF, 0x02, 0x03, 0xDE },
        .width = 2.0f,
    },
};

static void M_Initialise(const int16_t item_num)
{
    Creature_Initialise(item_num);
    ITEM *const item = Item_Get(item_num);
    Item_SwitchToAnim(item, M_ANIM_STAND, 0);
    item->goal_anim_state = M_STATE_STOP;
    item->current_anim_state = M_STATE_STOP;
}

static void M_TriggerLaser(const ITEM *const item)
{
    if (item->hit_points > 0 || !Item_TestFrameEqual(item, -1)) {
        FX_Laser_Spawn(item, &m_SwatGun);
    }
}

static void M_FireFinalShot(
    ITEM *const item, int16_t *const head, int16_t *const torso_y)
{
    const int16_t frame_idx = Item_GetRelativeFrame(item);
    if (frame_idx <= 44 || frame_idx >= 52 || (item->frame_num & 3) != 0) {
        return;
    }

    AI_INFO info = {};
    Creature_AIInfo(item, &info);

    if (!Creature_CanTargetEnemy(item, &info) || ABS(info.angle) >= DEG_45) {
        return;
    }

    *head = info.angle;
    *torso_y = info.angle;
    Creature_Shoot(item, &info, &m_SwatGun, info.angle, M_DAMAGE * 3);
    const SAMPLE_TRX_ID fire_sfx = item->object_id == O_SWAT_3
        ? SFX_AMERICAN_SWAT_FIRE
        : SFX_LONDON_SWAT_FIRE;
    Sound_Effect(fire_sfx, &item->pos, SPM_NORMAL);
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
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
            creature->flags = (Random_GetControl() & 1) == 0 ? 1 : 0;
        } else if (creature->flags != 0) {
            M_FireFinalShot(item, &head, &torso_y);
        }
        goto finish;
    }

    ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->ai_bits != 0) {
        Creature_GetAITarget(creature);
    } else {
        creature->enemy = lara_item;
    }

    AI_INFO info = {};
    Creature_AIInfo(item, &info);

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

    ITEM *const enemy = creature->enemy;
    creature->enemy = lara_item;
    if (item->hit_status
        || ((lara_info.distance < M_ALERT_DIST
             || Creature_CanSeeEnemy(item, &lara_info))
            && ABS(lara_item->pos.y - item->pos.y) < M_ALERT_HEIGHT)) {
        if (!creature->alerted) {
            const SAMPLE_TRX_ID alert_sfx = item->object_id == O_SWAT_3
                ? SFX_AMERICAN_HOY
                : SFX_ENGLISH_HOY;
            Sound_Effect(alert_sfx, &item->pos, SPM_NORMAL);
        }
        Creature_AlertAllGuards(item_num);
    }
    creature->enemy = enemy;

    switch (item->current_anim_state) {
    case M_STATE_STOP:
        head = lara_info.angle;
        creature->flags = 0;
        creature->maximum_turn = 0;

        if (Item_GetRelativeAnim(item) == M_ANIM_WALK_END) {
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
                item->goal_anim_state = M_STATE_WAIT;
            }
        } else if ((item->ai_bits & AI_PATROL_1) != 0) {
            head = 0;
            item->goal_anim_state = M_STATE_WALK;
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (Creature_CanTargetEnemy(item, &info)) {
            if (info.distance >= M_SHOOT_DIST
                && info.zone_num == info.enemy_zone_num) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (Random_GetControl() < 0x4000) {
                item->goal_anim_state = M_STATE_AIM_1;
            } else {
                item->goal_anim_state = M_STATE_AIM_3;
            }
        } else if (
            creature->mood == MOOD_BORED
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal
                    || lara_info.distance > M_RUN_DIST))) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (info.distance > M_RUN_DIST) {
            item->goal_anim_state = M_STATE_RUN;
        } else {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_WAIT:
        head = lara_info.angle;
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

    case M_STATE_WALK:
        head = lara_info.angle;
        creature->flags = 0;
        creature->maximum_turn = M_WALK_TURN;

        if ((item->ai_bits & AI_PATROL_1) != 0) {
            head = 0;
            item->goal_anim_state = M_STATE_WALK;
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (
            (item->ai_bits & AI_GUARD) != 0
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal
                    || lara_info.distance > M_RUN_DIST))) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (Creature_CanTargetEnemy(item, &info)) {
            if (info.distance < M_SHOOT_DIST
                || info.zone_num != info.enemy_zone_num) {
                item->goal_anim_state = M_STATE_STOP;
            } else {
                item->goal_anim_state = M_STATE_AIM_2;
            }
        } else if (creature->mood == MOOD_BORED) {
            if (info.ahead) {
                item->goal_anim_state = M_STATE_STOP;
            }
        } else if (info.distance > M_RUN_DIST) {
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
                && (creature->reached_goal
                    || lara_info.distance > M_RUN_DIST))) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (creature->mood != MOOD_ESCAPE) {
            if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (
                creature->mood == MOOD_BORED
                || (creature->mood == MOOD_STALK
                    && (item->ai_bits & AI_FOLLOW) == 0
                    && info.distance < M_RUN_DIST)) {
                item->goal_anim_state = M_STATE_WALK;
            }
        }
        break;

    case M_STATE_AIM_1:
    case M_STATE_AIM_2:
    case M_STATE_AIM_3:
        creature->flags = 0;
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;

            if (!Creature_CanTargetEnemy(item, &info)) {
                if (item->current_anim_state == M_STATE_AIM_2) {
                    item->goal_anim_state = M_STATE_WALK;
                } else {
                    item->goal_anim_state = M_STATE_STOP;
                }
            } else if (item->current_anim_state == M_STATE_AIM_1) {
                item->goal_anim_state = M_STATE_SHOOT_1;
            } else if (item->current_anim_state == M_STATE_AIM_2) {
                item->goal_anim_state = M_STATE_SHOOT_2;
            } else {
                item->goal_anim_state = M_STATE_SHOOT_3;
            }
        }
        break;

    case M_STATE_SHOOT_1:
    case M_STATE_SHOOT_2:
    case M_STATE_SHOOT_3:
        if (item->current_anim_state == M_STATE_SHOOT_3
            && item->goal_anim_state != M_STATE_STOP
            && (creature->mood == MOOD_ESCAPE || info.distance > M_SHOOT_DIST
                || !Creature_CanTargetEnemy(item, &info))) {
            item->goal_anim_state = M_STATE_STOP;
        }

        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        if (creature->flags == 0) {
            Creature_Shoot(item, &info, &m_SwatGun, torso_y, M_DAMAGE);
            creature->flags = 5;
        } else {
            creature->flags--;
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

    M_TriggerLaser(item);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
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

    Object_GetBone(obj, 0)->rot.x = true;
    Object_GetBone(obj, 0)->rot.y = true;
    Object_GetBone(obj, 7)->rot.y = true;
}

REGISTER_OBJECT(O_SWAT_1, M_Setup)
REGISTER_OBJECT(O_SWAT_2, M_Setup)
REGISTER_OBJECT(O_SWAT_3, M_Setup)
