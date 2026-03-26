#include <trx/game/objects/vehicles/upv.h>

#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/camera.h>
#include <trx/game/game.h>
#include <trx/game/gun.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/los.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>
#include <trx/game/stats.h>

// clang-format off
#define M_ACCELERATION     0x40000
#define M_FRICTION         0x18000
#define M_MAX_SPEED        0x400000
#define M_ROT_ACCELERATION 0x400000
#define M_ROT_SLOWACCEL    0x200000
#define M_ROT_FRICTION     0x100000
#define M_MAX_ROTATION     0x1C00000
#define M_UPDOWN_ACCEL     0x16C0000
#define M_UPDOWN_FRICTION  0xB60000
#define M_MAX_UPDOWN       0x16C0000
#define M_CAM_ELEVATION    (DEG_1 * -60) // = -10920
// clang-format on

static const BITE m_UPVBites[6] = {
    { .pos = { .x = 0, .y = 0, .z = 0 }, .mesh_num = 3 },
    { .pos = { .x = 0, .y = 96, .z = 256 }, .mesh_num = 0 },
    { .pos = { .x = -128, .y = 0, .z = -64 }, .mesh_num = 1 },
    { .pos = { .x = 0, .y = 0, .z = -64 }, .mesh_num = 1 },
    { .pos = { .x = 128, .y = 0, .z = -64 }, .mesh_num = 2 },
    { .pos = { .x = 0, .y = 0, .z = -64 }, .mesh_num = 2 },
};

typedef struct {
    int32_t vel;
    int32_t rot;
    int32_t rot_x;
    int16_t fan_rot;
    struct {
        bool control;
        bool surface;
        bool dive;
        bool dead;
    } flags;
    int8_t weapon_timer;
    bool current_weapon; // left|right
} M_PRIV;

typedef enum {
    // clang-format off
    M_STATE_DEATH           = 0,
    M_STATE_GET_OFF_SURFACE = 2,
    M_STATE_MOVE            = 4,
    M_STATE_POSE            = 5,
    M_STATE_GET_ON          = 8,
    M_STATE_GET_OFF         = 9,
    // clang-format on
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_DEATH            = 0,
    M_ANIM_IDLE             = 5,
    M_ANIM_GET_OFF_SURFACE  = 9,
    M_ANIM_GET_ON_SURFACE   = 10,
    M_ANIM_GET_ON_SURFACE_1 = 11,
    M_ANIM_GET_OFF          = 12,
    M_ANIM_GET_ON           = 13,
    // clang-format on
} M_ANIM;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "vel", &p->vel));
    JSON_SHOULD(JSON_READ(io, "rot", &p->rot));
    JSON_SHOULD(JSON_READ(io, "rot_x", &p->rot_x));
    JSON_SHOULD(JSON_READ(io, "fan_rot", &p->fan_rot));
    JSON_SHOULD(JSON_READ(io, "weapon_timer", &p->weapon_timer));
    JSON_SHOULD(JSON_READ(io, "current_weapon", &p->current_weapon));
    if (JSON_SHOULD(JSON_PUSH(io, "flags"))) {
        JSON_SHOULD(JSON_READ(io, "control", &p->flags.control));
        JSON_SHOULD(JSON_READ(io, "surface", &p->flags.surface));
        JSON_SHOULD(JSON_READ(io, "dive", &p->flags.dive));
        JSON_SHOULD(JSON_READ(io, "dead", &p->flags.dead));
        JSON_POP(io);
    }
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "vel", p->vel);
    JSONW_WRITE(io, "rot", p->rot);
    JSONW_WRITE(io, "rot_x", p->rot_x);
    JSONW_WRITE(io, "fan_rot", p->fan_rot);
    JSONW_WRITE(io, "weapon_timer", p->weapon_timer);
    JSONW_WRITE(io, "current_weapon", p->current_weapon);
    JSONW_PUSH_OBJECT(io);
    JSONW_WRITE(io, "control", p->flags.control);
    JSONW_WRITE(io, "surface", p->flags.surface);
    JSONW_WRITE(io, "dive", p->flags.dive);
    JSONW_WRITE(io, "dead", p->flags.dead);
    JSONW_POP_AND_SET(io, "flags");
}

static void M_Initialise(int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->flags.surface = true;
}

