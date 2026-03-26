#include <trx/game/objects/vehicles/mounted_gun.h>

#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/math.h>
#include <trx/game/camera.h>
#include <trx/game/gun.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

// clang-format off
#define M_MOUNT_RADIUS_SQ 30000
#define M_NEUTRAL_TILT    30
#define M_MAX_TILT        (2 * M_NEUTRAL_TILT - 1) // = 59
#define M_MAX_ROT         544
#define M_MIN_ROT         (-M_MAX_ROT) // = -544
#define M_ROT_SPEED       8
#define M_MAX_ROT_SPEED   64
#define M_MIN_ROT_SPEED   (-M_MAX_ROT_SPEED) // = -64
#define M_ROT_SCALE       45
#define M_FIRE_COOLDOWN   26
#define M_CAM_ELEVATION   (DEG_1 * -15) // = -2730
// clang-format on

typedef enum {
    M_GUN_STATE_IDLE,
    M_GUN_STATE_CONTROL,
    M_GUN_STATE_DISMOUNT,
    M_GUN_STATE_WAIT_END,
} M_GUN_STATE;

typedef struct {
    M_GUN_STATE state;
    int32_t fire_count;
    int16_t tilt;
    int16_t yaw;
    int16_t yaw_offset;
    int32_t yaw_speed;
} M_PRIV;

typedef enum {
    M_STATE_MOUNT,
    M_STATE_DISMOUNT,
    M_STATE_TILT,
} M_STATE;

typedef enum {
    M_ANIM_MOUNT,
    M_ANIM_DISMOUNT,
    M_ANIM_TILT,
} M_ANIM;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "state", &p->state));
    JSON_SHOULD(JSON_READ(io, "fire_count", &p->fire_count));
    JSON_SHOULD(JSON_READ(io, "tilt", &p->tilt));
    JSON_SHOULD(JSON_READ(io, "yaw", &p->yaw));
    JSON_SHOULD(JSON_READ(io, "yaw_offset", &p->yaw_offset));
    JSON_SHOULD(JSON_READ(io, "yaw_speed", &p->yaw_speed));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "state", p->state);
    JSONW_WRITE(io, "fire_count", p->fire_count);
    JSONW_WRITE(io, "tilt", p->tilt);
    JSONW_WRITE(io, "yaw", p->yaw);
    JSONW_WRITE(io, "yaw_offset", p->yaw_offset);
    JSONW_WRITE(io, "yaw_speed", p->yaw_speed);
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->tilt = M_NEUTRAL_TILT;
    p->yaw_offset = item->rot.y;
}

static bool M_Mount(const int16_t item_num)
{
    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!g_Input.action || lara->gun_status != LGS_ARMLESS
        || lara_item->gravity) {
        return false;
    }

    const ITEM *const item = Item_Get(item_num);
    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    const int32_t dist = SQUARE(dx) + SQUARE(dz);
    if (dist > M_MOUNT_RADIUS_SQ) {
        return false;
    }

    return true;
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    if (!M_Mount(item_num)) {
        Object_Collision(item_num, lara_item, coll);
        return;
    }

    Lara_Vehicle_SetIndex(item_num);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->gun_type == LGT_FLARE) {
        Lara_Flare_Dispose(false);
        lara->gun_type = LGT_UNARMED;
        lara->request_gun_type = LGT_UNARMED;
    }

    const ITEM *const item = Item_Get(item_num);
    lara->gun_status = LGS_HANDS_BUSY;
    lara_item->pos = item->pos;
    lara_item->rot = item->rot;
    Item_SwitchToObjAnim(lara_item, M_ANIM_MOUNT, 0, O_LARA_VEHICLE_ANIM);
    lara_item->current_anim_state = M_STATE_MOUNT;
    lara_item->goal_anim_state = M_STATE_MOUNT;

    M_PRIV *const p = item->priv;
    p->state = M_GUN_STATE_IDLE;
    p->tilt = M_NEUTRAL_TILT;
}

static void M_Fire(ITEM *const gun_item)
{
    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    ITEM *const projectile_item = Item_Get(item_num);
    projectile_item->object_id = O_HEAVY_ROCKET;
    projectile_item->room_num = lara_item->room_num;

    XYZ_32 offset = {
        .x = 0,
        .y = 0,
        .z = STEP_L,
    };
    Collide_GetJointAbsPosition(gun_item, &offset, 2);
    projectile_item->pos = offset;
    projectile_item->interp.prev.pos = projectile_item->pos;
    Item_Initialise(item_num);

    M_PRIV *const p = gun_item->priv;
    projectile_item->rot.x = DEG_1 * (32 - p->tilt);
    projectile_item->rot.y = gun_item->rot.y;
    projectile_item->rot.z = 0;

    projectile_item->speed = 16;
    Item_AddActive(item_num);
    projectile_item->status = IS_ACTIVE;

    Sound_Effect(SFX_ROCKET_FIRE, &projectile_item->pos, SPM_NORMAL);
    if (g_TRVersion >= 3) {
        Sound_Effect(
            SFX_EXPLOSION_1, &projectile_item->pos, 0x2000000 | SPM_PITCH);
    }

    const GAME_VECTOR pos = {
        .pos = projectile_item->pos,
        .room_num = projectile_item->room_num,
    };
    const int32_t smoke_count = g_Weapons[LGT_ROCKET].smoke_count;
    Sparks_TriggerGunSmoke(pos, true, LGT_ROCKET, smoke_count);

    projectile_item->shade.value_1 = -1;
    projectile_item->shade.value_2 = -1;

    const XYZ_32 back_128 = XYZ_32_FromYawPitch(
        projectile_item->rot.y, projectile_item->rot.x, -128);
    for (int32_t i = 0; i < 8; i++) {
        const int32_t dist = -(Random_GetControl() & 0x7FF);
        const XYZ_32 back_vel = XYZ_32_FromYawPitch(
            projectile_item->rot.y, projectile_item->rot.x, dist);
        Sparks_TriggerRocketFlame(
            back_128,
            (XYZ_32) {
                .x = back_vel.x - back_128.x,
                .y = back_vel.y - back_128.y,
                .z = back_vel.z - back_128.z,
            },
            item_num, projectile_item->room_num);
    }
}

