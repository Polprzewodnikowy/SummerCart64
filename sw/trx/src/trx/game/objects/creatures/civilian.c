#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_RADIUS         (WALL_L / 10)          // = 102
#define M_HIT_POINTS     15
#define M_BOX_DAMAGE     20
#define M_PUNCH_1_DAMAGE 40
#define M_PUNCH_3_DAMAGE 50
#define M_TOUCH_BITS     0b00100100'00000000
#define M_ALERT_DIST     SQUARE(WALL_L)         // = 1048576
#define M_ESCAPE_DIST    SQUARE(WALL_L * 3)     // = 9437184
#define M_RUN_DIST       SQUARE(WALL_L * 2)     // = 4194304
#define M_WALK_DIST      SQUARE(WALL_L)         // = 1048576
#define M_ATTACK_DIST_1  SQUARE(WALL_L / 3)     // = 116281
#define M_ATTACK_DIST_2  SQUARE(WALL_L * 2 / 3) // = 465124
#define M_ATTACK_DIST_3  SQUARE(WALL_L)         // = 1048576
#define M_WALK_TURN      (DEG_1 * 5)            // = 910
#define M_RUN_TURN       (DEG_1 * 6)            // = 1092
// clang-format on

typedef enum {
    M_STATE_NULL,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_PUNCH_3,
    M_STATE_AIM_3,
    M_STATE_WAIT,
    M_STATE_AIM_2,
    M_STATE_AIM_1,
    M_STATE_PUNCH_2,
    M_STATE_PUNCH_1,
    M_STATE_RUN,
    M_STATE_DEATH,
    M_STATE_UP_4,
    M_STATE_UP_2,
    M_STATE_UP_3,
    M_STATE_DOWN_4,
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_STOP   = 6,
    M_ANIM_DEATH  = 26,
    M_ANIM_UP_4   = 27,
    M_ANIM_UP_2   = 28,
    M_ANIM_UP_3   = 29,
    M_ANIM_DOWN_4 = 30,
    // clang-format on
} M_ANIM;

static const BITE m_Bite = {
    .pos = { 0, 0, 0 },
    .mesh_num = 13,
};

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    Item_SwitchToAnim(item, M_ANIM_STOP, 0);
    item->current_anim_state = M_STATE_STOP;
    item->goal_anim_state = M_STATE_STOP;
}

