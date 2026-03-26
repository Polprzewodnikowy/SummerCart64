#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/common.h>
#include <trx/game/spawn.h>

// clang-format off
#define BIG_EEL_HITPOINTS  20
#define BIG_EEL_TOUCH_BITS 0b00000001'10000000 // = 0x180
#define BIG_EEL_DAMAGE     500
#define BIG_EEL_ANGLE      (DEG_1 * 10) // = 1820
#define BIG_EEL_RANGE      (WALL_L * 6) // = 6144
#define BIG_EEL_MOVE       (WALL_L / 10) // = 102
#define BIG_EEL_LENGTH     (WALL_L * 5 / 2) // = 2560
#define BIG_EEL_SLIDE      (BIG_EEL_RANGE - BIG_EEL_LENGTH) // = 3584
// clang-format on

typedef enum {
    // clang-format off
    BIG_EEL_STATE_EMPTY  = 0,
    BIG_EEL_STATE_ATTACK = 1,
    BIG_EEL_STATE_STOP   = 2,
    BIG_EEL_STATE_DEATH  = 3,
    // clang-format on
} BIG_EEL_STATE;

typedef enum {
    BIG_EEL_ANIM_DEATH = 2,
} BIG_EEL_ANIM;

typedef struct {
    int32_t pos;
} M_PRIV;

static const BITE m_BigEelBite = {
    .pos = { .x = 7, .y = 157, .z = 333 },
    .mesh_num = 7,
};

static bool M_IsTargetable(const ITEM *const item)
{
    return false;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const ITEM *const lara_item = Lara_GetItem();
    M_PRIV *const p = item->priv;

    int32_t pos = p->pos;
    item->pos.z -= (pos * Math_Cos(item->rot.y)) >> W2V_SHIFT;
    item->pos.x -= ((pos * Math_Sin(item->rot.y)) >> W2V_SHIFT);

    if (item->hit_points <= 0) {
        if (pos < BIG_EEL_SLIDE) {
            pos += BIG_EEL_MOVE;
        }
        if (item->current_anim_state != BIG_EEL_STATE_DEATH) {
            Item_SwitchToAnim(item, BIG_EEL_ANIM_DEATH, 0);
            item->current_anim_state = BIG_EEL_STATE_DEATH;
        }
    } else {
        const int32_t dx = lara_item->pos.x - item->pos.x;
        const int32_t dz = lara_item->pos.z - item->pos.z;
        const int16_t angle = Math_Atan(dz, dx) - item->rot.y;
        const int32_t distance = Math_Sqrt(SQUARE(dx) + SQUARE(dz));

        switch (item->current_anim_state) {
        case BIG_EEL_STATE_STOP:
            if (pos > 0) {
                pos -= BIG_EEL_MOVE;
            }
            if (distance <= BIG_EEL_RANGE && ABS(angle) < BIG_EEL_ANGLE) {
                item->goal_anim_state = BIG_EEL_STATE_ATTACK;
            }
            break;

        case BIG_EEL_STATE_ATTACK:
            if (pos < distance - BIG_EEL_LENGTH) {
                pos += BIG_EEL_MOVE;
            }
            if (item->required_anim_state == BIG_EEL_STATE_EMPTY
                && (item->touch_bits & BIG_EEL_TOUCH_BITS) != 0) {
                Lara_TakeDamage(BIG_EEL_DAMAGE, true);
                Creature_Effect(item, &m_BigEelBite, Spawn_Blood);
                item->required_anim_state = BIG_EEL_STATE_STOP;
            }
            break;
        }
    }

    item->pos.x += (pos * Math_Sin(item->rot.y)) >> W2V_SHIFT;
    item->pos.z += (pos * Math_Cos(item->rot.y)) >> W2V_SHIFT;
    p->pos = pos;

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->is_targetable_func = M_IsTargetable;
    obj->priv_size = sizeof(M_PRIV);

    obj->hit_points = BIG_EEL_HITPOINTS;

    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_BIG_EEL, M_Setup)
