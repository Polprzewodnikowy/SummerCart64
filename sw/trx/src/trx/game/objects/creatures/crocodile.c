#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/pathing.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

// clang-format off
#define M_SHADOW_SIZE              (UNIT_SHADOW / (g_TRVersion < 3 ? 3 : 2))
#define M_PIVOT_LENGTH             (g_TRVersion < 3 ? 600 : 200)
#define M_HITPOINTS                (g_TRVersion < 3 ? 20 : 42)

#define M_CROCODILE_BITE_DAMAGE    100
#define M_CROCODILE_BITE_RANGE     SQUARE(435) // = 189225
#define M_CROCODILE_FASTTURN_ANGLE 0x4000
#define M_CROCODILE_FASTTURN_RANGE SQUARE(WALL_L * 3) // = 9437184
#define M_CROCODILE_FASTTURN_TURN  (6 * DEG_1) // = 1092
#define M_CROCODILE_TOUCH          0x3FC
#define M_CROCODILE_TURN           (3 * DEG_1) // = 546
#define M_CROCODILE_HITPOINTS      M_HITPOINTS
#define M_CROCODILE_RADIUS         (WALL_L / 3) // = 341
#define M_CROCODILE_SMARTNESS      0x2000

#define M_ALLIGATOR_BITE_DAMAGE    100
#define M_ALLIGATOR_FLOAT_SPEED    (WALL_L / 32) // = 32
#define M_ALLIGATOR_TURN           (3 * DEG_1) // = 546
#define M_ALLIGATOR_HITPOINTS      M_HITPOINTS
#define M_ALLIGATOR_RADIUS         (WALL_L / 3) // = 341
#define M_ALLIGATOR_SMARTNESS      0x400
#define M_ALLIGATOR_BITE_FRAME     42
// clang-format on

typedef enum {
    M_CROCODILE_DIE_ANIM = 11,
} M_CROCODILE_ANIM;

typedef enum {
    M_ALLIGATOR_DIE_ANIM = 4,
} M_ALLIGATOR_ANIM;

typedef enum {
    M_CROCODILE_STATE_EMPTY = 0,
    M_CROCODILE_STATE_STOP = 1,
    M_CROCODILE_STATE_RUN = 2,
    M_CROCODILE_STATE_WALK = 3,
    M_CROCODILE_STATE_FAST_TURN = 4,
    M_CROCODILE_STATE_ATTACK_1 = 5,
    M_CROCODILE_STATE_ATTACK_2 = 6,
    M_CROCODILE_STATE_DEATH = 7,
} M_CROCODILE_STATE;

typedef enum {
    M_ALLIGATOR_STATE_EMPTY = 0,
    M_ALLIGATOR_STATE_SWIM = 1,
    M_ALLIGATOR_STATE_ATTACK = 2,
    M_ALLIGATOR_STATE_DEATH = 3,
} M_ALLIGATOR_STATE;

static BITE m_CrocodileBite = { .pos = { 5, -21, 467 }, .mesh_num = 9 };

static const HYBRID_INFO m_CrocodileInfo = {
    .land.id = O_CROCODILE,
    .land.active_anim = M_CROCODILE_STATE_EMPTY,
    .land.death_anim = M_CROCODILE_DIE_ANIM,
    .land.death_state = M_CROCODILE_STATE_DEATH,
    .water.id = O_ALLIGATOR,
    .water.active_anim = M_ALLIGATOR_STATE_EMPTY,
    .water.death_anim = M_ALLIGATOR_DIE_ANIM,
    .water.death_state = M_ALLIGATOR_STATE_DEATH,
};

static void M_UpdateCreatureLOT(const ITEM *const item)
{
    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    OBJECT *const obj = Object_Get(item->object_id);
    creature->lot.setup = obj->lot_setup;
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (Creature_EnsureHabitat(
                Item_GetIndex(item), nullptr, &m_CrocodileInfo)) {
            M_UpdateCreatureLOT(item);
        }
    }
}