static void M_UserControl(ITEM *const gun_item)
{
    M_PRIV *const p = gun_item->priv;
    if (p->state != M_GUN_STATE_CONTROL) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int16_t frame_num = Item_GetRelativeFrame(gun_item);
    if (lara_item->hit_points <= 0 || g_Input.roll) {
        p->state = M_GUN_STATE_DISMOUNT;
        return;
    }

    if (g_Input.action && p->fire_count == 0) {
        M_Fire(gun_item);
        p->fire_count = M_FIRE_COOLDOWN;
        return;
    }

    if (g_Input.left) {
        if (p->yaw_speed > 0) {
            p->yaw_speed >>= 1;
        }

        p->yaw_speed -= M_ROT_SPEED;
        CLAMPL(p->yaw_speed, M_MIN_ROT_SPEED);

        if ((frame_num & 7) == 0 && ABS(p->yaw) < M_MAX_ROT) {
            Sound_Effect(SFX_LARA_UZI_STOP, &gun_item->pos, SPM_NORMAL);
        }
    } else if (g_Input.right) {
        if (p->yaw_speed < 0) {
            p->yaw_speed >>= 1;
        }

        p->yaw_speed += M_ROT_SPEED;
        CLAMPG(p->yaw_speed, M_MAX_ROT_SPEED);

        if ((frame_num & 7) == 0 && ABS(p->yaw) < M_MAX_ROT) {
            Sound_Effect(SFX_LARA_UZI_STOP, &gun_item->pos, SPM_NORMAL);
        }
    } else {
        p->yaw_speed -= p->yaw_speed >> 2;
        if (ABS(p->yaw_speed) < M_ROT_SPEED) {
            p->yaw_speed = 0;
        }
    }

    p->yaw += (int16_t)(p->yaw_speed >> 2);
    if (p->yaw < M_MIN_ROT) {
        p->yaw = M_MIN_ROT;
        p->yaw_speed = 0;
    } else if (p->yaw > M_MAX_ROT) {
        p->yaw = M_MAX_ROT;
        p->yaw_speed = 0;
    }

    if (g_Input.forward && p->tilt < M_MAX_TILT) {
        p->tilt++;
    } else if (g_Input.back && p->tilt != 0) {
        p->tilt--;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->collision_func = M_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

bool MountedGun_Control(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    ITEM *const lara_item = Lara_GetItem();
    ITEM *const gun_item = Lara_Vehicle_GetItem();
    M_PRIV *const p = gun_item->priv;

    M_UserControl(gun_item);

    if (p->state == M_GUN_STATE_DISMOUNT) {
        if (p->tilt < M_NEUTRAL_TILT) {
            p->tilt++;
        } else if (p->tilt > M_NEUTRAL_TILT) {
            p->tilt--;
        } else {
            Item_SwitchToObjAnim(
                lara_item, M_ANIM_DISMOUNT, 0, O_LARA_VEHICLE_ANIM);
            lara_item->current_anim_state = M_STATE_DISMOUNT;
            lara_item->goal_anim_state = M_STATE_DISMOUNT;
            p->state = M_GUN_STATE_WAIT_END;
        }
    }

    switch (lara_item->current_anim_state) {
    case M_STATE_MOUNT:
    case M_STATE_DISMOUNT:
        Item_Animate(lara_item);
        const ANIM *const anim = Item_GetAnim(lara_item);
        const int16_t anim_num =
            lara_item->anim_num - Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx;
        const int16_t frame_num = lara_item->frame_num - anim->frame_base;
        Item_SwitchToAnim(gun_item, anim_num, frame_num);

        if (p->state == M_GUN_STATE_WAIT_END
            && Item_TestFrameEqual(gun_item, -1)) {
            Lara_Vehicle_Dismount();
            lara->gun_status = LGS_ARMLESS;
        }
        break;

    case M_STATE_TILT:
        Item_SwitchToObjAnim(
            lara_item, M_ANIM_TILT, p->tilt, O_LARA_VEHICLE_ANIM);
        Item_SwitchToAnim(gun_item, M_ANIM_TILT, p->tilt);

        if (p->fire_count != 0) {
            p->fire_count--;
        }

        p->state = M_GUN_STATE_CONTROL;
        break;

    default:
        break;
    }

    gun_item->rot.y = p->yaw_offset + M_ROT_SCALE * p->yaw;
    lara_item->rot.y = gun_item->rot.y;
    g_Camera.target_elevation = M_CAM_ELEVATION;

    return true;
}

REGISTER_OBJECT(O_MOUNTED_GUN, M_Setup)