static bool M_CanGetOn(const ITEM *const item)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (!g_Input.action || lara->gun_status != LGS_ARMLESS
        || lara_item->gravity) {
        return false;
    }

    if (ABS(lara_item->pos.y - item->pos.y + 128) > 256) {
        return false;
    }

    const int32_t dist = XYZ_32_GetLength2((XYZ_32) {
        .x = lara_item->pos.x - item->pos.x,
        .y = 0,
        .z = lara_item->pos.z - item->pos.z,
    });
    if (dist > SQUARE(WALL_L / 2)) {
        return false;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    const int32_t h = Room_GetHeight(sector, item->pos);
    if (h < -32000) {
        return false;
    }

    return true;
}

static bool M_CanGetOff(const ITEM *const item)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    M_PRIV *const p = item->priv;
    if (lara->current.vel.x || lara->current.vel.z || p->vel) {
        return false;
    }

    const int32_t rad = WALL_L * Math_Cos(item->rot.x) >> W2V_SHIFT;
    XYZ_32 pos = {
        .x = item->pos.x
            + ((rad * Math_Sin(item->rot.y + DEG_180)) >> W2V_SHIFT),
        .y = item->pos.y - ((WALL_L * Math_Sin(item->rot.x)) >> W2V_SHIFT),
        .z = item->pos.z
            + ((rad * Math_Cos(item->rot.y + DEG_180)) >> W2V_SHIFT),
    };

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);

    const int32_t h = Room_GetHeight(sector, pos);
    if (h == NO_HEIGHT || pos.y > h) {
        return false;
    }

    const int32_t c = Room_GetCeiling(sector, pos);
    if (h - c < STEP_L || pos.y < c || c == NO_HEIGHT) {
        return false;
    }

    return true;
}

static void M_GetOn(ITEM *const item)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    ITEM *const lara_item = Lara_GetItem();

    Lara_Vehicle_SetIndex(Item_GetIndex(item));
    lara->water_status = LWS_ABOVE_WATER;

    if (lara->gun_type == LGT_FLARE) {
        Lara_Flare_Dispose(false);
        lara->flare.control = false;
        lara->gun_type = LGT_UNARMED;
        lara->request_gun_type = LGT_UNARMED;
    }

    lara->gun_status = LGS_HANDS_BUSY;
    // item->hit_points = 1; // TODO: why is it set?
    lara_item->pos = item->pos;
    lara_item->rot.y = item->rot.y;

    if (lara_item->current_anim_state == LS(LS_SURF_TREAD)
        || lara_item->current_anim_state == LS(LS_SURF_SWIM)) {
        Item_SwitchToObjAnim(
            lara_item, M_ANIM_GET_ON_SURFACE, 0, O_LARA_VEHICLE_ANIM);
    } else {
        Item_SwitchToObjAnim(lara_item, M_ANIM_GET_ON, 0, O_LARA_VEHICLE_ANIM);
    }

    lara_item->goal_anim_state = M_STATE_GET_ON;
    lara_item->current_anim_state = M_STATE_GET_ON;
    Item_Animate(lara_item);

    if (item->status != IS_ACTIVE) {
        item->status = IS_ACTIVE;
        Item_AddActive(Item_GetIndex(item));
    }
}

static void M_Collision(
    int16_t item_num, ITEM *const lara_item, COLL_INFO *coll)
{
    ITEM *const item = Item_Get(item_num);
    if (lara_item->hit_points < 0 || Lara_Vehicle_GetItem() != nullptr) {
        return;
    }

    if (M_CanGetOn(item)) {
        M_GetOn(item);
    } else {
        item->pos.y += 128;
        if (Lara_TestBoundsCollide(item, coll->radius)
            && Collide_TestCollision(item, lara_item)) {
            Lara_Col_ItemPush(item, coll, false, false);
        }
        item->pos.y -= 128;
    }
}