static bool M_Vault(ITEM *const item, const int16_t angle)
{
    const int32_t vault_result =
        Creature_Vault(Item_GetIndex(item), angle, 2, 260);
    switch (vault_result) {
    case -4:
        Item_SwitchToAnim(item, M_ANIM_DOWN_4, 0);
        item->current_anim_state = M_STATE_DOWN_4;
        return true;
    case 2:
        Item_SwitchToAnim(item, M_ANIM_UP_2, 0);
        item->current_anim_state = M_STATE_UP_2;
        return true;
    case 3:
        Item_SwitchToAnim(item, M_ANIM_UP_3, 0);
        item->current_anim_state = M_STATE_UP_3;
        return true;
    case 4:
        Item_SwitchToAnim(item, M_ANIM_UP_4, 0);
        item->current_anim_state = M_STATE_UP_4;
        return true;
    default:
        return false;
    }
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
            creature->lot.setup.step = STEP_L;
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

    Creature_UpdateMood(item, &info, true);
    if (creature->enemy == lara_item && info.distance > M_ESCAPE_DIST
        && ABS(info.enemy_facing) < 0x3000) {
        creature->mood = MOOD_ESCAPE;
    }
    Creature_ApplyMood(item, &info, true);
    angle = Creature_Turn(item, creature->maximum_turn);

    ITEM *const enemy = creature->enemy;
    creature->enemy = lara_item;
    if ((item->ai_bits & AI_FOLLOW) == 0
        && (item->hit_status || lara_info.distance < M_ALERT_DIST
            || Creature_CanSeeEnemy(item, &lara_info))) {
        if (!creature->alerted) {
            Sound_Effect(SFX_AMERICAN_HOY, &item->pos, SPM_NORMAL);
        }
        Creature_AlertAllGuards(item_num);
    }
    creature->enemy = enemy;

    switch (item->current_anim_state) {
    case M_STATE_STOP:
    case M_STATE_WAIT:
        if (item->current_anim_state == M_STATE_WAIT
            && (creature->alerted || item->goal_anim_state == M_STATE_RUN)) {
            item->goal_anim_state = M_STATE_STOP;
            break;
        }

        creature->flags = 0;
        creature->maximum_turn = 0;
        head = lara_info.angle;

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
        } else if (creature->mood == MOOD_ESCAPE) {
            if (lara->target != item && info.ahead) {
                item->goal_anim_state = M_STATE_STOP;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
        } else if (
            creature->mood == MOOD_BORED
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal
                    || lara_info.distance > M_RUN_DIST))) {
            if (item->required_anim_state != M_STATE_NULL) {
                item->goal_anim_state = item->required_anim_state;
            } else if (info.ahead) {
                item->goal_anim_state = M_STATE_STOP;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
        } else if (info.bite && info.distance < M_ATTACK_DIST_1) {
            item->goal_anim_state = M_STATE_AIM_1;
        } else if (info.bite && info.distance < M_ATTACK_DIST_2) {
            item->goal_anim_state = M_STATE_AIM_2;
        } else if (info.bite && info.distance < M_WALK_DIST) {
            item->goal_anim_state = M_STATE_WALK;
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
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (creature->mood == MOOD_BORED) {
            if (Random_GetControl() < 256) {
                item->required_anim_state = M_STATE_WAIT;
                item->goal_anim_state = M_STATE_STOP;
            }
        } else if (info.bite && info.distance < M_ATTACK_DIST_1) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (info.bite && info.distance < M_ATTACK_DIST_3) {
            item->goal_anim_state = M_STATE_AIM_3;
        } else {
            item->goal_anim_state = M_STATE_RUN;
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
        } else if (creature->mood == MOOD_ESCAPE) {
            if (lara->target != item && info.ahead) {
                item->goal_anim_state = M_STATE_STOP;
            }
        } else if (
            (item->ai_bits & AI_FOLLOW) != 0
            && (creature->reached_goal || lara_info.distance > M_RUN_DIST)) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (creature->mood == MOOD_BORED) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (info.ahead && info.distance < M_WALK_DIST) {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_AIM_1:
        if (info.ahead) {
            torso_x = info.x_angle;
            torso_y = info.angle;
        }
        creature->maximum_turn = M_WALK_TURN;

        creature->flags = 0;
        if (info.bite && info.distance < M_ATTACK_DIST_1) {
            item->goal_anim_state = M_STATE_PUNCH_1;
        } else {
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_AIM_2:
        if (info.ahead) {
            torso_x = info.x_angle;
            torso_y = info.angle;
        }
        creature->maximum_turn = M_WALK_TURN;

        creature->flags = 0;
        if (info.ahead && info.distance < M_ATTACK_DIST_2) {
            item->goal_anim_state = M_STATE_PUNCH_2;
        } else {
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_AIM_3:
        if (info.ahead) {
            torso_x = info.x_angle;
            torso_y = info.angle;
        }
        creature->maximum_turn = M_WALK_TURN;

        creature->flags = 0;
        if (info.bite && info.distance < M_ATTACK_DIST_3) {
            item->goal_anim_state = M_STATE_PUNCH_3;
        } else {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_PUNCH_1:
        if (info.ahead) {
            torso_x = info.x_angle;
            torso_y = info.angle;
        }
        creature->maximum_turn = M_WALK_TURN;

        if (creature->flags == 0 && (item->touch_bits & M_TOUCH_BITS) != 0) {
            Lara_TakeDamage(M_PUNCH_1_DAMAGE, true);
            Creature_Effect(item, &m_Bite, Spawn_Blood);
            Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);
            creature->flags = 1;
        }
        break;

    case M_STATE_PUNCH_2:
        if (info.ahead) {
            torso_x = info.x_angle;
            torso_y = info.angle;
        }
        creature->maximum_turn = M_WALK_TURN;

        if (creature->flags == 0 && (item->touch_bits & M_TOUCH_BITS) != 0) {
            Lara_TakeDamage(M_PUNCH_1_DAMAGE, true);
            Creature_Effect(item, &m_Bite, Spawn_Blood);
            Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);
            creature->flags = 1;
        }

        if (info.ahead && info.distance > M_ATTACK_DIST_2
            && info.distance < M_ATTACK_DIST_3) {
            item->goal_anim_state = M_STATE_PUNCH_3;
        }
        break;

    case M_STATE_PUNCH_3:
        if (info.ahead) {
            torso_x = info.x_angle;
            torso_y = info.angle;
        }
        creature->maximum_turn = M_WALK_TURN;

        if (creature->flags != 2 && (item->touch_bits & M_TOUCH_BITS) != 0) {
            Lara_TakeDamage(M_PUNCH_3_DAMAGE, true);
            Creature_Effect(item, &m_Bite, Spawn_Blood);
            Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);
            creature->flags = 2;
        }
        break;
    }

finish:
    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, torso_y);
    Creature_Joint(item, 1, torso_x);
    Creature_Joint(item, 2, head);

    if (item->current_anim_state >= M_STATE_DEATH) {
        creature->maximum_turn = 0;
        Creature_Animate(item_num, angle, 0);
    } else if (M_Vault(item, angle)) {
        creature->maximum_turn = 0;
    }
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
    obj->lot_setup = LOT_Setup(LOT_SETUP_CLIMBER);

    obj->intelligent = true;
    obj->save_anim = true;
    obj->save_flags = true;
    obj->save_hitpoints = true;
    obj->save_position = true;

    Object_GetBone(obj, 6)->rot.x = true;
    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 13)->rot.y = true;
}

REGISTER_OBJECT(O_CIVILIAN, M_Setup)