static void M_ControlCrocodile(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const croc = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_CROCODILE_STATE_DEATH) {
            item->current_anim_state = M_CROCODILE_STATE_DEATH;
            Item_SwitchToAnim(item, M_CROCODILE_DIE_ANIM, 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, true);

        if (item->current_anim_state == M_CROCODILE_STATE_FAST_TURN) {
            item->rot.y += M_CROCODILE_FASTTURN_TURN;
        } else {
            angle = Creature_Turn(item, M_CROCODILE_TURN);
        }

        switch (item->current_anim_state) {
        case M_CROCODILE_STATE_STOP:
            if (info.bite && info.distance < M_CROCODILE_BITE_RANGE) {
                item->goal_anim_state = M_CROCODILE_STATE_ATTACK_1;
            } else if (croc->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_CROCODILE_STATE_RUN;
            } else if (croc->mood == MOOD_ATTACK) {
                if ((info.angle < -M_CROCODILE_FASTTURN_ANGLE
                     || info.angle > M_CROCODILE_FASTTURN_ANGLE)
                    && info.distance > M_CROCODILE_FASTTURN_RANGE) {
                    item->goal_anim_state = M_CROCODILE_STATE_FAST_TURN;
                } else {
                    item->goal_anim_state = M_CROCODILE_STATE_RUN;
                }
            } else if (croc->mood == MOOD_STALK) {
                item->goal_anim_state = M_CROCODILE_STATE_WALK;
            }
            break;

        case M_CROCODILE_STATE_WALK:
            if (info.ahead && (item->touch_bits & M_CROCODILE_TOUCH)) {
                item->goal_anim_state = M_CROCODILE_STATE_STOP;
            } else if (croc->mood == MOOD_ATTACK || croc->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_CROCODILE_STATE_RUN;
            } else if (croc->mood == MOOD_BORED) {
                item->goal_anim_state = M_CROCODILE_STATE_STOP;
            }
            break;

        case M_CROCODILE_STATE_FAST_TURN:
            if (info.angle > -M_CROCODILE_FASTTURN_ANGLE
                && info.angle < M_CROCODILE_FASTTURN_ANGLE) {
                item->goal_anim_state = M_CROCODILE_STATE_WALK;
            }
            break;

        case M_CROCODILE_STATE_RUN:
            if (info.ahead && (item->touch_bits & M_CROCODILE_TOUCH)) {
                item->goal_anim_state = M_CROCODILE_STATE_STOP;
            } else if (croc->mood == MOOD_STALK) {
                item->goal_anim_state = M_CROCODILE_STATE_WALK;
            } else if (croc->mood == MOOD_BORED) {
                item->goal_anim_state = M_CROCODILE_STATE_STOP;
            } else if (
                croc->mood == MOOD_ATTACK
                && info.distance > M_CROCODILE_FASTTURN_RANGE
                && (info.angle < -M_CROCODILE_FASTTURN_ANGLE
                    || info.angle > M_CROCODILE_FASTTURN_ANGLE)) {
                item->goal_anim_state = M_CROCODILE_STATE_STOP;
            }
            break;

        case M_CROCODILE_STATE_ATTACK_1:
            if (item->required_anim_state == M_CROCODILE_STATE_EMPTY) {
                Creature_Effect(item, &m_CrocodileBite, Spawn_Blood);
                Lara_TakeDamage(M_CROCODILE_BITE_DAMAGE, true);
                item->required_anim_state = M_CROCODILE_STATE_STOP;
            }
            break;
        }
    }

    if (croc != nullptr) {
        Creature_Head(item, head);
    }

    // Test conversion to alligator and set relevant pathfinding values.
    if (Creature_EnsureHabitat(item_num, nullptr, &m_CrocodileInfo)) {
        M_UpdateCreatureLOT(item);
    }

    if (croc != nullptr) {
        Creature_Animate(item_num, angle, 0);
    } else {
        Item_Animate(item);
    }
}