static bool M_Draw(const ITEM *const item)
{
    int32_t rate;
    ANIM_FRAME *frames[2];
    const int32_t frac = Item_GetFrames(item, frames, &rate);

    OBJECT *const obj = Object_Get(item->object_id);
    Output_DrawShadow(256, &frames[0]->bounds, item);

    Matrix_Push();
    Matrix_TranslateAbs32((XYZ_32) {
        item->interp.result.pos.x,
        item->interp.result.pos.y + 128,
        item->interp.result.pos.z,
    });
    Matrix_Rot16(item->interp.result.rot);

    bool result = false;
    const CLIP clip = Output_CheckBoundsClip(&frames[0]->bounds);
    if (clip == CLIP_NOT_VISIBLE) {
        goto finish;
    }

    M_PRIV *const p = item->priv;
    Output_CalculateObjectLighting(item, &frames[0]->bounds);
    const ANIM_BONE *const bone = Anim_GetBone(obj->bone_idx);

    if (frac != 0) {
        Matrix_InitInterpolate(frac, rate);
        Matrix_TranslateRel16_ID(frames[0]->offset, frames[1]->offset);
        Matrix_Rot16_ID(frames[0]->mesh_rots[0], frames[1]->mesh_rots[0]);
        Output_DrawObjectMesh_I(Object_GetMesh(obj->mesh_idx), clip);

        Matrix_Push_I();
        Matrix_TranslateRel32_I(bone[0].pos);
        Matrix_Rot16_ID(frames[0]->mesh_rots[1], frames[1]->mesh_rots[1]);
        Matrix_RotX_I((item->rot.z + (p->rot_x >> 13)));
        Output_DrawObjectMesh_I(Object_GetMesh(obj->mesh_idx + 1), clip);
        Matrix_Pop_I();

        Matrix_Push_I();
        Matrix_TranslateRel32_I(bone[1].pos);
        Matrix_Rot16_ID(frames[0]->mesh_rots[2], frames[1]->mesh_rots[2]);
        Matrix_RotX_I(((p->rot_x >> 13) - item->rot.z));
        Output_DrawObjectMesh_I(Object_GetMesh(obj->mesh_idx + 2), clip);
        Matrix_Pop_I();

        Matrix_Push_I();
        Matrix_TranslateRel32_I(bone[2].pos);
        Matrix_Rot16_ID(frames[0]->mesh_rots[3], frames[1]->mesh_rots[3]);
        Matrix_RotZ_I(p->fan_rot);
        Output_DrawObjectMesh_I(Object_GetMesh(obj->mesh_idx + 3), clip);
        Matrix_Pop_I();
    } else {
        Matrix_TranslateRel16(frames[0]->offset);
        Matrix_Rot16(frames[0]->mesh_rots[0]);
        Output_DrawObjectMesh(Object_GetMesh(obj->mesh_idx), clip);

        Matrix_Push();
        Matrix_TranslateRel32(bone[0].pos);
        Matrix_Rot16(frames[0]->mesh_rots[1]);
        Matrix_RotX((item->rot.z + (p->rot_x >> 13)));
        Output_DrawObjectMesh(Object_GetMesh(obj->mesh_idx + 1), clip);
        Matrix_Pop();

        Matrix_Push();
        Matrix_TranslateRel32(bone[1].pos);
        Matrix_Rot16(frames[0]->mesh_rots[2]);
        Matrix_RotX(((p->rot_x >> 13) - item->rot.z));
        Output_DrawObjectMesh(Object_GetMesh(obj->mesh_idx + 2), clip);
        Matrix_Pop();

        Matrix_Push();
        Matrix_TranslateRel32(bone[2].pos);
        Matrix_Rot16(frames[0]->mesh_rots[3]);
        Matrix_RotZ(p->fan_rot);
        Output_DrawObjectMesh(Object_GetMesh(obj->mesh_idx + 3), clip);
        Matrix_Pop();
    }
    result = true;

finish:
    Matrix_Pop();
    return result;
}

