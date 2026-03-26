#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>

// clang-format off
#define M_PART_DAMAGE        250
#define M_ATTACK_DAMAGE      500
#define M_TOUCH_DAMAGE       5
#define M_NEED_TURN          (DEG_1 * 45) // = 8190
#define M_TURN               (DEG_1 * 3) // = 546
#define M_ATTACK_RANGE       SQUARE(2600) // = 6760000
#define M_CLOSE_RANGE        SQUARE(2250) // = 5062500
#define M_TOUCH_LEFT         0x7FF0
#define M_TOUCH_RIGHT        0x3FF8000
#define M_TOUCH              (M_TOUCH_LEFT | M_TOUCH_RIGHT)
#define M_HITPOINTS          500
#define M_RADIUS             (WALL_L / 3) // = 341
#define M_SMARTNESS          0x7FFF
#define M_FRAME_TURN_L_START 14
#define M_FRAME_TURN_L_END   22
#define M_FRAME_TURN_R_START 17
#define M_FRAME_TURN_R_END   22
// clang-format on

typedef enum {
    // clang-format off
    TORSO_ANIM_TURN_L = 8,
    TORSO_ANIM_DIE    = 13,
    TORSO_ANIM_TURN_R = 17,
    TORSO_ANIM_KILL   = 19,
    // clang-format on
} M_ANIM;

typedef enum {
    // clang-format off
    TORSO_STATE_EMPTY    = 0,
    TORSO_STATE_STOP     = 1,
    TORSO_STATE_TURN_L   = 2,
    TORSO_STATE_TURN_R   = 3,
    TORSO_STATE_ATTACK_1 = 4,
    TORSO_STATE_ATTACK_2 = 5,
    TORSO_STATE_ATTACK_3 = 6,
    TORSO_STATE_FORWARD  = 7,
    TORSO_STATE_SET      = 8,
    TORSO_STATE_FALL     = 9,
    TORSO_STATE_DEATH    = 10,
    TORSO_STATE_KILL     = 11,
    // clang-format on
} M_STATE;

