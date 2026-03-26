#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/collision/los.h>
#include <trx/game/creature.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>

#define PIERRE_POSE_CHANCE 0x60 // = 96
#define PIERRE_SHOT_DAMAGE 50
#define PIERRE_WALK_TURN (DEG_1 * 3) // = 546
#define PIERRE_RUN_TURN (DEG_1 * 6) // = 1092
#define PIERRE_WALK_RANGE SQUARE(WALL_L * 3) // = 9437184
#define PIERRE_DIE_ANIM 12
#define PIERRE_WIMP_CHANCE 0x2000
#define PIERRE_RUN_HITPOINTS 40
#define PIERRE_DISAPPEAR 10
#define PIERRE_HITPOINTS 70
#define PIERRE_RADIUS (WALL_L / 10) // = 102
#define PIERRE_SMARTNESS 0x7FFF

typedef enum {
    PIERRE_STATE_EMPTY = 0,
    PIERRE_STATE_STOP = 1,
    PIERRE_STATE_WALK = 2,
    PIERRE_STATE_RUN = 3,
    PIERRE_STATE_AIM = 4,
    PIERRE_STATE_DEATH = 5,
    PIERRE_STATE_POSE = 6,
    PIERRE_STATE_SHOOT = 7,
} PIERRE_STATE;

static const CREATURE_GUN m_PierreGun1 = {
    .muzzle = { .pos = { 60, 200, 0 }, .mesh_num = 11 },
};
static const CREATURE_GUN m_PierreGun2 = {
    .muzzle = { .pos = { -57, 200, 0 }, .mesh_num = 14 },
};
static int16_t m_PierreItemNum = NO_ITEM;