static void M_UserInput(
    ITEM *const item, ITEM *const lara_item, M_PRIV *const p)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    XYZ_32 pos;
    GAME_VECTOR start_pos;
    GAME_VECTOR target_pos;
    int32_t water_height;
    int16_t anim, frame;

    M_CanGetOff(item);
    anim = Item_GetRelativeObjAnim(lara_item, O_LARA_VEHICLE_ANIM);
    frame = Item_GetRelativeFrame(lara_item);

    switch (lara_item->current_anim_state) {
    case M_STATE_DEATH:
        if (anim == M_ANIM_DEATH && (frame == 16 || frame == 17)) {
            pos.x = 0;
            pos.y = 0;
            pos.z = 0;
            Lara_GetMeshPos(LM_HIPS, &pos);

            lara_item->pos = pos;
            Item_SwitchToAnim(lara_item, LA(LA_UNDERWATER_DEATH), 0);
            lara_item->current_anim_state = LS_UW_DEATH;
            lara_item->goal_anim_state = LS_UW_DEATH;
            lara_item->fall_speed = 0;
            lara_item->gravity = false;
            lara_item->rot.x = 0;
            lara_item->rot.z = 0;
            p->flags.dead = true;
        }

        item->speed = 0;
        break;

    case M_STATE_GET_OFF_SURFACE:
        if (anim == M_ANIM_GET_OFF_SURFACE && frame == 51) {
            pos.x = 0;
            pos.y = 0;
            pos.z = 0;
            water_height =
                Room_GetWaterHeight(lara_item->pos, lara_item->room_num);

            int32_t water_surface_dist;
            if (water_height == NO_HEIGHT) {
                water_surface_dist = NO_HEIGHT;
            } else {
                water_surface_dist = lara_item->pos.y - water_height;
            }

            Lara_GetMeshPos(LM_HIPS, &pos);
            lara_item->pos = pos;
            Item_SwitchToAnim(lara_item, LA(LA_UNDERWATER_TO_ONWATER), 0);
            lara_item->current_anim_state = LS_SURF_TREAD;
            lara_item->goal_anim_state = LS_SURF_TREAD;
            lara_item->fall_speed = 0;
            lara_item->gravity = false;
            lara_item->rot.x = 0;
            lara_item->rot.z = 0;
            Lara_UpdateRoomToHeight(-381);
            lara->water_status = LWS_SURFACE;
            lara->water_surface_dist = -water_surface_dist;
            lara->dive_timer = 11;
            lara->torso_rot.x = 0;
            lara->torso_rot.y = 0;
            lara->head_rot.x = 0;
            lara->head_rot.y = 0;
            lara->gun_status = LGS_ARMLESS;
            Lara_Vehicle_SetIndex(NO_ITEM);
            item->hit_points = 0;
        }
        break;

    case M_STATE_MOVE:
        if (lara_item->hit_points <= 0) {
            lara_item->goal_anim_state = 0;
            break;
        }

        if (g_Input.left) {
            p->rot -= M_ROT_ACCELERATION;
        } else if (g_Input.right) {
            p->rot += M_ROT_ACCELERATION;
        }

        if (p->flags.surface) {
            if (item->rot.x > 9100) {
                item->rot.x -= 182;
            } else if (item->rot.x < 9100) {
                item->rot.x += 182;
            }
        } else if (g_Input.forward) {
            p->rot_x -= M_UPDOWN_ACCEL;
        } else if (g_Input.back) {
            p->rot_x += M_UPDOWN_ACCEL;
        }

        if (g_Input.jump) {
            if (p->flags.surface && g_Input.forward && item->rot.x > -2730) {
                p->flags.dive = true;
            }

            p->vel += M_ACCELERATION;
        } else {
            lara_item->goal_anim_state = M_STATE_POSE;
        }
        break;

    case M_STATE_POSE:
        if (lara_item->hit_points <= 0) {
            lara_item->goal_anim_state = 0;
            break;
        }

        if (g_Input.left) {
            p->rot -= M_ROT_SLOWACCEL;
        } else if (g_Input.right) {
            p->rot += M_ROT_SLOWACCEL;
        }

        if (p->flags.surface) {
            if (item->rot.x > 9100) {
                item->rot.x -= 182;
            } else if (item->rot.x < 9100) {
                item->rot.x += 182;
            }
        } else if (g_Input.forward) {
            p->rot_x -= M_UPDOWN_ACCEL;
        } else if (g_Input.back) {
            p->rot_x += M_UPDOWN_ACCEL;
        }

        if (g_Input.roll && M_CanGetOff(item)) {
            if (p->flags.surface) {
                lara_item->goal_anim_state = M_STATE_GET_OFF_SURFACE;
            } else {
                lara_item->goal_anim_state = M_STATE_GET_OFF;
            }

            p->flags.control = false;
            Sound_StopEffect(SFX_UPV_LOOP);
            Sound_Effect(SFX_UPV_STOP, &item->pos, SPM_ALWAYS);
        } else if (g_Input.jump) {
            if (p->flags.surface && g_Input.forward && item->rot.x > -2730) {
                p->flags.dive = true;
            }

            lara_item->goal_anim_state = M_STATE_MOVE;
        }
        break;

    case M_STATE_GET_ON:
        if (anim == M_ANIM_GET_ON_SURFACE_1) {
            item->rot.x += 182;
            item->pos.y += 4;

            if (frame == 30) {
                Sound_Effect(SFX_UPV_START, &item->pos, SPM_ALWAYS);
            }

            if (frame == 50) {
                p->flags.control = true;
            }
        } else if (anim == M_ANIM_GET_ON) {
            if (frame == 30) {
                Sound_Effect(SFX_UPV_START, &item->pos, SPM_ALWAYS);
            }

            if (frame == 42) {
                p->flags.control = true;
            }
        }
        break;

    case M_STATE_GET_OFF:
        if (anim == M_ANIM_GET_OFF && frame == 42) {
            pos.x = 0;
            pos.y = 0;
            pos.z = 0;
            Lara_GetMeshPos(LM_HIPS, &pos);

            start_pos.pos = item->pos;
            start_pos.room_num = item->room_num;
            target_pos.pos = pos;
            target_pos.room_num = item->room_num;
            Camera_LOSCheck(&start_pos, &target_pos, 0);

            lara_item->pos = target_pos.pos;
            Item_SwitchToAnim(lara_item, LA(LA_UNDERWATER_IDLE), 0);
            lara_item->current_anim_state = LS_TREAD;
            lara_item->fall_speed = 0;
            lara_item->gravity = false;
            lara_item->rot.x = 0;
            lara_item->rot.z = 0;
            Lara_UpdateRoomToHeight(0);
            lara->water_status = LWS_UNDERWATER;
            lara->gun_status = LGS_ARMLESS;
            Lara_Vehicle_SetIndex(NO_ITEM);
            item->hit_points = 0;
        }
        break;
    }

    if (p->flags.dive) {
        if (item->rot.x > -2730) {
            item->rot.x -= 910;
        } else {
            p->flags.dive = false;
        }
    }

    if (p->vel > 0) {
        p->vel -= M_FRICTION;
        CLAMPL(p->vel, 0);

    } else if (p->vel < 0) {
        p->vel += M_FRICTION;
        CLAMPG(p->vel, 0);
    }

    CLAMP(p->vel, -M_MAX_SPEED, M_MAX_SPEED);

    if (p->rot > 0) {
        p->rot -= M_ROT_FRICTION;
        CLAMPL(p->rot, 0);
    } else if (p->rot < 0) {
        p->rot += M_ROT_FRICTION;
        CLAMPG(p->rot, 0);
    }

    if (p->rot_x > 0) {
        p->rot_x -= M_UPDOWN_FRICTION;
        CLAMPL(p->rot_x, 0);
    } else if (p->rot_x < 0) {
        p->rot_x += M_UPDOWN_FRICTION;
        CLAMPG(p->rot_x, 0);
    }

    CLAMP(p->rot, -M_MAX_ROTATION, M_MAX_ROTATION);
    CLAMP(p->rot_x, -M_MAX_UPDOWN, M_MAX_UPDOWN);
}

