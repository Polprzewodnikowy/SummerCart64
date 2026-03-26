#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

// clang-format off
#define M_ATTACK_RANGE   SQUARE(WALL_L * 3 / 2) // = 2359296
#define M_BITE_DAMAGE    100
#define M_CHARGE_DAMAGE  100
#define M_CLOSE_RANGE    SQUARE(g_TRVersion < 3 ? 680 : 585) // = 462400 / 342225
#define M_LUNGE_DAMAGE   100
#define M_LUNGE_RANGE    SQUARE(WALL_L * 3 / 2) // = 2359296
#define M_ROAR_CHANCE    (g_TRVersion < 3 ? 256 : 128)
#define M_RUN_TURN       (4 * DEG_1) // = 728
#define M_TOUCH          0xFF7C00
#define M_WALK_TURN      (g_TRVersion < 3 ? (1 * DEG_1) : (2 * DEG_1)) // = 182 / 364
#define M_HITPOINTS      (g_TRVersion == 3 ? 90 : 20)
#define M_RADIUS         (WALL_L / 3) // = 341
#define M_PIVOT_LENGTH   (g_TRVersion == 3 ? 600 : 400)
#define M_SMARTNESS      0x4000
#define M_INFIGHT_CHANCE 0x400
#define M_INFIGHT_RANGE  0x400000
// clang-format on

typedef enum {
    M_STATE_EMPTY,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_RUN,
    M_STATE_ATTACK_1,
    M_STATE_DEATH,
    M_STATE_WARNING,
    M_STATE_ATTACK_2,
    M_STATE_ATTACK_3,
} M_STATE;

typedef enum {
    M_ANIM_DEATH = 9,
} M_ANIM;

static BITE m_RaptorBite = {
    .pos = { 0, 66, 318 },
    .mesh_num = 22,
};

