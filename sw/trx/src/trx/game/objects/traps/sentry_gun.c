#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/creature.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>

typedef enum {
    M_STATE_FIRE,
    M_STATE_STILL,
} M_STATE;

typedef enum {
    M_MUZZLE_LEFT,
    M_MUZZLE_RIGHT,
} M_MUZZLE;

typedef struct {
    int16_t active_muzzle;
    int16_t muzzle_flash_timer;
    bool is_alerted;
    bool has_fired;
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_OPTIONAL(JSON_READ(io, "active_muzzle", &p->active_muzzle));
    JSON_OPTIONAL(JSON_READ(io, "muzzle_flash_timer", &p->muzzle_flash_timer));
    JSON_OPTIONAL(JSON_READ(io, "is_alerted", &p->is_alerted));
    JSON_OPTIONAL(JSON_READ(io, "has_fired", &p->has_fired));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "active_muzzle", p->active_muzzle);
    JSONW_WRITE(io, "muzzle_flash_timer", p->muzzle_flash_timer);
    JSONW_WRITE(io, "is_alerted", p->is_alerted);
    JSONW_WRITE(io, "has_fired", p->has_fired);
}

static const CREATURE_GUN m_FireLeft = {
    .muzzle = {
        .pos = { .x = 110, .y = -30, .z = -530 },
        .mesh_num = 2,
    },
    .tr3_enemy_flash = true,
    .tr3_flash = {
        .pos = { .x = 110, .y = -30, .z = -530 },
        .mesh_num = 2,
    },
    .tr3_enemy_weapon_flags = 1,
    .tr3_flash_shade = 600,
    .tr3_flash_rot_x = -DEG_180,
};

static const CREATURE_GUN m_FireRight = {
    .muzzle = {
        .pos = { .x = -110, .y = -30, .z = -530 },
        .mesh_num = 2,
    },
    .tr3_enemy_flash = true,
    .tr3_flash = {
        .pos = { .x = -110, .y = -30, .z = -530 },
        .mesh_num = 2,
    },
    .tr3_enemy_weapon_flags = 1,
    .tr3_flash_shade = 600,
    .tr3_flash_rot_x = -DEG_180,
};

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    Item_SwitchToAnim(item, 1, 0);
    item->current_anim_state = M_STATE_STILL;
    item->goal_anim_state = M_STATE_STILL;
    p->active_muzzle = M_MUZZLE_LEFT;
    p->muzzle_flash_timer = 0;
    p->is_alerted = false;
    p->has_fired = false;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (p->muzzle_flash_timer > 1) {
        p->muzzle_flash_timer--;

        XYZ_32 pos;
        Matrix_Push();
        if ((Random_GetControl() & 1) != 0) {
            p->active_muzzle = M_MUZZLE_LEFT;
            pos = m_FireLeft.muzzle.pos;
            Collide_GetJointAbsPosition(item, &pos, m_FireLeft.muzzle.mesh_num);
        } else {
            p->active_muzzle = M_MUZZLE_RIGHT;
            pos = m_FireRight.muzzle.pos;
            Collide_GetJointAbsPosition(
                item, &pos, m_FireRight.muzzle.mesh_num);
        }

        const RGB_888 color = { 192, 128, 32 };
        Output_AddDynamicLightRGB(pos, 2 * p->muzzle_flash_timer + 8, color);
        Matrix_Pop();
    }

    if (!Creature_Activate(item_num)) {
        return;
    }

    CREATURE *const creature = item->creature_data;
    if (creature == nullptr) {
        return;
    }

    if (item->hit_status) {
        p->is_alerted = true;
    }

    if (item->hit_points <= 0) {
        Item_Explode(item_num, -1, 0);
        LOT_DisableBaddieAI(item_num);
        Item_Kill(item_num);
        item->flags |= IF_INVISIBLE;
        item->status = IS_DEACTIVATED;
    }

    if (!p->is_alerted) {
        return;
    }

    AI_INFO info;
    Creature_AIInfo(item, &info);

    const int16_t tilt = -info.x_angle;

    switch (item->current_anim_state) {
    case M_STATE_FIRE:
        if (!Creature_CanTargetEnemy(item, &info)) {
            item->goal_anim_state = M_STATE_STILL;
        } else if (Item_GetRelativeFrame(item) == 0) {
            p->has_fired = true;

            if (p->active_muzzle == M_MUZZLE_RIGHT) {
                Creature_Shoot(
                    item, &info, &m_FireLeft, creature->joint_rotation[0], 10);
            } else {
                Creature_Shoot(
                    item, &info, &m_FireRight, creature->joint_rotation[0], 10);
            }

            p->muzzle_flash_timer = 10;
            Sound_Effect(SFX_LARA_UZI_STOP, &item->pos, SPM_NORMAL);
        }
        break;

    case M_STATE_STILL:
        if (Creature_CanTargetEnemy(item, &info) && !p->has_fired) {
            item->goal_anim_state = M_STATE_FIRE;
        } else if (p->has_fired) {
            if (item->ai_bits == AI_MODIFY) {
                item->goal_anim_state = M_STATE_FIRE;
            } else {
                p->is_alerted = false;
                p->has_fired = false;
            }
        }
        break;
    }

    int16_t diff = info.angle - creature->joint_rotation[0];
    CLAMP(diff, -DEG_1 * 10, DEG_1 * 10);

    creature->joint_rotation[0] += diff;
    Creature_Joint(item, 1, tilt);
    Item_Animate(item);

    if (info.angle > 0x4000) {
        item->rot.y += 0x8000;
        if (info.angle > 0 || info.angle < 0) {
            creature->joint_rotation[0] += 0x8000;
        }
    } else if (info.angle < -0x4000) {
        item->rot.y += 0x8000;
        if (info.angle > 0 || info.angle < 0) {
            creature->joint_rotation[0] += 0x8000;
        }
    }
}

static void M_HandleEvent(
    ITEM *const item, const OBJECT_EVENT event, const void *const data)
{
    M_PRIV *const p = item->priv;
    if (event != OBJECT_EVENT_ALERT) {
        return;
    }

    p->is_alerted = true;
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->event_func = M_HandleEvent;

    obj->shadow_size = 0;
    obj->hit_points = 100;
    obj->radius = 102;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 0)->rot.y = true;
    Object_GetBone(obj, 1)->rot.x = true;
}

REGISTER_OBJECT(O_SENTRY_GUN, M_Setup)