static void M_DoCurrent(ITEM *const item)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();

    if (lara->current.active != 0) {
        const int32_t sink_val = lara->current.active - 1;
        const OBJECT_VECTOR *const camera_obj = Camera_GetFixedObject(sink_val);
        const int32_t angle = -Math_Atan(
                                  lara_item->pos.x - camera_obj->pos.x,
                                  lara_item->pos.z - camera_obj->pos.z)
            - DEG_90;
        const int32_t speed = camera_obj->data;
        const int32_t xvel = (speed * Math_Sin(angle)) >> 4;
        const int32_t zvel = (speed * Math_Cos(angle)) >> 4;
        lara->current.vel.x += ((xvel - lara->current.vel.x) >> 4);
        lara->current.vel.z += ((zvel - lara->current.vel.z) >> 4);
    } else {
        int32_t shifter;

        int32_t abs_vel = ABS(lara->current.vel.x);
        if (abs_vel > 16) {
            shifter = 4;
        } else if (abs_vel > 8) {
            shifter = 3;
        } else {
            shifter = 2;
        }
        lara->current.vel.x -= lara->current.vel.x >> shifter;
        if (ABS(lara->current.vel.x) < 4) {
            lara->current.vel.x = 0;
        }

        abs_vel = ABS(lara->current.vel.z);
        if (abs_vel > 16) {
            shifter = 4;
        } else if (abs_vel > 8) {
            shifter = 3;
        } else {
            shifter = 2;
        }
        lara->current.vel.z -= lara->current.vel.z >> shifter;
        if (ABS(lara->current.vel.z) < 4) {
            lara->current.vel.z = 0;
        }

        if (lara->current.vel.x == 0 && lara->current.vel.z == 0) {
            return;
        }
    }

    item->pos.x += lara->current.vel.x >> 8;
    item->pos.z += lara->current.vel.z >> 8;
    lara->current.active = false;
}

