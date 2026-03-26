#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_RUN_TURN      (10 * DEG_1)
#define M_STOP_TURN     (3 * DEG_1)

#define M_UPSET_SPEED   15
#define M_HIT_RANGE     SQUARE(WALL_L / 3)
#define M_ATTACK_ANGLE  0x3000
#define M_JUMP_CHANCE   0x1000
#define M_ATTACK_CHANCE 0x1F
#define M_TOUCH_BITS    0x04
#define M_HIT_FLAG      1
// clang-format on

typedef enum {
    M_STATE_STOP,
    M_STATE_RUN,
    M_STATE_JUMP,
    M_STATE_ATTACK,
    M_STATE_DEATH,
} M_STATE;

typedef enum {
    M_ANIM_DEATH = 6,
} M_ANIM;

typedef struct {
    int32_t scared_timer;
    bool attack_lara;
} M_SHARED_PRIV;

typedef struct {
    bool attack_lara;
    int32_t scared_timer;
    int32_t carcass_item_num;
    M_SHARED_PRIV *shared;
} M_PRIV;

static M_SHARED_PRIV m_SharedPriv = {};
static BITE m_Bite = {
    .pos = { .x = 0, .y = 0, .z = 0 },
    .mesh_num = 2,
};

static bool M_FindCarcass(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    p->carcass_item_num = Item_FindTypeInRoom(item->room_num, O_ANIMATING_6);
    return p->carcass_item_num != NO_ITEM;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    Creature_Initialise(item_num);
    p->carcass_item_num = NO_ITEM;
    p->shared = &m_SharedPriv;
    p->shared->attack_lara = false;
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "scared_timer", &p->scared_timer));
    JSON_SHOULD(JSON_READ(io, "attack_lara", &p->attack_lara));
    JSON_SHOULD(JSON_READ(io, "shared_scared_timer", &p->shared->scared_timer));
    JSON_SHOULD(JSON_READ(io, "shared_attack_lara", &p->shared->attack_lara));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "scared_timer", p->scared_timer);
    JSONW_WRITE(io, "attack_lara", p->attack_lara);
    JSONW_WRITE(io, "shared_scared_timer", p->shared->scared_timer);
    JSONW_WRITE(io, "shared_attack_lara", p->shared->attack_lara);
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
    int16_t torso = 0;
    int16_t head = 0;

    if (p->carcass_item_num == NO_ITEM) {
        M_FindCarcass(item);
    }

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
        }
        goto finish;
    }

    AI_INFO info;
    Creature_AIInfo(item, &info);

    if (creature->mood == MOOD_BORED && p->carcass_item_num != NO_ITEM) {
        ITEM *const raptor = Item_Get(p->carcass_item_num);
        const int32_t dx = raptor->pos.x - item->pos.x;
        const int32_t dz = raptor->pos.z - item->pos.z;
        info.distance = SQUARE(dx) + SQUARE(dz);
        info.angle = Math_Atan(dz, dx) - item->rot.y;
        info.ahead = info.angle > -DEG_90 && info.angle < DEG_90;
    }

    const int16_t bits = (item_num & 7) * 0x200 - 0x700;

    ITEM *const lara_item = Lara_GetItem();
    if (p->shared->scared_timer == 0 && !p->shared->attack_lara
        && ((info.enemy_facing < M_ATTACK_ANGLE
             && info.enemy_facing > -M_ATTACK_ANGLE
             && lara_item->speed > M_UPSET_SPEED)
            || lara_item->current_anim_state == LS(LS_ROLL)
            || item->hit_status)) {
        p->scared_timer = (bits + 0x700) >> 7;
        p->shared->scared_timer = 280;
    } else if (p->shared->scared_timer > 0) {
        if (p->scared_timer > 0) {
            p->scared_timer--;
        } else {
            creature->mood = MOOD_ESCAPE;
            p->shared->scared_timer--;
        }
        if (Random_GetControl() < M_ATTACK_CHANCE && item->timer > 180) {
            p->shared->attack_lara = true;
        }
    } else if (info.zone_num == info.enemy_zone_num) {
        creature->mood = MOOD_ATTACK;
    } else {
        creature->mood = MOOD_BORED;
    }

    switch (creature->mood) {
    case MOOD_ATTACK:
        creature->target =
            XYZ_32_OffsetYaw(creature->enemy->pos, bits + item->rot.y, WALL_L);
        break;

    case MOOD_ESCAPE:
    case MOOD_STALK: {
        creature->target =
            XYZ_32_OffsetYaw(item->pos, bits + info.angle + DEG_180, WALL_L);
        int16_t room_num = item->room_num;
        SECTOR *const sector = Room_GetSector(creature->target, &room_num);
        if (ABS(Box_GetBox(sector->box)->height - item->pos.y) > STEP_L) {
            creature->mood = MOOD_BORED;
            p->scared_timer = p->shared->scared_timer;
        }
        break;
    }

    case MOOD_BORED:
        if (p->carcass_item_num != NO_ITEM) {
            ITEM *const raptor = Item_Get(p->carcass_item_num);
            creature->target.x = raptor->pos.x;
            creature->target.z = raptor->pos.z;
        }
        break;
    }

    angle = Creature_Turn(item, creature->maximum_turn);
    torso = info.ahead ? info.angle : 0;
    head = -(info.angle / 4);
    item->timer++;

    if (item->hit_status && item->timer > 200 && Random_GetControl() < 0xC1C) {
        p->shared->attack_lara = true;
    }

    switch (item->current_anim_state) {
    case M_STATE_STOP:
        creature->flags &= ~M_HIT_FLAG;
        creature->maximum_turn = M_STOP_TURN;

        if (creature->mood == MOOD_ATTACK) {
            if (info.ahead && info.distance < M_HIT_RANGE * 4) {
                if (!p->shared->attack_lara) {
                    item->goal_anim_state = M_STATE_STOP;
                } else if (Random_GetControl() < DEG_90) {
                    item->goal_anim_state = M_STATE_ATTACK;
                } else {
                    item->goal_anim_state = M_STATE_JUMP;
                }
            } else if (
                info.distance
                > M_HIT_RANGE * (9 - 4 * p->shared->attack_lara)) {
                item->goal_anim_state = M_STATE_RUN;
            }
        } else if (creature->mood == MOOD_BORED) {
            if (info.ahead && info.distance < M_HIT_RANGE * 3
                && p->carcass_item_num != NO_ITEM) {
                if (Random_GetControl() < DEG_90) {
                    item->goal_anim_state = M_STATE_ATTACK;
                } else {
                    item->goal_anim_state = M_STATE_JUMP;
                }
            } else if (info.distance > M_HIT_RANGE * 3) {
                item->goal_anim_state = M_STATE_RUN;
            }
        } else if (Random_GetControl() < M_JUMP_CHANCE) {
            item->goal_anim_state = M_STATE_JUMP;
        } else {
            item->goal_anim_state = M_STATE_RUN;
        }
        break;

    case M_STATE_RUN:
        creature->flags &= ~M_HIT_FLAG;
        creature->maximum_turn = M_RUN_TURN;
        if (info.angle < M_ATTACK_ANGLE && info.angle > -M_ATTACK_ANGLE
            && info.distance < M_HIT_RANGE * (9 - 4 * p->shared->attack_lara)) {
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_JUMP:
    case M_STATE_ATTACK:
        creature->maximum_turn = M_RUN_TURN;
        if (!(creature->flags & M_HIT_FLAG)) {
            if ((item->touch_bits & M_TOUCH_BITS) && p->shared->attack_lara) {
                creature->flags |= M_HIT_FLAG;
                Lara_TakeDamage(90, true);
                Creature_Effect(item, &m_Bite, Spawn_Blood);
            } else if (
                info.distance < M_HIT_RANGE && info.ahead
                && p->carcass_item_num != NO_ITEM
                && creature->mood != MOOD_ATTACK) {
                creature->flags |= M_HIT_FLAG;
                Creature_Effect(item, &m_Bite, Spawn_Blood);
            }
        }
        break;
    }

finish:
    Creature_Tilt(item, angle >> 1);
    Creature_Joint(item, 0, torso);
    Creature_Joint(item, 1, head);
    Creature_Animate(item_num, angle, angle >> 1);
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
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->radius = 102;
    obj->shadow_size = 85;
    obj->hit_points = 10;
    obj->pivot_length = 50;

    // obj->non_lot = true; // TODO(TR3)
    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 1)->rot.y = true;
    Object_GetBone(obj, 2)->rot.y = true;
}

REGISTER_OBJECT(O_COMPY, M_Setup)
