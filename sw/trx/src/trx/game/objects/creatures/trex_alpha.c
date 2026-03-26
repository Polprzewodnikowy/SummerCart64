#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/general/flare_item.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_PIVOT_LENGTH      1800
#define M_HITPOINTS         800
#define M_TOUCH_BITS        0b00110000'00000000
#define M_RADIUS            (WALL_L / 3) // = 341
#define M_RUN_TURN          (DEG_1 * 4) // = 728
#define M_WALK_TURN         (DEG_1 * 2) // = 364
#define M_FRONT_ARC         FRONT_ARC
#define M_RUN_RANGE         SQUARE(WALL_L * 5) // = 26214400
#define M_ATTACK_RANGE      SQUARE(WALL_L * 4) // = 16777216
#define M_HIT_RADIUS        SQUARE(M_RADIUS * 2) // = 465124
#define M_BITE_RANGE        SQUARE(1500) // = 2250000
#define M_TOUCH_DAMAGE      1
#define M_TRAMPLE_DAMAGE    10
#define M_BITE_DAMAGE       10000
#define M_RAPTOR_DAMAGE     (M_TRAMPLE_DAMAGE * 5) // = 50
#define M_ROAR_CHANCE       256
#define M_SMARTNESS         0x7FFF
#define M_ATTACK_FRAME      20
#define M_AGGRESSION_TIME   (LOGIC_FPS * 4) // = 120
#define M_DISTRACTION_COUNT 3
#define M_FLARE_SEEN        (-1)
// clang-format on

typedef enum {
    M_ANIM_KILL = 11,
} M_ANIM;

typedef enum {
    M_STATE_EMPTY,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_RUN,
    M_STATE_ATTACK_1,
    M_STATE_DEATH,
    M_STATE_ROAR,
    M_STATE_ATTACK_2,
    M_STATE_KILL,
    M_STATE_LONG_ROAR_START,
    M_STATE_LONG_ROAR_MID,
    M_STATE_LONG_ROAR_END,
    M_STATE_SNIFF_START,
    M_STATE_SNIFF_MID,
    M_STATE_SNIFF_END,
} M_STATE;

typedef struct {
    int32_t aggression_timer;
    int32_t distraction_count;
} M_PRIV;

static BITE m_Bite = {
    .pos = { .x = 0, .y = 32, .z = 64 },
    .mesh_num = 13,
};

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "aggression_timer", &p->aggression_timer));
    JSON_SHOULD(JSON_READ(io, "distraction_count", &p->distraction_count));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "aggression_timer", p->aggression_timer);
    JSONW_WRITE(io, "distraction_count", p->distraction_count);
}

static void M_KillLara(ITEM *const item)
{
    Lara_TakeDamage(M_BITE_DAMAGE, true);
    Creature_SpecialKill(item, M_ANIM_KILL, M_STATE_KILL, LS_EXTRA_TREX_KILL);
    Lara_Skin_SwapAllExtra(LS_EXTRA_TREX_KILL);
}

static bool M_IsCandidateTarget(const ITEM *const item)
{
    if (item->object_id == O_RAPTOR) {
        return item->status == IS_ACTIVE && item->hit_points > 0;
    }
    if (item->object_id == O_FLARE_ITEM) {
        return FlareItem_IsActive(item) && item->hit_points != M_FLARE_SEEN;
    }
    return false;
}

static void M_CalculateTarget(ITEM *const item)
{
    CREATURE *const creature = item->creature_data;
    if (creature->hurt_by_lara) {
        creature->enemy = Lara_GetItem();
        return;
    }

    creature->enemy = nullptr;
    int32_t best_distance = INT32_MAX;
    Room_GetNearbyRooms(item->pos, WALL_L * 4, 0, item->room_num);

    for (int32_t i = 0; i < Room_DrawGetCount(); i++) {
        const ROOM *const nearby_room = Room_Get(Room_DrawGetRoom(i));
        int16_t target_item_num = nearby_room->item_num;
        while (target_item_num != NO_ITEM) {
            const ITEM *const candidate = Item_Get(target_item_num);
            if (!M_IsCandidateTarget(candidate)) {
                goto loopend;
            }

            const XYZ_32 delta = {
                .x = (candidate->pos.x - item->pos.x) >> 6,
                .y = (candidate->pos.y - item->pos.y) >> 6,
                .z = (candidate->pos.z - item->pos.z) >> 6,
            };
            const int32_t distance = XYZ_32_GetLength2(delta);
            if (distance < best_distance) {
                creature->enemy = (ITEM *)candidate;
                best_distance = distance;
            }
        loopend:
            target_item_num = candidate->next_item;
        }
    }

    if (creature->enemy != nullptr
        && creature->enemy->object_id == O_FLARE_ITEM) {
        creature->enemy->hit_points = 1;
    }
}