static void M_FireHarpoon(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    AMMO_INFO *const ammo = Gun_GetAmmoInfo(LGT_HARPOON);
    if (ammo->ammo <= 0) {
        return;
    }

    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return;
    }

    ITEM *const bolt = Item_Get(item_num);
    bolt->object_id = O_HARPOON_BOLT;
    bolt->shade.value_1 = -0x3DF0;
    bolt->room_num = item->room_num;
    XYZ_32 pos = {
        .x = p->current_weapon != 0 ? 22 : -22,
        .y = 24,
        .z = 230,
    };
    Collide_GetJointAbsPosition(item, &pos, 3);
    bolt->pos = pos;
    Item_Initialise(item_num);
    bolt->rot.x = item->rot.x;
    bolt->rot.y = item->rot.y;
    bolt->rot.z = 0;
    bolt->fall_speed = (-256 * Math_Sin(bolt->rot.x)) >> W2V_SHIFT;
    bolt->speed = (256 * Math_Cos(bolt->rot.x)) >> W2V_SHIFT;
    bolt->hit_points = 256;
    // bolt->item_flags[0] = 1; // TODO: what
    Item_AddActive(item_num);
    Sound_Effect(SFX_UPV_HARPOON, &Lara_GetItem()->pos, SPM_ALWAYS);

    if (!Game_IsBonusFlagSet(GBF_NGPLUS)) {
        ammo->ammo--;
    }

    Stats_AddAmmoUsed();
    p->current_weapon ^= 1;
}

static void M_BackgroundCollision(
    ITEM *const item, const ITEM *const lara_item, M_PRIV *const p)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    COLL_INFO coll = {
        .bad_pos = -NO_HEIGHT,
        .bad_neg = -400,
        .bad_ceiling = 400,
        .old = item->pos,
        .radius = 300,
        .slopes_are_walls = false,
        .slopes_are_pits = false,
        .lava_is_pit = false,
        .enable_hit = false,
        .enable_baddie_push = true,
    };

    if (item->rot.x < -DEG_90 || item->rot.x > DEG_90) {
        lara->move_angle = item->rot.y + DEG_180;
    } else {
        lara->move_angle = item->rot.y;
    }

    coll.facing = lara->move_angle;
    int32_t h = (WALL_L * Math_Sin(item->rot.x)) >> W2V_SHIFT;
    h = ABS(h);
    CLAMPL(h, 200);

    coll.bad_neg = -h;
    Collide_GetCollisionInfo(
        &coll, item->pos.x, item->pos.y + h / 2, item->pos.z, item->room_num,
        h);
    Collide_ShiftItem(item, &coll);

    switch (coll.coll_type) {
    case COLL_FRONT:
        if (p->rot_x > 0x1FFE0000) {
            p->rot_x += 0x16C0000;
        } else if (p->rot_x < -0x1FFE0000) {
            p->rot_x -= 0x16C0000;
        } else {
            p->vel = 0;
        }
        break;

    case COLL_TOP:
        if (p->rot_x >= -0x1FFE0000) {
            p->rot_x -= 0x16C0000;
        }
        break;

    case COLL_TOP_FRONT:
        p->vel = 0;
        break;

    case COLL_LEFT:
        item->rot.y += 910;
        break;

    case COLL_RIGHT:
        item->rot.y -= 910;
        break;

    case COLL_CLAMP:
        item->pos = coll.old;
        p->vel = 0;
        return;
    }

    if (coll.side_mid.floor < 0) {
        item->pos.y += coll.side_mid.floor;
        p->rot_x += 0x16C0000;
    }
}

