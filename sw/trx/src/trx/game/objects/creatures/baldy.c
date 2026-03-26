#include <trx/core/utils.h>
#include <trx/game/const.h>
#include <trx/game/creature.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/spawn.h>

#define BALDY_SHOT_DAMAGE 150
#define BALDY_WALK_TURN (DEG_1 * 3) // = 546
#define BALDY_RUN_TURN (DEG_1 * 6) // = 1092
#define BALDY_WALK_RANGE SQUARE(WALL_L * 4) // = 16777216
#define BALDY_DIE_ANIM 14
#define BALDY_HITPOINTS 200
#define BALDY_RADIUS (WALL_L / 10) // = 102
#define BALDY_SMARTNESS 0x7FFF

typedef enum {
    BALDY_STATE_EMPTY = 0,
    BALDY_STATE_STOP = 1,
    BALDY_STATE_WALK = 2,
    BALDY_STATE_RUN = 3,
    BALDY_STATE_AIM = 4,
    BALDY_STATE_DEATH = 5,
    BALDY_STATE_SHOOT = 6,
} BALDY_STATE;

static const CREATURE_GUN m_BaldyGun = {
    .muzzle = { .pos = { -20, 440, 20 }, .mesh_num = 9 },
};

static void M_Initialise(const int16_t item_num)
{
    Creature_Initialise(item_num);
    Item_Get(item_num)->current_anim_state = BALDY_STATE_RUN;
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->hit_points <= 0) {
            const uint16_t flags =
                Music_GetTrackFlags(Music_ToGameID(MX_BALDY_SPEECH));
            Music_SetTrackFlags(
                Music_ToGameID(MX_BALDY_SPEECH), flags | IF_ONE_SHOT);
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

    CREATURE *const baldy = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;
    int16_t tilt = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != BALDY_STATE_DEATH) {
            item->current_anim_state = BALDY_STATE_DEATH;
            Item_SwitchToAnim(item, BALDY_DIE_ANIM, 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, true);

        angle = Creature_Turn(item, baldy->maximum_turn);

        switch (item->current_anim_state) {
        case BALDY_STATE_STOP:
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = BALDY_STATE_AIM;
            } else if (baldy->mood == MOOD_BORED) {
                item->goal_anim_state = BALDY_STATE_WALK;
            } else {
                item->goal_anim_state = BALDY_STATE_RUN;
            }
            break;

        case BALDY_STATE_WALK:
            baldy->maximum_turn = BALDY_WALK_TURN;
            if (baldy->mood == MOOD_ESCAPE || !info.ahead) {
                item->required_anim_state = BALDY_STATE_RUN;
                item->goal_anim_state = BALDY_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->required_anim_state = BALDY_STATE_AIM;
                item->goal_anim_state = BALDY_STATE_STOP;
            } else if (info.distance > BALDY_WALK_RANGE) {
                item->required_anim_state = BALDY_STATE_RUN;
                item->goal_anim_state = BALDY_STATE_STOP;
            }
            break;

        case BALDY_STATE_RUN:
            baldy->maximum_turn = BALDY_RUN_TURN;
            tilt = angle / 2;
            if (baldy->mood != MOOD_ESCAPE || info.ahead) {
                if (Creature_CanTargetEnemy(item, &info)) {
                    item->required_anim_state = BALDY_STATE_AIM;
                    item->goal_anim_state = BALDY_STATE_STOP;
                } else if (info.ahead && info.distance < BALDY_WALK_RANGE) {
                    item->required_anim_state = BALDY_STATE_WALK;
                    item->goal_anim_state = BALDY_STATE_STOP;
                }
            }
            break;

        case BALDY_STATE_AIM:
            baldy->flags = 0;
            if (item->required_anim_state) {
                item->goal_anim_state = BALDY_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = BALDY_STATE_SHOOT;
            } else {
                item->goal_anim_state = BALDY_STATE_STOP;
            }
            break;

        case BALDY_STATE_SHOOT:
            if (!baldy->flags) {
                info.distance /= 2;
                Creature_Shoot(
                    item, &info, &m_BaldyGun, head, BALDY_SHOT_DAMAGE);
                baldy->flags = 1;
            }
            if (baldy->mood == MOOD_ESCAPE) {
                item->required_anim_state = BALDY_STATE_RUN;
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
    obj->initialise_func = M_Initialise;
    obj->handle_save_func = M_HandleSave;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = BALDY_HITPOINTS;
    obj->radius = BALDY_RADIUS;
    obj->smartness = BALDY_SMARTNESS;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 0)->rot.y = true;
}

REGISTER_OBJECT(O_BALDY, M_Setup)