static void M_CalculateTarget(const ITEM *const item)
{
    CREATURE *const raptor = item->creature_data;
    int32_t best_distance = INT32_MAX;
    const ITEM *best_item = nullptr;

    for (int32_t i = 0; i < LOT_SLOT_COUNT; i++) {
        const CREATURE *const creature = LOT_GetBaddieSlot(i);
        if (creature->item_num == NO_ITEM || creature == raptor) {
            continue;
        }

        const ITEM *const candidate = Item_Get(creature->item_num);
        const XYZ_32 delta = {
            .x = (candidate->pos.x - item->pos.x) >> 6,
            .y = (candidate->pos.y - item->pos.y) >> 6,
            .z = (candidate->pos.z - item->pos.z) >> 6,
        };
        const int32_t distance = XYZ_32_GetLength2(delta);
        if (distance < best_distance) {
            best_item = candidate;
            best_distance = distance;
        }
    }

    if (best_item != nullptr
        && (best_item->object_id != item->object_id
            || (Random_GetControl() < M_INFIGHT_CHANCE
                && best_distance < M_INFIGHT_RANGE))) {
        raptor->enemy = (ITEM *)best_item;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const XYZ_32 lara_delta = {
        .x = (lara_item->pos.x - item->pos.x) >> 6,
        .y = (lara_item->pos.y - item->pos.y) >> 6,
        .z = (lara_item->pos.z - item->pos.z) >> 6,
    };
    if (XYZ_32_GetLength2(lara_delta) < best_distance) {
        raptor->enemy = (ITEM *)lara_item;
    }
}

static void M_Attack(ITEM *const item, const AI_INFO *const info)
{
    int32_t damage = 0;
    int16_t next_state = M_STATE_EMPTY;
    switch (item->current_anim_state) {
    case M_STATE_ATTACK_1:
        damage = M_LUNGE_DAMAGE;
        next_state = M_STATE_STOP;
        break;
    case M_STATE_ATTACK_2:
        damage = M_CHARGE_DAMAGE;
        next_state = M_STATE_RUN;
        break;
    case M_STATE_ATTACK_3:
        damage = M_BITE_DAMAGE;
        next_state = M_STATE_STOP;
        break;
    default:
        return;
    }

    if (g_TRVersion < 3) {
        if (item->required_anim_state == M_STATE_EMPTY && info->ahead
            && (item->touch_bits & M_TOUCH) != 0) {
            Creature_Effect(item, &m_RaptorBite, Spawn_Blood);
            Lara_TakeDamage(damage, true);
            item->required_anim_state = next_state;
        }
        return;
    }

    CREATURE *const raptor = item->creature_data;
    const ITEM *const lara_item = Lara_GetItem();
    if (raptor->enemy == lara_item) {
        if ((raptor->flags & 1) != 0 || (item->touch_bits & M_TOUCH) == 0) {
            return;
        }

        raptor->flags |= 1;
        Creature_Effect(item, &m_RaptorBite, Spawn_Blood);
        if (lara_item->hit_points <= 0) {
            raptor->flags |= 2;
        }

        Lara_TakeDamage(damage, true);
        item->required_anim_state = next_state;
        return;
    }

    if ((raptor->flags & 1) != 0 || raptor->enemy == nullptr
        || !XYZ_32_IsNearby(raptor->enemy->pos, item->pos, WALL_L / 2)) {
        return;
    }

    raptor->flags |= 1;
    Creature_Effect(item, &m_RaptorBite, Spawn_Blood);

    raptor->enemy->hit_points -= damage / 4;
    raptor->enemy->hit_status = true;
    if (raptor->enemy->hit_points <= 0) {
        raptor->flags |= 2;
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const raptor = item->creature_data;
    int16_t head = 0;
    int16_t neck = 0;
    int16_t angle = 0;
    int16_t tilt = 0;

    const bool is_tr3 = g_TRVersion == 3;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            item->current_anim_state = M_STATE_DEATH;
            const int32_t offset = Random_GetControl() > 0x4000 ? 0 : 1;
            Item_SwitchToAnim(item, M_ANIM_DEATH + offset, 0);
        }
        goto finish;
    }

    if (is_tr3
        && (raptor->enemy == nullptr || (Random_GetControl() & 0x7F) == 0)) {
        M_CalculateTarget(item);
    }

    if (item->ai_bits != 0) {
        Creature_GetAITarget(raptor);
    }

    AI_INFO info;
    Creature_AIInfo(item, &info);

    if (info.ahead) {
        head = info.angle;
    }

    Creature_Mood(item, &info, true);
    if (is_tr3 && raptor->mood == MOOD_BORED) {
        raptor->maximum_turn /= 2;
    }

    angle = Creature_Turn(item, raptor->maximum_turn);
    neck = angle * -6;

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();

    switch (item->current_anim_state) {
    case M_STATE_STOP:
        raptor->flags &= ~1;
        raptor->maximum_turn = 0;

        if (item->required_anim_state != M_STATE_EMPTY) {
            item->goal_anim_state = item->required_anim_state;
        } else if (is_tr3 && (raptor->flags & 2) != 0) {
            raptor->flags &= ~2;
            item->goal_anim_state = M_STATE_WARNING;
        } else if ((item->touch_bits & M_TOUCH) != 0) {
            item->goal_anim_state = M_STATE_ATTACK_3;
        } else if (info.bite && info.distance < M_CLOSE_RANGE) {
            item->goal_anim_state = M_STATE_ATTACK_3;
        } else if (info.bite && info.distance < M_LUNGE_RANGE) {
            item->goal_anim_state = M_STATE_ATTACK_1;
        } else if (
            is_tr3 && raptor->mood == MOOD_ESCAPE && lara->target != item
            && info.ahead && !item->hit_status) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (raptor->mood == MOOD_BORED) {
            item->goal_anim_state = M_STATE_WALK;
        } else {
            item->goal_anim_state = M_STATE_RUN;
        }
        break;

    case M_STATE_WALK:
        raptor->flags &= ~1;
        raptor->maximum_turn = M_WALK_TURN;

        if (raptor->mood != MOOD_BORED) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (info.ahead && Random_GetControl() < M_ROAR_CHANCE) {
            item->required_anim_state = M_STATE_WARNING;
            item->goal_anim_state = M_STATE_STOP;
            raptor->flags &= ~2;
        }
        break;

    case M_STATE_RUN:
        tilt = angle;
        raptor->flags &= ~1;
        raptor->maximum_turn = M_RUN_TURN;

        if ((item->touch_bits & M_TOUCH) != 0) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (is_tr3 && (raptor->flags & 2) != 0) {
            item->goal_anim_state = M_STATE_STOP;
            item->required_anim_state = M_STATE_WARNING;
            raptor->flags &= ~2;
        } else if (info.bite && info.distance < M_ATTACK_RANGE) {
            if (item->goal_anim_state == M_STATE_RUN) {
                if (Random_GetControl() < 0x2000) {
                    item->goal_anim_state = M_STATE_STOP;
                } else {
                    item->goal_anim_state = M_STATE_ATTACK_2;
                }
            }
        } else if (
            info.ahead && raptor->mood != MOOD_ESCAPE
            && Random_GetControl() < M_ROAR_CHANCE
            && (!is_tr3
                || (raptor->enemy != nullptr
                    && raptor->enemy->object_id != O_ANIMATING_6))) {
            item->required_anim_state = M_STATE_WARNING;
            item->goal_anim_state = M_STATE_STOP;
        } else if (raptor->mood == MOOD_BORED) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (
            is_tr3 && raptor->mood == MOOD_ESCAPE && lara->target != item
            && info.ahead) {
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_ATTACK_1:
    case M_STATE_ATTACK_2:
    case M_STATE_ATTACK_3:
        raptor->maximum_turn = M_WALK_TURN;
        tilt = angle;
        M_Attack(item, &info);
        break;

    default:
        break;
    }

finish:
    Creature_Tilt(item, tilt);
    if (is_tr3) {
        Creature_Joint(item, 0, head / 2);
        Creature_Joint(item, 1, head / 2);
        Creature_Joint(item, 2, neck);
        Creature_Joint(item, 3, neck);
    } else {
        Creature_Head(item, head);
    }
    Creature_Animate(item_num, angle, tilt);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = Creature_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = M_HITPOINTS;
    obj->pivot_length = M_PIVOT_LENGTH;
    obj->radius = M_RADIUS;
    obj->smartness = M_SMARTNESS;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 21)->rot.y = true;
    if (g_TRVersion >= 3) {
        Object_GetBone(obj, 20)->rot.y = true;
        Object_GetBone(obj, 23)->rot.y = true;
        Object_GetBone(obj, 25)->rot.y = true;
    }
}

REGISTER_OBJECT(O_RAPTOR, M_Setup)
