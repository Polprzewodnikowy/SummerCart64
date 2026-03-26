#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/log.h>
#include <trx/core/utils.h>
#include <trx/game/const.h>
#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

#define M_POISON_FIRE_TIMER 24

typedef enum {
    // clang-format off
    STATE_IDLE   = 0,
    STATE_FIRE   = 1,
    STATE_RELOAD = 2,
    // clang-format on
} M_STATE;

typedef struct {
    int32_t fire_timer;
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "fire_timer", &p->fire_timer));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "fire_timer", p->fire_timer);
}

static OBJECT_ID M_GetProjectileObjectID(const OBJECT_ID emitter_id)
{
    switch (emitter_id) {
    case O_DART_EMITTER:
        return O_DART;
    case O_DISC_EMITTER:
        return O_DISC;
    case O_POISON_DART_EMITTER:
        return O_POISON_DART;
    default:
        return NO_OBJECT;
    }
}

static void M_TriggerPoisonDartSmoke(
    const ITEM *const item, const int32_t x, const int32_t z)
{
    const int32_t x_limit = x != 0 ? ABS(x << 1) - 1 : 0;
    const int32_t z_limit = x == 0 ? ABS(z << 1) - 1 : 0;
    for (int32_t i = 0; i < 5; i++) {
        const int32_t rnd = -Random_GetControl();
        const XZ_32 vel = {
            .x = x >= 0 ? (x_limit & rnd) : -(x_limit & rnd),
            .z = z >= 0 ? (z_limit & rnd) : -(z_limit & rnd),
        };
        Sparks_TriggerDartSmoke(item->pos, vel, false);
    }
}

static void M_CreateProjectile(const ITEM *const item)
{
    const OBJECT_ID projectile_obj_id =
        M_GetProjectileObjectID(item->object_id);
    if (!Object_Get(projectile_obj_id)->loaded) {
        LOG_ERROR(
            "Projectile object not loaded for item #%d", Item_GetIndex(item));
        return;
    }

    const int16_t projectile_item_num = Item_Create();
    if (projectile_item_num == NO_ITEM) {
        return;
    }

    ITEM *const projectile_item = Item_Get(projectile_item_num);
    projectile_item->object_id = projectile_obj_id;
    projectile_item->room_num = item->room_num;
    projectile_item->shade.value_1 = -1;
    projectile_item->rot.y = item->rot.y;
    projectile_item->pos.y = item->pos.y - 512;

    const bool is_poison = item->object_id == O_POISON_DART_EMITTER;
    const int32_t wall_inset = is_poison ? 0 : 100;

    int32_t x = 0;
    int32_t z = 0;
    switch (projectile_item->rot.y) {
    case 0:
        z = (is_poison ? 1 : -1) * (WALL_L / 2 - wall_inset);
        break;
    case DEG_90:
        x = (is_poison ? 1 : -1) * (WALL_L / 2 - wall_inset);
        break;
    case -DEG_180:
        z = (is_poison ? -1 : 1) * (WALL_L / 2 - wall_inset);
        break;
    case -DEG_90:
        x = (is_poison ? -1 : 1) * (WALL_L / 2 - wall_inset);
        break;
    }

    projectile_item->pos.x = item->pos.x + x;
    projectile_item->pos.z = item->pos.z + z;
    Item_Initialise(projectile_item_num);
    Item_AddActive(projectile_item_num);
    projectile_item->status = IS_ACTIVE;

    if (is_poison) {
        projectile_item->rot.y += DEG_180;
        projectile_item->speed = STEP_L;
        M_TriggerPoisonDartSmoke(projectile_item, x, z);
        Sound_Effect(SFX_BLOWPIPE_BLOW, &projectile_item->pos, SPM_NORMAL);
    } else if (item->object_id == O_DART_EMITTER) {
        const int16_t effect_num = Effect_Create(projectile_item->room_num);
        if (effect_num != NO_EFFECT) {
            EFFECT *const effect = Effect_Get(effect_num);
            effect->pos = projectile_item->pos;
            effect->rot = projectile_item->rot;
            effect->speed = 0;
            effect->frame_num = 0;
            effect->counter = 0;
            effect->object_id = O_DART_EFFECT;
        }
        Sound_Effect(SFX_DART, &projectile_item->pos, SPM_NORMAL);
    } else {
        Sound_Effect(SFX_DISC, &projectile_item->pos, SPM_NORMAL);
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (Item_IsTriggerActive(item)) {
        if (item->current_anim_state == STATE_IDLE) {
            item->goal_anim_state = STATE_FIRE;
        }
    } else if (item->current_anim_state == STATE_FIRE) {
        item->goal_anim_state = STATE_IDLE;
    }

    if (item->current_anim_state == STATE_FIRE
        && Item_TestFrameEqual(item, 0)) {
        M_CreateProjectile(item);
    }

    Item_Animate(item);
}

static void M_ControlPoisonEmitter(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (!Item_IsTriggerActive(item)) {
        p->fire_timer = 0;
        return;
    }

    if (p->fire_timer > 0) {
        p->fire_timer--;
        return;
    }

    p->fire_timer = M_POISON_FIRE_TIMER;
    M_CreateProjectile(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->save_flags = true;
}

static void M_SetupPoisonEmitter(OBJECT *const obj)
{
    obj->draw_func = nullptr;
    obj->control_func = M_ControlPoisonEmitter;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_DART_EMITTER, M_Setup)
REGISTER_OBJECT(O_DISC_EMITTER, M_Setup)
REGISTER_OBJECT(O_POISON_DART_EMITTER, M_SetupPoisonEmitter)
