#include <trx/config.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/traps/common.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

// clang-format off
#define M_AXE_DAMAGE      100
#define M_PENDULUM_DAMAGE 50
#define M_MAX_FIRE_DIST   (WALL_L * 16) // = 16384
#define M_FIRE_FALLOFF    11
// clang-format on

typedef struct {
    bool initialised;
    bool on_fire;
    int16_t effect_num;
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "initialised", &p->initialised));
    JSON_SHOULD(JSON_READ(io, "on_fire", &p->on_fire));
    if (g_Config.gameplay.enable_enhanced_saves) {
        JSON_SHOULD(JSON_READ(io, "fx_num", &p->effect_num));
    }
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "initialised", p->initialised);
    JSONW_WRITE(io, "on_fire", p->on_fire);
    JSONW_WRITE(io, "fx_num", Effect_GetInOrderNum(p->effect_num));
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->effect_num = NO_EFFECT;
}

static void M_InitialiseFire(ITEM *const pendulum_item)
{
    M_PRIV *const p = pendulum_item->priv;
    const OBJECT_ID fire_obj_id =
        g_TRVersion < 3 ? O_FLAME_EMITTER : O_FLAME_EMITTER_BIG;
    const int16_t fire_item_idx = Item_FindTypeAtPos(
        pendulum_item->room_num, pendulum_item->pos, fire_obj_id);
    if (fire_item_idx != NO_ITEM) {
        ITEM *const fire_item = Item_Get(fire_item_idx);
        Item_Kill(fire_item_idx);
        fire_item->room_num = NO_ROOM;
        p->on_fire = true;
    }

    p->initialised = true;
}

static void M_TriggerFireSparks(const ITEM *const item)
{
    const ITEM *const lara_item = Lara_GetItem();
    const XZ_32 delta = {
        .x = lara_item->pos.x - item->pos.x,
        .z = lara_item->pos.z - item->pos.z,
    };
    if (ABS(delta.x) > M_MAX_FIRE_DIST || ABS(delta.z) > M_MAX_FIRE_DIST) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = (Random_GetControl() & 0x1F) + 48;
    spark->src_color.g = spark->src_color.r >> 1;
    spark->src_color.b = 0;
    spark->dst_color.r = (Random_GetControl() & 0x3F) + 192;
    spark->dst_color.g = (Random_GetControl() & 0x3F) + 128;
    spark->dst_color.b = 32;

    spark->col_fade_speed = (Random_GetControl() & 3) + 12;
    spark->fade_to_black = 8;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->life = (Random_GetControl() & 7) + 28;
    spark->s_life = spark->life;
    spark->dynamic = -1;
    spark->pos.x = (Random_GetControl() & 0x1F) - 16;
    spark->pos.y = 0;
    spark->pos.z = (Random_GetControl() & 0x1F) - 16;
    spark->vel.x = (Random_GetControl() & 0x3F) - 32;
    spark->vel.y = -16 - (Random_GetControl() & 0xF);
    spark->vel.z = (Random_GetControl() & 0x3F) - 32;
    spark->friction = 4;
    spark->flags = SPARK_F_ATTACHED_NODE | SPARK_F_ALT_SPRITE | SPARK_F_ITEM
        | SPARK_F_SPRITE | SPARK_F_SCALE;

    if ((Random_GetControl() & 1) != 0) {
        spark->flags |= SPARK_F_ROTATE;
        spark->rot_angle = Random_GetControl() & 0xFFF;
        spark->rot_add = (Random_GetControl() & 0x1F) - 16;
    }

    spark->node_num = 3;
    spark->item_num = Item_GetIndex(item);
    spark->gravity = -16 - (Random_GetControl() & 0x1F);
    spark->max_y_vel = -16 - (Random_GetControl() & 7);
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->scalar = 3;
    spark->size.width = (Random_GetControl() & 7) + 32;
    spark->src_size.width = spark->size.width;
    spark->dst_size.width = spark->size.width >> 2;
    spark->size.height = spark->size.width;
    spark->src_size.height = spark->size.height;
    spark->dst_size.height = spark->size.height >> 2;
    Sparks_FinishSetup(spark);
}

static void M_TriggerFireLight(const ITEM *const item)
{
    XYZ_32 pos = { 0, -STEP_L * 2, 0 };
    Collide_GetJointAbsPosition(item, &pos, 5);
    const RGB_888 color = {
        .r = (Random_GetControl() & 0x3F) + 192,
        .g = (Random_GetControl() & 0x1F) + 96,
        .b = 0,
    };
    Output_AddDynamicLightRGB(pos, M_FIRE_FALLOFF, color);
}

