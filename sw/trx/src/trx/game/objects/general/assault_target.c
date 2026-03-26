#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/gym.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/pathing/lot.h>
#include <trx/game/sound.h>
#include <trx/version.h>

typedef enum {
    M_STATE_RISE = 0,
    M_STATE_HIT_1 = 1,
    M_STATE_HIT_2 = 2,
    M_STATE_HIT_3 = 3,
} M_STATE;

typedef struct {
    int32_t x_rot_speed;
    int32_t bounce_stage;
    bool destroyed;
    bool targetable;
} M_PRIV;

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    return false;
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "x_rot_speed", &p->x_rot_speed));
    JSON_SHOULD(JSON_READ(io, "bounce_stage", &p->bounce_stage));
    JSON_SHOULD(JSON_READ(io, "destroyed", &p->destroyed));
    p->targetable = !p->destroyed;
    JSON_OPTIONAL(JSON_READ(io, "targetable", &p->targetable));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "x_rot_speed", p->x_rot_speed);
    JSONW_WRITE(io, "bounce_stage", p->bounce_stage);
    JSONW_WRITE(io, "destroyed", p->destroyed);
    JSONW_WRITE(io, "targetable", p->targetable);
}

static void M_ResetItemState(ITEM *const item, const OBJECT *const obj)
{
    Item_SwitchToAnim(item, 0, 0);
    const ANIM *const anim = Item_GetAnim(item);
    item->current_anim_state = anim->current_anim_state;
    item->goal_anim_state = item->current_anim_state;
    item->required_anim_state = M_STATE_RISE;
    item->prev_frame_num = item->frame_num;
    item->rot.x = 0;
    item->rot.z = 0;
    item->timer = 0;
    item->hit_points = obj->hit_points;
    item->max_hit_points = obj->hit_points;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const OBJECT *const obj = Object_Get(item->object_id);

    if (item->active) {
        Item_RemoveActive(item_num);
    }

    if (item->creature_data != nullptr) {
        LOT_DisableBaddieAI(item_num);
    }

    M_PRIV *const p = item->priv;
    p->x_rot_speed = 0;
    p->bounce_stage = 0;
    p->destroyed = false;
    p->targetable = true;

    item->active = false;
    item->status = IS_INACTIVE;
    item->flags = 0;
    item->collidable = true;

    M_ResetItemState(item, obj);
}

static void M_Control(const int16_t item_num)
{
    if (g_TRVersion < 3) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    if (item->status != IS_ACTIVE) {
        return;
    }

    M_PRIV *const p = item->priv;
    if (p == nullptr) {
        return;
    }

    if (p->targetable) {
        if (item->hit_status) {
            Sound_Effect(SFX_TARGET_HITS, &item->pos, SPM_NORMAL);
        }

        switch (item->current_anim_state) {
        case M_STATE_RISE:
            if (item->hit_points < 6) {
                item->hit_points = 6;
                item->goal_anim_state = M_STATE_HIT_1;
            }
            break;

        case M_STATE_HIT_1:
            if (item->hit_points < 4) {
                Item_SwitchToAnim(item, 2, 0);
                item->current_anim_state = M_STATE_HIT_2;
                item->goal_anim_state = M_STATE_HIT_2;
                item->hit_points = 4;
            }
            break;

        case M_STATE_HIT_2:
            if (item->hit_points < 2) {
                Item_SwitchToAnim(item, 3, 0);
                item->current_anim_state = M_STATE_HIT_3;
                item->goal_anim_state = M_STATE_HIT_3;
                item->hit_points = 2;
            }
            break;

        case M_STATE_HIT_3:
            if (item->hit_points <= 0) {
                LARA_INFO *const lara = Lara_GetLaraInfo();
                if (lara->target == item) {
                    lara->target = nullptr;
                }
                p->targetable = false;
                p->x_rot_speed = DEG_1 * 10;
                p->bounce_stage = 0;
                p->destroyed = true;
            }
            break;
        }

        item->timer++;

        if (item->timer > GYM_ASSAULT_TARGET_TIME) {
            LARA_INFO *const lara = Lara_GetLaraInfo();
            if (lara->target == item) {
                lara->target = nullptr;
            }
            p->targetable = false;
            p->x_rot_speed = DEG_1;
            p->bounce_stage = 0;
            p->destroyed = false;
        }
    } else {
        if (p->destroyed) {
            int32_t rot_x = item->rot.x;
            rot_x += p->x_rot_speed;
            p->x_rot_speed += (DEG_1 * 4) >> p->bounce_stage;

            if (rot_x > 0x3800) {
                if (p->bounce_stage == 2) {
                    item->rot.x = 0x3800;
                    Item_RemoveActive(item_num);
                    return;
                }

                if (p->bounce_stage == 1) {
                    Sound_Effect(SFX_TARGET_SMASH, &item->pos, SPM_NORMAL);
                }

                p->x_rot_speed = (-p->x_rot_speed) >> 2;
                p->bounce_stage++;
                rot_x = 0x3800;
            }

            item->rot.x = rot_x;
        } else {
            int32_t rot_x = item->rot.x;
            rot_x -= p->x_rot_speed;
            p->x_rot_speed += 91 >> p->bounce_stage;

            if (rot_x < -0x2A00) {
                if (p->bounce_stage == 2) {
                    item->rot.x = -0x2A00;
                    Item_RemoveActive(item_num);
                    return;
                }

                Sound_Effect(
                    SFX_TARGET_HITS, &item->pos, SPM_PITCH | (0x20000 << 8));

                p->x_rot_speed = (-p->x_rot_speed) >> 2;
                p->bounce_stage++;
                rot_x = -0x2A00;
            }

            item->rot.x = (int16_t)rot_x;
        }
    }

    Item_Animate(item);
}

static bool M_IsTargetable(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p != nullptr && p->targetable && item->status == IS_ACTIVE
        && item->hit_points > 0;
}

static bool M_CanTakeDamage(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p != nullptr && p->targetable && item->hit_points > 0;
}

static bool M_CanBeProjectileTarget(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p != nullptr && p->targetable && item->status == IS_ACTIVE
        && item->collidable && item->hit_points > 0;
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;
    obj->is_targetable_func = M_IsTargetable;
    obj->can_take_damage_func = M_CanTakeDamage;
    obj->can_be_projectile_target_func = M_CanBeProjectileTarget;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->hit_points = 8;
    obj->shadow_size = 128;
    obj->radius = 102;
    obj->intelligent = false;
}

REGISTER_OBJECT(O_ASSAULT_TARGET, M_Setup)