static void M_KillLara(ITEM *const item)
{
    const ITEM *const lara_item = Lara_GetItem();
    Lara_TakeDamage(lara_item->hit_points, true);
    Creature_SpecialKill(
        item, TORSO_ANIM_KILL, TORSO_STATE_KILL, LS_EXTRA_TORSO_KILL);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (item->status == IS_INVISIBLE) {
        if (!LOT_EnableBaddieAI(item_num, 0)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    CREATURE *const torso = item->creature_data;
    int16_t head = 0;
    ITEM *const lara_item = Lara_GetItem();

    if (item->hit_points <= 0) {
        if (item->current_anim_state != TORSO_STATE_DEATH) {
            item->current_anim_state = TORSO_STATE_DEATH;
            Item_SwitchToAnim(item, TORSO_ANIM_DIE, 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, true);

        int16_t angle =
            Math_Atan(
                torso->target.z - item->pos.z, torso->target.x - item->pos.x)
            - item->rot.y;

        if (item->touch_bits) {
            Lara_TakeDamage(M_TOUCH_DAMAGE, true);
        }

        switch (item->current_anim_state) {
        case TORSO_STATE_SET:
            item->goal_anim_state = TORSO_STATE_FALL;
            item->gravity = true;
            break;

        case TORSO_STATE_STOP:
            if (lara_item->hit_points <= 0) {
                break;
            }

            torso->flags = 0;
            if (angle > M_NEED_TURN) {
                item->goal_anim_state = TORSO_STATE_TURN_R;
            } else if (angle < -M_NEED_TURN) {
                item->goal_anim_state = TORSO_STATE_TURN_L;
            } else if (info.distance >= M_ATTACK_RANGE) {
                item->goal_anim_state = TORSO_STATE_FORWARD;
            } else if (lara_item->hit_points > M_ATTACK_DAMAGE) {
                if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = TORSO_STATE_ATTACK_1;
                } else {
                    item->goal_anim_state = TORSO_STATE_ATTACK_2;
                }
            } else if (info.distance < M_CLOSE_RANGE) {
                item->goal_anim_state = TORSO_STATE_ATTACK_3;
            } else {
                item->goal_anim_state = TORSO_STATE_FORWARD;
            }
            break;

        case TORSO_STATE_FORWARD:
            if (angle < -M_TURN) {
                item->goal_anim_state -= M_TURN;
            } else if (angle > M_TURN) {
                item->goal_anim_state += M_TURN;
            } else {
                item->goal_anim_state += angle;
            }

            if (angle > M_NEED_TURN || angle < -M_NEED_TURN) {
                item->goal_anim_state = TORSO_STATE_STOP;
            } else if (info.distance < M_ATTACK_RANGE) {
                item->goal_anim_state = TORSO_STATE_STOP;
            }
            break;

        case TORSO_STATE_TURN_L:
            if (!torso->flags) {
                torso->flags = item->frame_num;
            } else if (
                Item_TestAnimEqual(item, TORSO_ANIM_TURN_L)
                && Item_TestFrameRange(
                    item, M_FRAME_TURN_L_START, M_FRAME_TURN_L_END)) {
                item->rot.y -= DEG_1 * 9;
            }

            if (angle > -M_NEED_TURN) {
                item->goal_anim_state = TORSO_STATE_STOP;
            }
            break;

        case TORSO_STATE_TURN_R:
            if (!torso->flags) {
                torso->flags = item->frame_num;
            } else if (
                Item_TestAnimEqual(item, TORSO_ANIM_TURN_R)
                && Item_TestFrameRange(
                    item, M_FRAME_TURN_R_START, M_FRAME_TURN_R_END)) {
                item->rot.y += DEG_1 * 14;
            }

            if (angle < M_NEED_TURN) {
                item->goal_anim_state = TORSO_STATE_STOP;
            }
            break;

        case TORSO_STATE_ATTACK_1:
            if (!torso->flags && (item->touch_bits & M_TOUCH_RIGHT)) {
                Lara_TakeDamage(M_ATTACK_DAMAGE, true);
                torso->flags = 1;
            }
            break;

        case TORSO_STATE_ATTACK_2:
            if (!torso->flags && (item->touch_bits & M_TOUCH)) {
                Lara_TakeDamage(M_ATTACK_DAMAGE, true);
                torso->flags = 1;
            }
            break;

        case TORSO_STATE_ATTACK_3:
            if ((item->touch_bits & M_TOUCH_RIGHT)
                || lara_item->hit_points <= 0) {
                M_KillLara(item);
            }
            break;

        case TORSO_STATE_KILL:
            g_Camera.target_distance = WALL_L * 2;
            g_Camera.flags = CF_FOLLOW_CENTRE;
            break;
        }
    }

    Creature_Head(item, head);

    if (item->current_anim_state == TORSO_STATE_FALL) {
        Item_Animate(item);

        if (item->pos.y > item->floor) {
            item->goal_anim_state = TORSO_STATE_STOP;
            item->gravity = false;
            item->pos.y = item->floor;
            g_Camera.bounce = 500;
        }
    } else {
        Creature_Animate(item_num, 0, 0);
    }

    if (item->status == IS_DEACTIVATED) {
        Sound_Effect(SFX_ATLANTEAN_DEATH, &item->pos, SPM_NORMAL);
        Item_Explode(item_num, -1, M_PART_DAMAGE);
        Room_TestTriggers(item);

        Item_Kill(item_num);
        item->status = IS_DEACTIVATED;
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = Creature_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 3;
    obj->hit_points = M_HITPOINTS;
    obj->radius = M_RADIUS;
    obj->smartness = M_SMARTNESS;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 1)->rot.y = true;
}

REGISTER_OBJECT(O_TORSO, M_Setup)