static void M_TriggerFireEffect(const ITEM *const item)
{
    M_PRIV *const p = item->priv;
    EFFECT *effect = nullptr;
    if (p->effect_num == NO_EFFECT) {
        p->effect_num = Effect_Create(item->room_num);
        if (p->effect_num == NO_EFFECT) {
            return;
        }
        effect = Effect_Get(p->effect_num);
        effect->object_id = O_FLAME;
        effect->counter = 0;
        effect->frame_num = 0;
    } else {
        effect = Effect_Get(p->effect_num);
    }

    XYZ_32 pos = { -32, -STEP_L - 16, 0 };
    Collide_GetJointAbsPosition(item, &pos, 5);
    effect->pos = pos;
}

static void M_KillFireEffect(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    if (p->effect_num == NO_EFFECT) {
        return;
    }

    Effect_Kill(p->effect_num);
    p->effect_num = NO_EFFECT;
    if (g_TRVersion == 1) {
        Sound_StopEffect(SFX_LOOP_FOR_SMALL_FIRES);
    }
}

static inline int16_t M_GetDamage(const OBJECT_ID obj_id)
{
    return obj_id == O_SWINGING_AXE ? M_AXE_DAMAGE : M_PENDULUM_DAMAGE;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    item->enable_interpolation = true;

    if (!p->initialised) {
        M_InitialiseFire(item);
    }

    const OBJECT *const obj = Object_Get(item->object_id);
    const ANIM *const base_anim = Object_GetAnim(obj, 0);

    bool working;
    if (Anim_HasChange(base_anim, TRAP_WORKING)) {
        working = item->current_anim_state == TRAP_WORKING;
        if (Item_IsTriggerActive(item)) {
            if (item->current_anim_state == TRAP_SET) {
                item->goal_anim_state = TRAP_WORKING;
            }
        } else {
            if (item->current_anim_state == TRAP_WORKING) {
                item->goal_anim_state = TRAP_SET;
            }
        }
    } else {
        working = true;
        if (!Item_IsTriggerActive(item) && Item_TestFrameEqual(item, -1)) {
            Item_SwitchToAnim(item, 0, 0);
            item->status = IS_INACTIVE;
            Item_RemoveActive(item_num);
            item->enable_interpolation = false;
            M_KillFireEffect(item);
            return;
        }
    }

    if (working && item->touch_bits != 0) {
        const int16_t damage = M_GetDamage(item->object_id);
        Lara_TakeDamage(damage, true);

        if (p->on_fire) {
            Lara_CatchFire();
        } else {
            const ITEM *const lara_item = Lara_GetItem();
            const XYZ_32 pos = {
                .x = lara_item->pos.x + (Random_GetControl() - 0x4000) / 256,
                .z = lara_item->pos.z + (Random_GetControl() - 0x4000) / 256,
                .y = lara_item->pos.y - Random_GetControl() / 44,
            };
            Spawn_Blood(
                pos.x, pos.y, pos.z, lara_item->speed,
                lara_item->rot.y + (Random_GetControl() - 0x4000) / 8,
                lara_item->room_num);
        }
    }

    const SECTOR *const sector = Room_GetSector(item->pos, &item->room_num);
    item->floor = Room_GetHeight(sector, item->pos);

    if (p->on_fire) {
        if (g_TRVersion >= 3) {
            M_TriggerFireSparks(item);
            M_TriggerFireLight(item);
        } else {
            M_TriggerFireEffect(item);
        }
    }

    Item_Animate(item);
}

static void M_HandleSave(ITEM *const item, const SAVEGAME_STAGE stage)
{
    if (stage == SAVEGAME_STAGE_AFTER_LOAD) {
        item->enable_interpolation = item->status == IS_ACTIVE;
    }
}

static void M_SetupCommon(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->handle_save_func = M_HandleSave;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->save_flags = true;
    obj->save_anim = true;
}

static void M_SetupAxe(OBJECT *const obj)
{
    M_SetupCommon(obj);
    obj->collision_func = Object_Collision_Trap;
}

static void M_SetupPendulum(OBJECT *const obj)
{
    M_SetupCommon(obj);
    obj->collision_func = Object_Collision;
}

REGISTER_OBJECT(O_SWINGING_AXE, M_SetupAxe)
REGISTER_OBJECT(O_PENDULUM_1, M_SetupPendulum)
REGISTER_OBJECT(O_PENDULUM_2, M_SetupPendulum)
