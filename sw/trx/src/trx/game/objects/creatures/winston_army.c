#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_RADIUS     (WALL_L / 10) // = 102
#define M_STOP_RANGE SQUARE(WALL_L * 3 / 2) // = 2359296
// clang-format on

typedef enum {
    // clang-format off
    M_STATE_EMPTY     = 0,
    M_STATE_STOP      = 1,
    M_STATE_WALK      = 2,
    M_STATE_DEF_1     = 3,
    M_STATE_DEF_2     = 4,
    M_STATE_DEF_3     = 5,
    M_STATE_HIT_1     = 6,
    M_STATE_HIT_2     = 7,
    M_STATE_HIT_3     = 8,
    M_STATE_HIT_DOWN  = 9,
    M_STATE_FALL_DOWN = 10,
    M_STATE_GET_UP    = 11,
    M_STATE_BRUSH_OFF = 12,
    M_STATE_ON_FLOOR  = 13,
    // clang-format on
} M_STATE;

typedef struct {
    int16_t knockdown_timer;
    bool spawn_checked;
} M_PRIV;

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    return false;
}

static bool M_CanBeExploded(const ITEM *const item)
{
    return false;
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "knockdown_timer", &p->knockdown_timer));
    JSON_SHOULD(JSON_READ(io, "spawn_checked", &p->spawn_checked));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "knockdown_timer", p->knockdown_timer);
    JSONW_WRITE(io, "spawn_checked", p->spawn_checked);
}

static bool M_RemoveNormalWinston(void)
{
    const int32_t item_count = Item_GetTotalCount();
    for (int32_t item_num = 0; item_num < item_count; item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (item->object_id != O_WINSTON || (item->flags & IF_KILLED) != 0) {
            continue;
        }
        item->status = IS_INVISIBLE;
        Item_Kill(item_num);
        return true;
    }
    return false;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    M_PRIV *const p = item->priv;

    AI_INFO info;
    Creature_AIInfo(item, &info);
    Creature_Mood(item, &info, true);

    const int16_t angle = Creature_Turn(item, creature->maximum_turn);

    if (!p->spawn_checked) {
        M_RemoveNormalWinston();
        p->spawn_checked = true;
    }

    if (item->hit_points <= 0) {
        creature->maximum_turn = 0;

        switch (item->current_anim_state) {
        case M_STATE_HIT_DOWN:
        case M_STATE_FALL_DOWN:
            if (item->hit_status) {
                item->goal_anim_state = M_STATE_HIT_DOWN;
            } else {
                p->knockdown_timer--;
                if (p->knockdown_timer < 0) {
                    item->goal_anim_state = M_STATE_ON_FLOOR;
                }
            }

            break;

        case M_STATE_GET_UP:
            item->hit_points = 16;
            if (Random_GetControl() & 1) {
                creature->flags = 999;
            }
            break;

        case M_STATE_ON_FLOOR:
            if (item->hit_status) {
                item->goal_anim_state = M_STATE_HIT_DOWN;
            } else {
                p->knockdown_timer--;
                if (p->knockdown_timer < 0) {
                    item->goal_anim_state = M_STATE_GET_UP;
                }
            }

            break;

        default:
            Item_SwitchToObjAnim(item, 16, 0, O_WINSTON_ARMY);
            item->current_anim_state = M_STATE_FALL_DOWN;
            item->goal_anim_state = M_STATE_FALL_DOWN;
            p->knockdown_timer = 150;
            break;
        }
    } else {
        switch (item->current_anim_state) {
        case M_STATE_STOP:
            creature->maximum_turn = DEG_1 * 2;

            if (creature->flags == 999) {
                item->goal_anim_state = M_STATE_BRUSH_OFF;
            } else if (lara->target == item) {
                item->goal_anim_state = M_STATE_DEF_1;
            } else if (
                (info.distance > M_STOP_RANGE || !info.ahead)
                && item->goal_anim_state != M_STATE_WALK) {
                item->goal_anim_state = M_STATE_WALK;
                Sound_Effect(SFX_WINSTON_GRUNT_2, &item->pos, SPM_NORMAL);
            }
            break;

        case M_STATE_WALK:
            creature->maximum_turn = DEG_1 * 2;
            if (lara->target == item) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (info.distance < M_STOP_RANGE) {
                if (info.ahead) {
                    item->goal_anim_state = M_STATE_STOP;
                    if (creature->flags & 1) {
                        creature->flags--;
                    }
                } else if ((creature->flags & 1) == 0) {
                    Sound_Effect(SFX_WINSTON_GRUNT_1, &item->pos, SPM_NORMAL);
                    Sound_Effect(SFX_WINSTON_CUPS, &item->pos, SPM_NORMAL);
                    creature->flags |= 1;
                }
            }

            break;

        case M_STATE_DEF_1:
            creature->maximum_turn = DEG_1 * 2;
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            }
            if (item->hit_status) {
                item->goal_anim_state = M_STATE_HIT_1;
            } else if (lara->target != item) {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_DEF_2:
            creature->maximum_turn = DEG_1 * 2;
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            }
            if (item->hit_status) {
                item->goal_anim_state = M_STATE_HIT_2;
            } else if (lara->target != item) {
                item->goal_anim_state = M_STATE_DEF_1;
            }
            break;

        case M_STATE_DEF_3:
            creature->maximum_turn = DEG_1 * 2;
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            }
            if (item->hit_status) {
                item->goal_anim_state = M_STATE_HIT_3;
            } else if (lara->target != item) {
                item->goal_anim_state = M_STATE_DEF_1;
            }
            break;

        case M_STATE_HIT_1:
            if (Random_GetControl() & 1) {
                item->required_anim_state = M_STATE_DEF_3;
            } else {
                item->required_anim_state = M_STATE_DEF_2;
            }
            break;

        case M_STATE_HIT_2:
        case M_STATE_HIT_3:
            item->required_anim_state = M_STATE_DEF_1;
            break;

        case M_STATE_BRUSH_OFF:
            creature->maximum_turn = 0;
            creature->flags = 0;
            break;
        }
    }

    if (Random_GetControl() < 0x100) {
        Sound_Effect(SFX_WINSTON_CUPS, &item->pos, SPM_NORMAL);
    }

    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;
    obj->can_be_exploded_func = M_CanBeExploded;

    obj->hit_points = 20;
    obj->shadow_size = UNIT_SHADOW / 4;
    obj->radius = M_RADIUS;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_WINSTON_ARMY, M_Setup)
