#include <trx/core/utils.h>
#include <trx/game/const.h>
#include <trx/game/creature.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>

#define LARSON_POSE_CHANCE 0x60 // = 96
#define LARSON_SHOT_DAMAGE 50
#define LARSON_WALK_TURN (DEG_1 * 3) // = 546
#define LARSON_RUN_TURN (DEG_1 * 6) // = 1092
#define LARSON_WALK_RANGE SQUARE(WALL_L * 3) // = 9437184
#define LARSON_DIE_ANIM 15
#define LARSON_HITPOINTS 50
#define LARSON_RADIUS (WALL_L / 10) // = 102
#define LARSON_SMARTNESS 0x7FFF

typedef enum {
    LARSON_STATE_EMPTY = 0,
    LARSON_STATE_STOP = 1,
    LARSON_STATE_WALK = 2,
    LARSON_STATE_RUN = 3,
    LARSON_STATE_AIM = 4,
    LARSON_STATE_DEATH = 5,
    LARSON_STATE_POSE = 6,
    LARSON_STATE_SHOOT = 7,
} LARSON_STATE;

static const CREATURE_GUN m_LarsonGun = {
    .muzzle = { .pos = { -60, 170, 0 }, .mesh_num = 14 },
};

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->hit_points <= 0) {
            const uint16_t flags =
                Music_GetTrackFlags(Music_ToGameID(MX_LARSON_SPEECH));
            Music_SetTrackFlags(
                Music_ToGameID(MX_LARSON_SPEECH), flags | IF_ONE_SHOT);
        }
    }
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

    CREATURE *const person = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;
    int16_t tilt = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != LARSON_STATE_DEATH) {
            item->current_anim_state = LARSON_STATE_DEATH;
            Item_SwitchToAnim(item, LARSON_DIE_ANIM, 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, person->maximum_turn);

        switch (item->current_anim_state) {
        case LARSON_STATE_STOP:
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (person->mood == MOOD_BORED) {
                item->goal_anim_state = Random_GetControl() < LARSON_POSE_CHANCE
                    ? LARSON_STATE_POSE
                    : LARSON_STATE_WALK;
            } else if (person->mood == MOOD_ESCAPE) {
                item->goal_anim_state = LARSON_STATE_RUN;
            } else {
                item->goal_anim_state = LARSON_STATE_WALK;
            }
            break;

        case LARSON_STATE_POSE:
            if (person->mood != MOOD_BORED) {
                item->goal_anim_state = LARSON_STATE_STOP;
            } else if (Random_GetControl() < LARSON_POSE_CHANCE) {
                item->required_anim_state = LARSON_STATE_WALK;
                item->goal_anim_state = LARSON_STATE_STOP;
            }
            break;

        case LARSON_STATE_WALK:
            person->maximum_turn = LARSON_WALK_TURN;
            if (person->mood == MOOD_BORED
                && Random_GetControl() < LARSON_POSE_CHANCE) {
                item->required_anim_state = LARSON_STATE_POSE;
                item->goal_anim_state = LARSON_STATE_STOP;
            } else if (person->mood == MOOD_ESCAPE) {
                item->required_anim_state = LARSON_STATE_RUN;
                item->goal_anim_state = LARSON_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->required_anim_state = LARSON_STATE_AIM;
                item->goal_anim_state = LARSON_STATE_STOP;
            } else if (!info.ahead || info.distance > LARSON_WALK_RANGE) {
                item->required_anim_state = LARSON_STATE_RUN;
                item->goal_anim_state = LARSON_STATE_STOP;
            }
            break;

        case LARSON_STATE_RUN:
            person->maximum_turn = LARSON_RUN_TURN;
            tilt = angle / 2;
            if (person->mood == MOOD_BORED
                && Random_GetControl() < LARSON_POSE_CHANCE) {
                item->required_anim_state = LARSON_STATE_POSE;
                item->goal_anim_state = LARSON_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->required_anim_state = LARSON_STATE_AIM;
                item->goal_anim_state = LARSON_STATE_STOP;
            } else if (info.ahead && info.distance < LARSON_WALK_RANGE) {
                item->required_anim_state = LARSON_STATE_WALK;
                item->goal_anim_state = LARSON_STATE_STOP;
            }
            break;

        case LARSON_STATE_AIM:
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = LARSON_STATE_SHOOT;
            } else {
                item->goal_anim_state = LARSON_STATE_STOP;
            }
            break;

        case LARSON_STATE_SHOOT:
            if (!item->required_anim_state) {
                Creature_Shoot(
                    item, &info, &m_LarsonGun, head, LARSON_SHOT_DAMAGE);
                item->required_anim_state = LARSON_STATE_AIM;
            }
            if (person->mood == MOOD_ESCAPE) {
                item->required_anim_state = LARSON_STATE_STOP;
            }
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Head(item, head);

    Creature_Animate(item_num, angle, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = Creature_Initialise;
    obj->handle_save_func = M_HandleSave;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = LARSON_HITPOINTS;
    obj->radius = LARSON_RADIUS;
    obj->smartness = LARSON_SMARTNESS;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 6)->rot.y = true;
}

REGISTER_OBJECT(O_LARSON, M_Setup)
