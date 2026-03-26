#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/spawn.h>

// clang-format off
#define EEL_HITPOINTS  5
#define EEL_TOUCH_BITS 0b00000001'10000000 // = 0x180
#define EEL_DAMAGE     50
#define EEL_ANGLE      (DEG_1 * 10) // = 1820
#define EEL_RANGE      (WALL_L * 2) // = 2048
#define EEL_MOVE       (WALL_L / 10) // = 102
#define EEL_TURN       (DEG_1 / 2) // = 91
#define EEL_LENGTH     (WALL_L / 2) // = 512
#define EEL_SLIDE      (EEL_RANGE - EEL_LENGTH) // = 1536
// clang-format on

typedef enum {
    // clang-format off
    EEL_STATE_EMPTY  = 0,
    EEL_STATE_ATTACK = 1,
    EEL_STATE_STOP   = 2,
    EEL_STATE_DEATH  = 3,
    // clang-format on
} EEL_STATE;

typedef enum {
    EEL_ANIM_DEATH = 3,
} EEL_ANIM;

typedef struct {
    int32_t pos;
} M_PRIV;

static const BITE m_EelBite = {
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
    M_PRIV *const p = item->priv;

    int32_t pos = p->pos;
    item->pos.x -= (pos * Math_Sin(item->rot.y)) >> W2V_SHIFT;
    item->pos.z -= (pos * Math_Cos(item->rot.y)) >> W2V_SHIFT;

    if (item->hit_points <= 0) {
        if (pos < EEL_SLIDE) {
            pos += EEL_MOVE;
        }
        if (item->current_anim_state != EEL_STATE_DEATH) {
            Item_SwitchToAnim(item, EEL_ANIM_DEATH, 0);
            item->current_anim_state = EEL_STATE_DEATH;
        }
    } else {
        const ITEM *const lara_item = Lara_GetItem();
        const int32_t dx = lara_item->pos.x - item->pos.x;
        const int32_t dz = lara_item->pos.z - item->pos.z;
        const int16_t quadrant = (item->rot.y + DEG_45) & 0xC000;
        const int16_t angle = Math_Atan(dz, dx);
        const int32_t distance = Math_Sqrt(SQUARE(dx) + SQUARE(dz));

        switch (item->current_anim_state) {
        case EEL_STATE_STOP:
            if (pos > 0) {
                pos -= EEL_MOVE;
            }
            if (distance <= EEL_RANGE && ABS(angle - quadrant) < EEL_ANGLE) {
                item->goal_anim_state = EEL_STATE_ATTACK;
            }
            break;

        case EEL_STATE_ATTACK:
            if (pos < distance - EEL_LENGTH) {
                pos += EEL_MOVE;
            }
            if (angle < item->rot.y - EEL_TURN) {
                item->rot.y -= EEL_TURN;
            } else if (angle > item->rot.y + EEL_TURN) {
                item->rot.y += EEL_TURN;
            }
            if (item->required_anim_state == EEL_STATE_EMPTY
                && (item->touch_bits & EEL_TOUCH_BITS) != 0) {
                Lara_TakeDamage(EEL_DAMAGE, true);
                Creature_Effect(item, &m_EelBite, Spawn_Blood);
                item->required_anim_state = EEL_STATE_STOP;
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

    obj->hit_points = EEL_HITPOINTS;

    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_EEL, M_Setup)