static bool M_CanAttack(const ITEM *const item, const ITEM *const target)
{
    if (target == Lara_GetItem()) {
        return (item->touch_bits & M_TOUCH_BITS) != 0;
    }

    if (target == nullptr || Item_GetRelativeFrame(item) != M_ATTACK_FRAME) {
        return false;
    }

    const XYZ_32 pos = {
        .x =
            ABS(target->pos.x
                - (item->pos.x
                   + ((M_PIVOT_LENGTH * Math_Sin(item->rot.y)) >> W2V_SHIFT))),
        .y = ABS(target->pos.y - item->pos.y),
        .z =
            ABS(target->pos.z
                - (item->pos.z
                   + ((M_PIVOT_LENGTH * Math_Cos(item->rot.y)) >> W2V_SHIFT))),
    };
    return pos.x < M_HIT_RADIUS && pos.y <= M_HIT_RADIUS
        && pos.z < M_HIT_RADIUS;
}

static void M_Attack(ITEM *const item, ITEM *const target)
{
    if (target == Lara_GetItem()) {
        Creature_Effect(item, &m_Bite, Spawn_Blood);
        M_KillLara(item);
    } else if (target->object_id == O_RAPTOR) {
        Creature_Effect(item, &m_Bite, Spawn_Blood);
        target->hit_points -= M_RAPTOR_DAMAGE;
        target->hit_status = true;
    } else if (target->object_id == O_FLARE_ITEM) {
        target->hit_points = M_FLARE_SEEN;
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    CREATURE *const creature = item->creature_data;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        item->goal_anim_state = item->current_anim_state == M_STATE_STOP
            ? M_STATE_DEATH
            : M_STATE_STOP;
        goto finish;
    }

    M_CalculateTarget(item);

    AI_INFO info;
    Creature_AIInfo(item, &info);
    Creature_UpdateMood(item, &info, true);

    ITEM *const lara_item = Lara_GetItem();
    if (p->aggression_timer == 0 && p->distraction_count == 0
        && creature->enemy == lara_item) {
        creature->mood = MOOD_BORED;
    }
    Creature_ApplyMood(item, &info, true);

    if (creature->mood == MOOD_BORED) {
        creature->maximum_turn >>= 1;
    }

    angle = Creature_Turn(item, creature->maximum_turn);

    if (item->touch_bits != 0) {
        if (item->current_anim_state == M_STATE_RUN) {
            Lara_TakeDamage(M_TRAMPLE_DAMAGE, false);
        } else {
            Lara_TakeDamage(M_TOUCH_DAMAGE, false);
        }
    }

    creature->flags = creature->mood != MOOD_ESCAPE && !info.ahead
        && info.enemy_facing > -M_FRONT_ARC && info.enemy_facing < M_FRONT_ARC;

    if (creature->flags == 0 && info.distance > M_BITE_RANGE
        && info.distance < M_ATTACK_RANGE && info.bite) {
        creature->flags = 1;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->gun_type != LGT_FLARE
        && (lara_item->current_anim_state == LS(LS_STOP)
            || lara_item->current_anim_state == LS(LS_CROUCH_IDLE))
        && lara_item->current_anim_state == lara_item->goal_anim_state
        && !item->hit_status) {
        p->aggression_timer--;
        CLAMPL(p->aggression_timer, 0);
    } else {
        p->aggression_timer = M_AGGRESSION_TIME;
        p->distraction_count = M_DISTRACTION_COUNT;
    }

    switch (item->current_anim_state) {
    case M_STATE_STOP:
        if (item->required_anim_state != M_STATE_EMPTY) {
            item->goal_anim_state = item->required_anim_state;
        } else if (creature->mood == MOOD_BORED || creature->flags != 0) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (creature->mood == MOOD_ESCAPE) {
            if (lara->target != item && info.ahead && !item->hit_status) {
                item->goal_anim_state = Random_GetControl() < M_ROAR_CHANCE
                    ? M_STATE_ROAR
                    : M_STATE_STOP;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
        } else if (info.distance < M_BITE_RANGE && info.bite) {
            if (p->aggression_timer != 0) {
                item->goal_anim_state = M_STATE_ATTACK_2;
            } else if ((Random_GetControl() & 1) != 0) {
                if (p->distraction_count != 0) {
                    item->goal_anim_state = M_STATE_LONG_ROAR_START;
                }
            } else if (p->distraction_count != 0) {
                item->goal_anim_state = M_STATE_SNIFF_START;
            }
        } else {
            item->goal_anim_state = M_STATE_RUN;
        }
        break;

    case M_STATE_WALK:
        creature->maximum_turn = M_WALK_TURN;
        if (creature->mood != MOOD_BORED || creature->flags == 0) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (info.ahead && Random_GetControl() < M_ROAR_CHANCE) {
            item->required_anim_state = M_STATE_ROAR;
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_RUN:
        creature->maximum_turn = M_RUN_TURN;
        if (info.distance < M_RUN_RANGE && info.bite) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (creature->flags != 0) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (
            creature->mood == MOOD_ESCAPE || !info.ahead
            || Random_GetControl() >= M_ROAR_CHANCE) {
            if (creature->mood == MOOD_BORED
                || (creature->mood == MOOD_ESCAPE && lara->target != item
                    && info.ahead)) {
                item->goal_anim_state = M_STATE_STOP;
            }
        } else {
            item->required_anim_state = M_STATE_ROAR;
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_ROAR:
        creature->maximum_turn = 0;
        break;

    case M_STATE_ATTACK_2:
        creature->maximum_turn = M_WALK_TURN;
        if (M_CanAttack(item, creature->enemy)) {
            if (creature->enemy == lara_item) {
                creature->maximum_turn = 0;
            }
            M_Attack(item, creature->enemy);
        }

        if ((Random_GetControl() & 3) == 0) {
            item->required_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_KILL:
        creature->maximum_turn = 0;
        Creature_Effect(item, &m_Bite, Spawn_Blood);
        break;

    case M_STATE_LONG_ROAR_START:
    case M_STATE_SNIFF_START:
        const bool roar_state =
            item->current_anim_state == M_STATE_LONG_ROAR_START;
        creature->maximum_turn = 0;
        if (p->distraction_count > 0 && Item_TestFrameEqual(item, 0)) {
            p->distraction_count--;
            if (creature->enemy != nullptr
                && creature->enemy->object_id == O_FLARE_ITEM) {
                M_Attack(item, creature->enemy);
                if (roar_state) {
                    p->distraction_count--;
                } else {
                    p->distraction_count = 0;
                }
            }
        }

        if (roar_state) {
            item->goal_anim_state = M_STATE_LONG_ROAR_END;
        }
        break;

    case M_STATE_SNIFF_MID:
        creature->maximum_turn = 0;
        if (!Item_TestFrameEqual(item, 0)) {
            break;
        }
        if ((Random_GetControl() & 1) != 0 && p->distraction_count != 0
            && p->aggression_timer == 0) {
            item->goal_anim_state = M_STATE_SNIFF_MID;
            p->distraction_count--;
            CLAMPL(p->distraction_count, 0);
        } else {
            item->goal_anim_state = M_STATE_SNIFF_END;
        }
        break;
    }

finish:
    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->collision_func = Creature_Collision;
    obj->control_func = M_Control;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->radius = M_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->pivot_length = M_PIVOT_LENGTH;
    obj->smartness = M_SMARTNESS;
    obj->lot_setup = LOT_Setup(LOT_SETUP_BEAST);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    obj->hit_points = M_HITPOINTS;

    Object_GetBone(obj, 9)->rot.y = true;
    Object_GetBone(obj, 11)->rot.y = true;
    Object_GetBone(obj, 20)->rot.y = true;
    Object_GetBone(obj, 22)->rot.y = true;
}

REGISTER_OBJECT(O_TREX_ALPHA, M_Setup)