static void M_TriggerMist(
    const XYZ_32 pos, const int32_t speed, const int16_t angle)
{
    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;

    spark->src_color.r = 0;
    spark->src_color.g = 0;
    spark->src_color.b = 0;
    spark->dst_color.r = 64;
    spark->dst_color.g = 64;
    spark->dst_color.b = 64;

    spark->fade_to_black = 12;
    spark->col_fade_speed = (Random_GetControl() & 3) + 4;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->life = (Random_GetControl() & 3) + 20;
    spark->s_life = spark->life;
    spark->dynamic = -1;
    spark->pos.x = (Random_GetControl() & 0xF) + pos.x - 8;
    spark->pos.y = (Random_GetControl() & 0xF) + pos.y - 8;
    spark->pos.z = (Random_GetControl() & 0xF) + pos.z - 8;

    spark->vel.x = (Random_GetControl() & 0x7F)
        + ((speed * Math_Sin(angle)) >> (W2V_SHIFT + 2)) - 64;
    spark->vel.y = 0;
    spark->vel.z = (Random_GetControl() & 0x7F)
        + ((speed * Math_Cos(angle)) >> (W2V_SHIFT + 2)) - 64;

    spark->friction = 3;

    if (Random_GetControl() & 1) {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_ROTATE | SPARK_F_SPRITE
            | SPARK_F_SCALE;
        spark->rot_angle = Random_GetControl() & 0xFFF;

        if (Random_GetControl() & 1) {
            spark->rot_add = -16 - (Random_GetControl() & 0xF);
        } else {
            spark->rot_add = (Random_GetControl() & 0xF) + 16;
        }
    } else {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_SPRITE | SPARK_F_SCALE;
    }

    spark->scalar = 3;
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->max_y_vel = 0;
    spark->gravity = 0;
    spark->dst_size.width = ((Random_GetControl() & 7) + (speed >> 1) + 16);
    spark->dst_size.height = spark->dst_size.width;
    spark->src_size.width = spark->dst_size.width >> 2;
    spark->src_size.height = spark->src_size.width;
    spark->size.width = spark->src_size.width;
    spark->size.height = spark->src_size.height;
    Sparks_FinishSetup(spark);
}

static void M_Control(int16_t item_num)
{
    XYZ_32 pos;
    GAME_VECTOR start_pos;
    GAME_VECTOR target_pos;
    int32_t c;

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (Lara_Vehicle_GetItem() == item) {
        if (p->vel) {
            p->fan_rot += (p->vel >> 12);
            pos = m_UPVBites[0].pos;
            Collide_GetJointAbsPosition(item, &pos, m_UPVBites[0].mesh_num);
            M_TriggerMist(
                (XYZ_32) { pos.x, pos.y + 128, pos.z }, ABS(p->vel) >> 16,
                item->rot.y + DEG_180);

            if (!(Random_GetControl() & 1)) {
                XYZ_32 bubble_pos = {
                    .x = (Random_GetControl() & 0x3F) + pos.x - 32,
                    .y = pos.y + 128,
                    .z = (Random_GetControl() & 0x3F) + pos.z - 32,
                };
                int16_t room_num = item->room_num;
                Room_GetSector(bubble_pos, &room_num);
                Spawn_BubbleEx(&bubble_pos, room_num, 4, 8);
            }
        } else {
            p->fan_rot += 364;
        }
    }

    for (int32_t i = 0; i < 2; i++) {
        pos = (XYZ_32) {
            .x = m_UPVBites[1].pos.x,
            .y = m_UPVBites[1].pos.y,
            .z = m_UPVBites[1].pos.z << (6 * i),
        };
        Collide_GetJointAbsPosition(item, &pos, m_UPVBites[1].mesh_num);
        c = 255 - (Random_GetControl() & 0x1F);

        if (i == 1) {
            target_pos.pos = pos;
            target_pos.room_num = item->room_num;
            LOS_Check(&start_pos, &target_pos, true);
            pos = target_pos.pos;
        } else {
            start_pos.pos = pos;
            start_pos.room_num = item->room_num;
        }

        Output_AddDynamicLightRGB(pos, 8 * i + 16, (RGB_888) { c, c, c });
    }

    if (p->weapon_timer > 0) {
        p->weapon_timer--;
    }
}