static bool M_CanDropItems(const ITEM *const item)
{
    return item->hit_points <= 0 && (item->flags & IF_ONE_SHOT) != 0;
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        if (item->hit_points <= 0 && (item->flags & IF_ONE_SHOT)) {
            const uint16_t flags =
                Music_GetTrackFlags(Music_ToGameID(MX_PIERRE_SPEECH));
            Music_SetTrackFlags(
                Music_ToGameID(MX_PIERRE_SPEECH), flags | IF_ONE_SHOT);
        }
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (g_Config.gameplay.change_pierre_spawn) {
        if (m_PierreItemNum == NO_ITEM) {
            m_PierreItemNum = item_num;
        } else if (m_PierreItemNum != item_num) {
            ITEM *old_pierre = Item_Get(m_PierreItemNum);
            if (old_pierre->flags & IF_ONE_SHOT) {
                if (!(item->flags & IF_ONE_SHOT)) {
                    Item_Kill(item_num);
                }
            } else {
                Item_Kill(m_PierreItemNum);
                m_PierreItemNum = item_num;
            }
        }
    } else {
        if (m_PierreItemNum == NO_ITEM) {
            m_PierreItemNum = item_num;
        } else if (m_PierreItemNum != item_num) {
            if (item->flags & IF_ONE_SHOT) {
                Item_Kill(m_PierreItemNum);
            } else {
                Item_Kill(item_num);
            }
        }
    }

    if (item->status == IS_INVISIBLE) {
        if (!LOT_EnableBaddieAI(item_num, 0)) {
            return;
        }
        item->status = IS_ACTIVE;
    }

    CREATURE *const pierre = item->creature_data;
    int16_t head = 0;
    int16_t angle = 0;
    int16_t tilt = 0;

    if (item->hit_points <= PIERRE_RUN_HITPOINTS
        && !(item->flags & IF_ONE_SHOT)) {
        item->hit_points = PIERRE_RUN_HITPOINTS;
        pierre->flags++;
    }

    if (item->hit_points <= 0) {
        if (item->current_anim_state != PIERRE_STATE_DEATH) {
            item->current_anim_state = PIERRE_STATE_DEATH;
            Item_SwitchToAnim(item, PIERRE_DIE_ANIM, 0);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (info.ahead) {
            head = info.angle;
        }

        if (pierre->flags) {
            info.enemy_zone_num = -1;
            item->hit_status = true;
        }
        Creature_Mood(item, &info, false);

        angle = Creature_Turn(item, pierre->maximum_turn);

        switch (item->current_anim_state) {
        case PIERRE_STATE_STOP:
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (pierre->mood == MOOD_BORED) {
                item->goal_anim_state = Random_GetControl() < PIERRE_POSE_CHANCE
                    ? PIERRE_STATE_POSE
                    : PIERRE_STATE_WALK;
            } else if (pierre->mood == MOOD_ESCAPE) {
                item->goal_anim_state = PIERRE_STATE_RUN;
            } else {
                item->goal_anim_state = PIERRE_STATE_WALK;
            }
            break;

        case PIERRE_STATE_POSE:
            if (pierre->mood != MOOD_BORED) {
                item->goal_anim_state = PIERRE_STATE_STOP;
            } else if (Random_GetControl() < PIERRE_POSE_CHANCE) {
                item->required_anim_state = PIERRE_STATE_WALK;
                item->goal_anim_state = PIERRE_STATE_STOP;
            }
            break;

        case PIERRE_STATE_WALK:
            pierre->maximum_turn = PIERRE_WALK_TURN;
            if (pierre->mood == MOOD_BORED
                && Random_GetControl() < PIERRE_POSE_CHANCE) {
                item->required_anim_state = PIERRE_STATE_POSE;
                item->goal_anim_state = PIERRE_STATE_STOP;
            } else if (pierre->mood == MOOD_ESCAPE) {
                item->required_anim_state = PIERRE_STATE_RUN;
                item->goal_anim_state = PIERRE_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->required_anim_state = PIERRE_STATE_AIM;
                item->goal_anim_state = PIERRE_STATE_STOP;
            } else if (!info.ahead || info.distance > PIERRE_WALK_RANGE) {
                item->required_anim_state = PIERRE_STATE_RUN;
                item->goal_anim_state = PIERRE_STATE_STOP;
            }
            break;

        case PIERRE_STATE_RUN:
            pierre->maximum_turn = PIERRE_RUN_TURN;
            tilt = angle / 2;
            if (pierre->mood == MOOD_BORED
                && Random_GetControl() < PIERRE_POSE_CHANCE) {
                item->required_anim_state = PIERRE_STATE_POSE;
                item->goal_anim_state = PIERRE_STATE_STOP;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->required_anim_state = PIERRE_STATE_AIM;
                item->goal_anim_state = PIERRE_STATE_STOP;
            } else if (info.ahead && info.distance < PIERRE_WALK_RANGE) {
                item->required_anim_state = PIERRE_STATE_WALK;
                item->goal_anim_state = PIERRE_STATE_STOP;
            }
            break;

        case PIERRE_STATE_AIM:
            if (item->required_anim_state) {
                item->goal_anim_state = item->required_anim_state;
            } else if (Creature_CanTargetEnemy(item, &info)) {
                item->goal_anim_state = PIERRE_STATE_SHOOT;
            } else {
                item->goal_anim_state = PIERRE_STATE_STOP;
            }
            break;

        case PIERRE_STATE_SHOOT:
            if (!item->required_anim_state) {
                Creature_Shoot(
                    item, &info, &m_PierreGun1, head, PIERRE_SHOT_DAMAGE / 2);
                Creature_Shoot(
                    item, &info, &m_PierreGun2, head, PIERRE_SHOT_DAMAGE / 2);
                item->required_anim_state = PIERRE_STATE_AIM;
            }
            if (pierre->mood == MOOD_ESCAPE
                && Random_GetControl() > PIERRE_WIMP_CHANCE) {
                item->required_anim_state = PIERRE_STATE_STOP;
            }
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Head(item, head);
    Creature_Animate(item_num, angle, 0);

    if (pierre->flags) {
        GAME_VECTOR target;
        target.x = item->pos.x;
        target.y = item->pos.y - WALL_L;
        target.z = item->pos.z;

        GAME_VECTOR start;
        start.x = g_Camera.pos.x;
        start.y = g_Camera.pos.y;
        start.z = g_Camera.pos.z;
        start.room_num = g_Camera.pos.room_num;

        if (LOS_Check(&start, &target, true)) {
            pierre->flags = 1;
        } else if (pierre->flags > PIERRE_DISAPPEAR) {
            item->hit_points = 0;
            LOT_DisableBaddieAI(item_num);
            Item_Kill(item_num);
            m_PierreItemNum = NO_ITEM;
        }
    }

    int16_t wh = Room_GetWaterHeight(item->pos, item->room_num);
    if (wh != NO_HEIGHT) {
        item->hit_points = 0;
        LOT_DisableBaddieAI(item_num);
        Item_Kill(item_num);
        m_PierreItemNum = NO_ITEM;
    }
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
    obj->can_drop_items_func = M_CanDropItems;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = PIERRE_HITPOINTS;
    obj->radius = PIERRE_RADIUS;
    obj->smartness = PIERRE_SMARTNESS;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_anim = true;
    obj->save_flags = true;

    m_PierreItemNum = NO_ITEM;

    Object_GetBone(obj, 6)->rot.y = true;
}

REGISTER_OBJECT(O_PIERRE, M_Setup)
