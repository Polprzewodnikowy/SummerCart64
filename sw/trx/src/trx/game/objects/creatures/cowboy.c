#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/effects.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/savegame.h>
#include <trx/game/spawn.h>

#define COWBOY_SHOT_DAMAGE 70
#define COWBOY_WALK_TURN (DEG_1 * 3) // = 546
#define COWBOY_RUN_TURN (DEG_1 * 6) // = 1092
#define COWBOY_WALK_RANGE SQUARE(WALL_L * 3) // = 9437184
#define COWBOY_DIE_ANIM 7
#define COWBOY_HITPOINTS 150
#define COWBOY_RADIUS (WALL_L / 10) // = 102
#define COWBOY_SMARTNESS 0x7FFF

typedef enum {
    COWBOY_STATE_EMPTY = 0,
    COWBOY_STATE_STOP = 1,
    COWBOY_STATE_WALK = 2,
    COWBOY_STATE_RUN = 3,
    COWBOY_STATE_AIM = 4,
    COWBOY_STATE_DEATH = 5,
    COWBOY_STATE_SHOOT = 6,
} COWBOY_STATE;

static const CREATURE_GUN m_CowboyGun1 = {
    .muzzle = { .pos = { 1, 200, 41 }, .mesh_num = 5 },
};
static const CREATURE_GUN m_CowboyGun2 = {
    .muzzle = { .pos = { -2, 200, 40 }, .mesh_num = 8 },
};

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->hit_points <= 0) {
            const uint16_t flags =
                Music_GetTrackFlags(Music_ToGameID(MX_COWBOY_SPEECH));
            Music_SetTrackFlags(
                Music_ToGameID(MX_COWBOY_SPEECH), flags | IF_ONE_SHOT);
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

    CREATURE *const cowboy = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;
    int16_t tilt = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != COWBOY_STATE_DEATH) {
            item->current_anim_state = COWBOY_STATE_DEATH;
            Item_SwitchToAnim(item, COWBOY_DIE_ANIM, 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, cowboy->maximum_turn);

        switch (item->current_anim_state) {
        case COWBOY_STATE_STOP:
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = COWBOY_STATE_AIM;
            } else if (cowboy->mood == MOOD_BORED) {
                item->goal_anim_state = COWBOY_STATE_WALK;
            } else {
                item->goal_anim_state = COWBOY_STATE_RUN;
            }
            break;

        case COWBOY_STATE_WALK:
            cowboy->maximum_turn = COWBOY_WALK_TURN;
            if (cowboy->mood == MOOD_ESCAPE || !info.ahead) {
                item->required_anim_state = COWBOY_STATE_RUN;
                item->goal_anim_state = COWBOY_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->required_anim_state = COWBOY_STATE_AIM;
                item->goal_anim_state = COWBOY_STATE_STOP;
            } else if (info.distance > COWBOY_WALK_RANGE) {
                item->required_anim_state = COWBOY_STATE_RUN;
                item->goal_anim_state = COWBOY_STATE_STOP;
            }
            break;

        case COWBOY_STATE_RUN:
            cowboy->maximum_turn = COWBOY_RUN_TURN;
            tilt = angle / 2;
            if (cowboy->mood != MOOD_ESCAPE || info.ahead) {
                if (Creature_CanTargetEnemy(item, &info)) {
                    item->required_anim_state = COWBOY_STATE_AIM;
                    item->goal_anim_state = COWBOY_STATE_STOP;
                } else if (info.ahead && info.distance < COWBOY_WALK_RANGE) {
                    item->required_anim_state = COWBOY_STATE_WALK;
                    item->goal_anim_state = COWBOY_STATE_STOP;
                }
            }
            break;

        case COWBOY_STATE_AIM:
            cowboy->flags = 0;
            if (item->required_anim_state) {
                item->goal_anim_state = COWBOY_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = COWBOY_STATE_SHOOT;
            } else {
                item->goal_anim_state = COWBOY_STATE_STOP;
            }
            break;

        case COWBOY_STATE_SHOOT:
            if (!cowboy->flags) {
                Creature_Shoot(
                    item, &info, &m_CowboyGun1, head, COWBOY_SHOT_DAMAGE);
            } else if (cowboy->flags == 6) {
                if (Creature_CanTargetEnemy(item, &info)) {
                    Creature_Shoot(
                        item, &info, &m_CowboyGun2, head, COWBOY_SHOT_DAMAGE);
                } else {
                    int16_t effect_num = Creature_Effect(
                        item, &m_CowboyGun2.muzzle, Spawn_GunShot);
                    if (effect_num != NO_EFFECT) {
                        Effect_Get(effect_num)->rot.y += head;
                    }
                }
            }
            cowboy->flags++;

            if (cowboy->mood == MOOD_ESCAPE) {
                item->required_anim_state = COWBOY_STATE_RUN;
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
    obj->hit_points = COWBOY_HITPOINTS;
    obj->radius = COWBOY_RADIUS;
    obj->smartness = COWBOY_SMARTNESS;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 0)->rot.y = true;
}

REGISTER_OBJECT(O_COWBOY, M_Setup)