bool UPV_Control(void)
{
    ITEM *const item = Lara_Vehicle_GetItem();
    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();
    M_PRIV *const p = item->priv;

    if (!p->flags.dead) {
        M_UserInput(item, lara_item, p);
        item->speed = p->vel >> 16;
        item->rot.x += p->rot_x >> 16;
        item->rot.y += p->rot >> 16;
        item->rot.z = (int16_t)(p->rot >> 12);

        if (item->rot.x > 14560) {
            item->rot.x = 14560;
        } else if (item->rot.x < -14560) {
            item->rot.x = -14560;
        }

        item->pos.x += (Math_Cos(item->rot.x)
                        * ((item->speed * Math_Sin(item->rot.y)) >> W2V_SHIFT))
            >> W2V_SHIFT;
        item->pos.y -= (item->speed * Math_Sin(item->rot.x)) >> W2V_SHIFT;
        item->pos.z += (Math_Cos(item->rot.x)
                        * ((item->speed * Math_Cos(item->rot.y)) >> W2V_SHIFT))
            >> W2V_SHIFT;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    item->floor = Room_GetHeight(sector, item->pos);

    if (p->flags.control && !p->flags.dead) {
        const int32_t water_height =
            Room_GetWaterHeightEx(item->pos, room_num, false);

        if (water_height != NO_HEIGHT
            && !Room_Get(item->room_num)->flags.underwater) {
            if (water_height - item->pos.y >= -210) {
                item->pos.y = water_height + 210;
            }

            if (!p->flags.surface) {
                Sound_Effect(SFX_LARA_BREATH, &lara_item->pos, SPM_ALWAYS);
                p->flags.dive = false;
            }

            p->flags.surface = true;
        } else if (
            water_height != NO_HEIGHT && water_height - item->pos.y >= -210) {
            item->pos.y = water_height + 210;

            if (!p->flags.surface) {
                Sound_Effect(SFX_LARA_BREATH, &lara_item->pos, SPM_ALWAYS);
                p->flags.dive = false;
            }

            p->flags.surface = true;
        } else {
            p->flags.surface = false;
        }

        if (p->flags.surface) {
            if (lara_item->hit_points >= 0) {
                lara->air += 10;
                CLAMPG(lara->air, LARA_MAX_AIR);
            }
        } else if (lara_item->hit_points >= 0) {
            lara->air--;
            if (lara->air < 0) {
                lara->air = -1;
                lara_item->hit_points -= 5;
            }
        }
    }

    Room_TestTriggers(lara_item);
    Room_TestTriggers(item);

    if (Lara_Vehicle_GetItem() == nullptr) {
        if (!p->flags.dead) {
            return false;
        }
    } else if (!p->flags.dead) {
        M_DoCurrent(item);

        if (g_Input.action && p->flags.control && p->weapon_timer == 0) {
            M_FireHarpoon(item);
            p->weapon_timer = 15;
        }

        if (room_num != item->room_num) {
            Item_UpdateRoom(Lara_Vehicle_GetIndex(), room_num);
            Item_UpdateRoom(lara->item_num, room_num);
        }

        lara_item->pos.x = item->pos.x;
        lara_item->pos.y = item->pos.y + 128;
        lara_item->pos.z = item->pos.z;
        lara_item->rot = item->rot;
        Item_Animate(lara_item);
        M_BackgroundCollision(item, lara_item, p);

        if (p->flags.control) {
            Sound_Effect(
                SFX_UPV_LOOP, &item->pos,
                (item->speed << 16) | 0x1000000 | SPM_PITCH | SPM_ALWAYS);
        }

        const int16_t anim_idx =
            Item_GetRelativeObjAnim(lara_item, O_LARA_VEHICLE_ANIM);
        const int16_t frame_idx = Item_GetRelativeFrame(lara_item);
        Item_SwitchToAnim(item, anim_idx, frame_idx);
        g_Camera.target_elevation = p->flags.surface ? M_CAM_ELEVATION : 0;
        return true;
    }

    Item_Animate(lara_item);

    if (room_num != item->room_num) {
        Item_UpdateRoom(Lara_Vehicle_GetIndex(), room_num);
    }

    M_BackgroundCollision(item, lara_item, p);
    p->rot_x = 0;
    Item_SwitchToAnim(item, M_ANIM_IDLE, 0);
    item->current_anim_state = M_STATE_POSE;
    item->goal_anim_state = M_STATE_POSE;
    item->fall_speed = 0;
    item->gravity = true;
    item->speed = 0;
    Item_Animate(item);
    return true;
}

static void M_Setup(OBJECT *const obj)
{
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = M_Collision;
    obj->draw_func = M_Draw;

    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_UPV, M_Setup)