static void M_ControlAlligator(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const gator = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_ALLIGATOR_STATE_DEATH) {
            item->current_anim_state = M_ALLIGATOR_STATE_DEATH;
            Item_SwitchToAnim(item, M_ALLIGATOR_DIE_ANIM, 0);
            item->hit_points = 0;
            Carrier_TestItemDrops(item_num);
        }

        // Test if we should convert to a crocodile. If not, control the death
        // pose of the alligator in the water.
        if (g_TRVersion >= 3
            || !Creature_EnsureHabitat(item_num, nullptr, &m_CrocodileInfo)) {
            Creature_Float(item_num);
        }
        return;
    }

    AI_INFO info;
    Creature_AIInfo(item, &info);

    if (info.ahead) {
        head = info.angle;
    }

    Creature_UpdateMood(item, &info, true);
    if (g_TRVersion >= 3) {
        const ITEM *const lara_item = Lara_GetItem();
        if (!Room_Get(lara_item->room_num)->flags.underwater
            && !Lara_Vehicle_IsMounted()) {
            gator->mood = MOOD_BORED;
        }
    }
    Creature_ApplyMood(item, &info, true);

    Creature_Turn(item, M_ALLIGATOR_TURN);

    switch (item->current_anim_state) {
    case M_ALLIGATOR_STATE_SWIM:
        if (info.bite && item->touch_bits) {
            item->goal_anim_state = M_ALLIGATOR_STATE_ATTACK;
            if (g_Config.gameplay.fix_alligator_ai) {
                item->required_anim_state = M_ALLIGATOR_STATE_SWIM;
            }
        }
        break;

    case M_ALLIGATOR_STATE_ATTACK:
        if (item->frame_num
            == (g_Config.gameplay.fix_alligator_ai
                    ? M_ALLIGATOR_BITE_FRAME
                    : Item_GetAnim(item)->frame_base)) {
            item->required_anim_state = M_ALLIGATOR_STATE_EMPTY;
        }

        if (info.bite && item->touch_bits) {
            if (item->required_anim_state == M_ALLIGATOR_STATE_EMPTY) {
                Creature_Effect(item, &m_CrocodileBite, Spawn_Blood);
                Lara_TakeDamage(M_ALLIGATOR_BITE_DAMAGE, true);
                item->required_anim_state = M_ALLIGATOR_STATE_SWIM;
            }
            if (g_Config.gameplay.fix_alligator_ai) {
                item->goal_anim_state = M_ALLIGATOR_STATE_SWIM;
            }
        } else {
            item->goal_anim_state = M_ALLIGATOR_STATE_SWIM;
        }
        break;
    }

    Creature_Joint(item, 0, head);
    if (g_TRVersion < 3) {
        int32_t wh = 0;
        if (Creature_EnsureHabitat(item_num, &wh, &m_CrocodileInfo)) {
            M_UpdateCreatureLOT(item);
        } else {
            CLAMPL(item->pos.y, wh + STEP_L);
        }
    } else {
        Creature_Underwater(item, STEP_L);
    }

    Creature_Animate(item_num, angle, 0);
}

static void M_SetupBase(OBJECT *const obj)
{
    obj->initialise_func = Creature_Initialise;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = M_SHADOW_SIZE;
    obj->pivot_length = M_PIVOT_LENGTH;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;
    obj->handle_save_func = M_HandleSave;
    Object_GetBone(obj, 7)->rot.y = true;
}

static void M_SetupCrocodile(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    M_SetupBase(obj);
    obj->control_func = M_ControlCrocodile;
    obj->hit_points = M_CROCODILE_HITPOINTS;
    obj->radius = M_CROCODILE_RADIUS;
    obj->smartness = M_CROCODILE_SMARTNESS;
}

static void M_SetupAlligator(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    M_SetupBase(obj);
    obj->control_func = M_ControlAlligator;
    obj->hit_points = M_ALLIGATOR_HITPOINTS;
    obj->radius = M_ALLIGATOR_RADIUS;
    obj->smartness = M_ALLIGATOR_SMARTNESS;
    obj->lot_setup = LOT_Setup(LOT_SETUP_FLYER);
}

REGISTER_OBJECT(O_ALLIGATOR, M_SetupAlligator)
REGISTER_OBJECT(O_CROCODILE, M_SetupCrocodile)
