#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/music.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>

#define SKATE_KID_STOP_SHOT_DAMAGE 50
#define SKATE_KID_SKATE_SHOT_DAMAGE 40
#define SKATE_KID_STOP_RANGE SQUARE(WALL_L * 4) // = 16777216
#define SKATE_KID_DONT_STOP_RANGE SQUARE(WALL_L * 5 / 2) // = 6553600
#define SKATE_KID_TOO_CLOSE SQUARE(WALL_L) // = 1048576
#define SKATE_KID_SKATE_TURN (DEG_1 * 4) // = 728
#define SKATE_KID_PUSH_CHANCE 0x200
#define SKATE_KID_SKATE_CHANCE 0x400
#define SKATE_KID_DIE_ANIM 13
#define SKATE_KID_HITPOINTS 125
#define SKATE_KID_RADIUS (WALL_L / 5) // = 204
#define SKATE_KID_SMARTNESS 0x7FFF
#define SKATE_KID_SPEECH_HITPOINTS 120
#define SKATE_KID_SPEECH_STARTED 1

typedef enum {
    SKATE_KID_STATE_STOP = 0,
    SKATE_KID_STATE_SHOOT_1 = 1,
    SKATE_KID_STATE_SKATE = 2,
    SKATE_KID_STATE_PUSH = 3,
    SKATE_KID_STATE_SHOOT_2 = 4,
    SKATE_KID_STATE_DEATH = 5,
} SKATE_KID_STATE;

typedef struct {
    int16_t skateboard_item_num;
} M_PRIV;

static const CREATURE_GUN m_KidGun1 = {
    .muzzle = { .pos = { 0, 150, 34 }, .mesh_num = 7 },
};
static const CREATURE_GUN m_KidGun2 = {
    .muzzle = { .pos = { 0, 150, 37 }, .mesh_num = 4 },
};

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->skateboard_item_num = NO_ITEM;

    Creature_Initialise(item_num);
    item->current_anim_state = SKATE_KID_STATE_SKATE;

    if (!Object_Get(O_SKATEBOARD)->loaded) {
        return;
    }

    const int16_t skateboard_item_num = Item_Create();
    if (skateboard_item_num == NO_ITEM) {
        LOG_WARNING("Failed to create skateboard item for skate kid.");
        return;
    }

    ITEM *const skateboard_item = Item_Get(skateboard_item_num);
    skateboard_item->object_id = O_SKATEBOARD;
    skateboard_item->pos = item->pos;
    skateboard_item->rot = item->rot;
    skateboard_item->room_num = item->room_num;
    skateboard_item->status = item->status;
    skateboard_item->collidable = false;
    skateboard_item->shade.value_1 = -1;
    Item_Initialise(skateboard_item_num);

    p->skateboard_item_num = skateboard_item_num;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (item->status == IS_INVISIBLE) {
        if (!LOT_EnableBaddieAI(item_num, 0)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    CREATURE *const kid = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != SKATE_KID_STATE_DEATH) {
            item->current_anim_state = SKATE_KID_STATE_DEATH;
            Item_SwitchToAnim(item, SKATE_KID_DIE_ANIM, 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, SKATE_KID_SKATE_TURN);

        if (item->hit_points < SKATE_KID_SPEECH_HITPOINTS
            && !(item->flags & SKATE_KID_SPEECH_STARTED)) {
            const MUSIC_PLAY_MODE mode =
                g_Config.audio.fix_speeches_killing_music ? MPM_OVERLAY
                                                          : MPM_NO_REPEAT;
            Music_Play(MX_SKATEKID_SPEECH, mode);
            item->flags |= SKATE_KID_SPEECH_STARTED;
        }

        switch (item->current_anim_state) {
        case SKATE_KID_STATE_STOP:
            kid->flags = 0;
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = SKATE_KID_STATE_SHOOT_1;
            } else {
                item->goal_anim_state = SKATE_KID_STATE_SKATE;
            }
            break;

        case SKATE_KID_STATE_SKATE:
            kid->flags = 0;
            if (Random_GetControl() < SKATE_KID_PUSH_CHANCE) {
                item->goal_anim_state = SKATE_KID_STATE_PUSH;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                if (info.distance > SKATE_KID_DONT_STOP_RANGE
                    && info.distance < SKATE_KID_STOP_RANGE
                    && kid->mood != MOOD_ESCAPE) {
                    item->goal_anim_state = SKATE_KID_STATE_STOP;
                } else {
                    item->goal_anim_state = SKATE_KID_STATE_SHOOT_2;
                }
            }
            break;

        case SKATE_KID_STATE_PUSH:
            if (Random_GetControl() < SKATE_KID_SKATE_CHANCE) {
                item->goal_anim_state = SKATE_KID_STATE_SKATE;
            }
            break;

        case SKATE_KID_STATE_SHOOT_1:
        case SKATE_KID_STATE_SHOOT_2:
            if (!kid->flags && Creature_CanTargetEnemy(item, &info)) {
                Creature_Shoot(
                    item, &info, &m_KidGun1, head,
                    item->current_anim_state == SKATE_KID_STATE_SHOOT_1
                        ? SKATE_KID_STOP_SHOT_DAMAGE
                        : SKATE_KID_SKATE_SHOT_DAMAGE);

                Creature_Shoot(
                    item, &info, &m_KidGun2, head,
                    item->current_anim_state == SKATE_KID_STATE_SHOOT_1
                        ? SKATE_KID_STOP_SHOT_DAMAGE
                        : SKATE_KID_SKATE_SHOT_DAMAGE);

                kid->flags = 1;
            }
            if (kid->mood == MOOD_ESCAPE
                || info.distance < SKATE_KID_TOO_CLOSE) {
                item->required_anim_state = SKATE_KID_STATE_SKATE;
            }
            break;
        }
    }

    Creature_Head(item, head);
    Creature_Animate(item_num, angle, 0);

    if (p->skateboard_item_num != NO_ITEM) {
        ITEM *const skateboard_item = Item_Get(p->skateboard_item_num);
        skateboard_item->pos = item->pos;
        skateboard_item->rot = item->rot;
        skateboard_item->status = item->status;
        Item_UpdateRoom(p->skateboard_item_num, item->room_num);

        const int16_t relative_anim = Item_GetRelativeAnim(item);
        const int16_t relative_frame = Item_GetRelativeFrame(item);
        Item_SwitchToObjAnim(
            skateboard_item, relative_anim, relative_frame, O_SKATEBOARD);
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = SKATE_KID_HITPOINTS;
    obj->radius = SKATE_KID_RADIUS;
    obj->smartness = SKATE_KID_SMARTNESS;
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    Object_GetBone(obj, 0)->rot.y = true;

    if (!Object_Get(O_SKATEBOARD)->loaded) {
        LOG_WARNING(
            "Skateboard object (%d) is not loaded and so will not be drawn.",
            O_SKATEBOARD);
    }
}

static void M_SetupSkateboard(OBJECT *const obj)
{
    obj->control_func = nullptr;
}

REGISTER_OBJECT(O_SKATEKID, M_Setup)
REGISTER_OBJECT(O_SKATEBOARD, M_SetupSkateboard)
