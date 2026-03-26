#include <trx/game/lara/misc.h>

#include <trx/config.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/level/settings.h>
#include <trx/game/matrix.h>
#include <trx/game/objects/effects/flame.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/version.h>

static void M_GetJointAbsPosition_I(
    XYZ_32 *const vec, const ANIM_FRAME *const frame1,
    const ANIM_FRAME *const frame2, const int32_t frac, const int32_t rate)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const ITEM *const item = Lara_GetItem();
    const OBJECT *obj = Object_Get(item->object_id);

    Matrix_PushUnit();
    Matrix_Rot16(item->rot);

    const ANIM_BONE *const bone = Object_GetBone(obj, 0);
    const XYZ_16 *mesh_rots_1 = frame1->mesh_rots;
    const XYZ_16 *mesh_rots_2 = frame2->mesh_rots;
    Matrix_InitInterpolate(frac, rate);

    Matrix_TranslateRel16_ID(frame1->offset, frame2->offset);
    Matrix_Rot16_ID(mesh_rots_1[LM_HIPS], mesh_rots_2[LM_HIPS]);

    Matrix_TranslateRel32_I(bone[LM_TORSO - 1].pos);
    Matrix_Rot16_ID(mesh_rots_1[LM_TORSO], mesh_rots_2[LM_TORSO]);
    Matrix_Rot16_I(lara_info->torso_rot);

    LARA_GUN_TYPE gun_type = LGT_UNARMED;
    if (lara_info->gun_status == LGS_READY
        || lara_info->gun_status == LGS_SPECIAL
        || lara_info->gun_status == LGS_DRAW
        || lara_info->gun_status == LGS_UNDRAW) {
        gun_type = lara_info->gun_type;
    }

    if (lara_info->gun_type == LGT_FLARE) {
        Matrix_Interpolate();
        Matrix_TranslateRel32(bone[LM_UARM_L - 1].pos);
        if (lara_info->flare.control) {
            const LARA_ARM *const arm = &lara_info->left_arm;
            const ANIM *const anim = Anim_GetAnim(arm->anim_num);
            mesh_rots_1 =
                arm->frame_base[arm->frame_num - anim->frame_base].mesh_rots;
        }
        Matrix_Rot16(mesh_rots_1[LM_UARM_L]);

        Matrix_TranslateRel32(bone[LM_LARM_L - 1].pos);
        Matrix_Rot16(mesh_rots_1[LM_LARM_L]);

        Matrix_TranslateRel32(bone[LM_HAND_L - 1].pos);
        Matrix_Rot16(mesh_rots_1[LM_HAND_L]);
    } else if (gun_type != LGT_UNARMED) {
        Matrix_Interpolate();
        Matrix_TranslateRel32(bone[LM_UARM_R - 1].pos);

        const LARA_ARM *const arm = &lara_info->right_arm;
        const ANIM *const anim = Anim_GetAnim(arm->anim_num);
        mesh_rots_1 = arm->frame_base[arm->frame_num].mesh_rots;
        Matrix_Rot16(mesh_rots_1[LM_UARM_R]);

        Matrix_TranslateRel32(bone[LM_LARM_R - 1].pos);
        Matrix_Rot16(mesh_rots_1[LM_LARM_R]);

        Matrix_TranslateRel32(bone[LM_HAND_R - 1].pos);
        Matrix_Rot16(mesh_rots_1[LM_HAND_R]);
    }

    Matrix_TranslateRel32(*vec);
    vec->x = item->pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
    vec->y = item->pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
    vec->z = item->pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);
    Matrix_Pop();
}

// TODO: joint is ignored - this only works for hands.
void Lara_GetJointAbsPosition(XYZ_32 *const vec, const LARA_MESH joint)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    ANIM_FRAME *frmptr[2] = { nullptr, nullptr };
    if (lara_info->hit_direction < 0) {
        int32_t rate;
        const int32_t frac = Item_GetFrames(lara_item, frmptr, &rate);
        if (frac != 0) {
            M_GetJointAbsPosition_I(vec, frmptr[0], frmptr[1], frac, rate);
            return;
        }
    }

    const ANIM_FRAME *const hit_frame = Lara_GetHitFrame(lara_item);
    const ANIM_FRAME *const frame_ptr =
        hit_frame == nullptr ? frmptr[0] : hit_frame;

    Matrix_PushUnit();
    Matrix_Rot16(lara_item->rot);

    const XYZ_16 *mesh_rots = frame_ptr->mesh_rots;
    const OBJECT *const obj = Object_Get(lara_item->object_id);
    const ANIM_BONE *bone = Object_GetBone(obj, 0);

    Matrix_TranslateRel16(frame_ptr->offset);
    Matrix_Rot16(mesh_rots[LM_HIPS]);

    Matrix_TranslateRel32(bone[LM_TORSO - 1].pos);
    Matrix_Rot16(mesh_rots[LM_TORSO]);
    Matrix_Rot16(lara_info->torso_rot);

    LARA_GUN_TYPE gun_type = LGT_UNARMED;
    if (lara_info->gun_status == LGS_READY
        || lara_info->gun_status == LGS_SPECIAL
        || lara_info->gun_status == LGS_DRAW
        || lara_info->gun_status == LGS_UNDRAW) {
        gun_type = lara_info->gun_type;
    }

    if (lara_info->gun_type == LGT_FLARE) {
        Matrix_TranslateRel32(bone[LM_UARM_L - 1].pos);
        if (lara_info->flare.control) {
            const LARA_ARM *const arm = &lara_info->left_arm;
            const ANIM *const anim = Anim_GetAnim(arm->anim_num);
            mesh_rots =
                arm->frame_base[arm->frame_num - anim->frame_base].mesh_rots;
        }
        Matrix_Rot16(mesh_rots[LM_UARM_L]);

        Matrix_TranslateRel32(bone[LM_LARM_L - 1].pos);
        Matrix_Rot16(mesh_rots[LM_LARM_L]);

        Matrix_TranslateRel32(bone[LM_HAND_L - 1].pos);
        Matrix_Rot16(mesh_rots[LM_HAND_L]);
    } else if (gun_type != LGT_UNARMED) {
        Matrix_TranslateRel32(bone[LM_UARM_R - 1].pos);

        const LARA_ARM *const arm = &lara_info->right_arm;
        const ANIM *const anim = Anim_GetAnim(arm->anim_num);
        mesh_rots = arm->frame_base[arm->frame_num].mesh_rots;
        Matrix_Rot16(mesh_rots[LM_UARM_R]);

        Matrix_TranslateRel32(bone[LM_LARM_R - 1].pos);
        Matrix_Rot16(mesh_rots[LM_LARM_R]);

        Matrix_TranslateRel32(bone[LM_HAND_R - 1].pos);
        Matrix_Rot16(mesh_rots[LM_HAND_R]);
    }

    Matrix_TranslateRel32(*vec);
    vec->x = lara_item->pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
    vec->y = lara_item->pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
    vec->z = lara_item->pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);
    Matrix_Pop();
}

void Lara_RefuseInteraction(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (!XYZ_32_AreEquivalent(
            lara_info->interact_target.initial_pos, lara_item->pos)) {
        lara_info->interact_target.initial_pos = lara_item->pos;
        Sound_Effect(SFX_LARA_NO, &lara_item->pos, SPM_ALWAYS);
    }
}

void Lara_TakeHit(ITEM *const lara_item, const int32_t dx, const int32_t dz)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const int16_t hit_angle = lara_item->rot.y + DEG_180 - Math_Atan(dz, dx);
    lara_info->hit_direction = Math_GetDirection(hit_angle);
    if (lara_info->hit_frame == 0) {
        Sound_Effect(
            g_TRVersion == 1 ? SFX_LARA_BODYSL : SFX_LARA_INJURY,
            &lara_item->pos, SPM_NORMAL);
    }
    lara_info->hit_frame++;
    lara_info->interact_target.is_moving = false;
    lara_info->interact_target.item_num = NO_ITEM;
    CLAMPG(lara_info->hit_frame, 34);
}

void Lara_TouchDeathSector(const GF_DEATH_TILE death_tile)
{
    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_item->hit_points < 0 || lara_info->water_status == LWS_CHEAT) {
        return;
    }

    int16_t room_num = lara_item->room_num;
    const XYZ_32 pos = { lara_item->pos.x, MAX_HEIGHT, lara_item->pos.z };
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    const int32_t height = Room_GetHeight(sector, pos);
    if (lara_item->floor != height) {
        return;
    }

    if (g_Config.debug.enable_invulnerability) {
        switch (death_tile) {
        case GF_DEATH_TILE_RAPIDS:
        case GF_DEATH_TILE_ELECTRIC:
            Lara_CatchFire();
            break;
        case GF_DEATH_TILE_LAVA:
            Lara_TouchLava();
            break;
        }
        return;
    }

    lara_item->hit_points = -1;
    lara_item->hit_status = true;

    switch (death_tile) {
    case GF_DEATH_TILE_RAPIDS:
        Lara_RapidsDrown();
        break;
    case GF_DEATH_TILE_ELECTRIC:
        lara_info->electric = 1;
        break;
    case GF_DEATH_TILE_LAVA:
        Lara_TouchLava();
        break;
    }
}

void Lara_TouchLava(void)
{
    if (g_TRVersion == 3) {
        Lara_CatchFire();
        return;
    }

    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->burn || lara_info->water_status != LWS_ABOVE_WATER) {
        return;
    }

    const OBJECT *const obj = Object_Get(O_FLAME);
    for (int32_t i = 0; i < 10; i++) {
        const int16_t effect_num = Effect_Create(lara_item->room_num);
        if (effect_num != NO_EFFECT) {
            EFFECT *const effect = Effect_Get(effect_num);
            effect->object_id = O_FLAME;
            effect->frame_num = obj->mesh_count * Random_GetControl() / 0x7FFF;
            effect->counter = -1 - 24 * Random_GetControl() / 0x7FFF;
        }
    }
    lara_info->burn = true;
}

void Lara_RapidsDrown(void)
{
    ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();

    Lara_SwitchToExtraState(LS_EXTRA_RAPIDS_DROWN);

    lara_item->gravity = false;
    lara_item->hit_points = -1;
    lara_item->hit_status = true;
    lara_item->fall_speed = 0;
    lara_item->speed = 0;

    lara_info->gun_type = LGT_UNARMED;
}

int32_t Lara_FloorFront(
    const ITEM *const item, const int16_t ang, const int32_t dist)
{
    XYZ_32 pos = item->pos;
    pos.y -= LARA_HEIGHT;
    pos = XYZ_32_OffsetYaw(pos, ang, dist);
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    int32_t height = Room_GetHeight(sector, pos);
    if (height != NO_HEIGHT) {
        height -= item->pos.y;
        if (height > 0
            && Room_GetPitSector(sector, pos.x, pos.z)->is_death_sector) {
            return STEP_L * 2;
        }
    }
    return height;
}

int32_t Lara_CeilingFront(
    const ITEM *const item, const int16_t ang, const int32_t dist,
    const int32_t item_height)
{
    const int32_t x = item->pos.x + ((dist * Math_Sin(ang)) >> W2V_SHIFT);
    const int32_t y = item->pos.y - item_height;
    const int32_t z = item->pos.z + ((dist * Math_Cos(ang)) >> W2V_SHIFT);
    const XYZ_32 pos = { x, y, z };
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    int32_t height = Room_GetCeiling(sector, pos);
    if (height != NO_HEIGHT) {
        height += item_height - item->pos.y;
    }
    return height;
}

void Lara_UpdateRoomToHeight(const int32_t height)
{
    ITEM *const lara_item = Lara_GetItem();
    XYZ_32 pos = lara_item->pos;
    pos.y += height;

    int16_t room_num = lara_item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    lara_item->floor = Room_GetHeight(sector, pos);

    const int16_t item_num = Item_GetIndex(lara_item);
    Item_UpdateRoom(item_num, room_num);
}

int32_t Lara_GetWaterDepth(
    const int32_t x, const int32_t y, const int32_t z, int16_t room_num)
{
    const ROOM *room = Room_Get(room_num);
    const SECTOR *sector;

    while (true) {
        int32_t z_sector = (z - room->pos.z) >> WALL_SHIFT;
        int32_t x_sector = (x - room->pos.x) >> WALL_SHIFT;

        if (z_sector <= 0) {
            z_sector = 0;
            if (x_sector < 1) {
                x_sector = 1;
            } else if (x_sector > room->size.x - 2) {
                x_sector = room->size.x - 2;
            }
        } else if (z_sector >= room->size.z - 1) {
            z_sector = room->size.z - 1;
            if (x_sector < 1) {
                x_sector = 1;
            } else if (x_sector > room->size.x - 2) {
                x_sector = room->size.x - 2;
            }
        } else if (x_sector < 0) {
            x_sector = 0;
        } else if (x_sector >= room->size.x) {
            x_sector = room->size.x - 1;
        }

        sector = Room_GetUnitSector(room, x_sector, z_sector);
        if (sector->portal_room.wall == NO_ROOM) {
            break;
        }
        room_num = sector->portal_room.wall;
        room = Room_Get(room_num);
    }

    if (room->flags.underwater || room->flags.swamp) {
        while (sector->portal_room.sky != NO_ROOM) {
            room = Room_Get(sector->portal_room.sky);
            if (!room->flags.underwater && !room->flags.swamp) {
                const XYZ_32 pos = { x, y, z };
                const int32_t water_height = Room_GetWaterHeight(pos, room_num);
                sector = Room_GetSector(pos, &room_num);
                return Room_GetHeight(sector, pos) - water_height;
            }
            sector = Room_GetWorldSector(room, x, z);
        }
        return 0x7FFF;
    }

    while (sector->portal_room.pit != NO_ROOM) {
        room = Room_Get(sector->portal_room.pit);
        if (room->flags.underwater || room->flags.swamp) {
            const XYZ_32 pos = { x, y, z };
            const int32_t water_height = Room_GetWaterHeight(pos, room_num);
            sector = Room_GetSector(pos, &room_num);
            return Room_GetHeight(sector, pos) - water_height;
        }
        sector = Room_GetWorldSector(room, x, z);
    }
    return NO_HEIGHT;
}

bool Lara_IsM16Active(void)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    ITEM *const lara_item = Lara_GetItem();
    if (lara->gun_item_num == NO_ITEM || lara_item->hit_points <= 0
        || (lara->gun_type != LGT_M16 && lara->gun_type != LGT_MP5)) {
        return false;
    }

    const ITEM *const item = Item_Get(lara->gun_item_num);
    return item->current_anim_state == 0 || item->current_anim_state == 2
        || item->current_anim_state == 4;
}

void Lara_CatchFire(void)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->burn || lara_info->water_status == LWS_CHEAT) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int16_t effect_num = Effect_Create(lara_item->room_num);
    if (effect_num == NO_EFFECT) {
        return;
    }

    EFFECT *const effect = Effect_Get(effect_num);
    if (g_TRVersion == 3) {
        // TR3 effects use Collide_GetJointAbsPosition but only every x frames,
        // which lets Lara briefly catch fire even if she touches liquids
        // (for example, when running into the boiling water in Tony's room).
        effect->pos = (XYZ_32) {};
    } else {
        effect->pos = lara_item->pos;
    }
    effect->frame_num = g_TRVersion == 3 ? FLAME_SMALL : 0;
    effect->object_id = O_FLAME;
    effect->counter = -1;
    lara_info->burn = true;
}

void Lara_Extinguish(void)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    lara_info->electric = 0;

    if (!lara_info->burn) {
        return;
    }

    lara_info->burn = false;

    // put out flame objects
    int16_t effect_num = Effect_GetActiveNum();
    while (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        const int16_t next_effect_num = effect->next_active;
        if (effect->object_id == O_FLAME && effect->counter < 0) {
            effect->counter = 0;
            Effect_Kill(effect_num);
        }
        effect_num = next_effect_num;
    }
}

bool Lara_HasState(const LARA_TRX_STATE *const test_arr)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->extra_anim) {
        return false;
    }

    const ITEM *const lara_item = Lara_GetItem();
    for (int32_t i = 0; test_arr[i] != LS_TRX_INVALID; i++) {
        if (test_arr[i] == LS_U(lara_item->current_anim_state)) {
            return true;
        }
    }
    return false;
}

bool Lara_HasExtraState(const LARA_EXTRA_STATE *const test_arr)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (!lara_info->extra_anim) {
        return false;
    }

    const ITEM *const lara_item = Lara_GetItem();
    for (int32_t i = 0; test_arr[i] != (LARA_EXTRA_STATE)-1; i++) {
        if (test_arr[i] == (LARA_EXTRA_STATE)lara_item->current_anim_state) {
            return true;
        }
    }
    return false;
}

void Lara_SwitchToExtraState(const LARA_EXTRA_STATE goal_state)
{
    ITEM *const lara_item = Lara_GetItem();
    Item_SwitchToObjAnim(lara_item, LS_EXTRA_BREATH, 0, O_LARA_EXTRA);
    lara_item->current_anim_state = LS_EXTRA_BREATH;
    lara_item->goal_anim_state = goal_state;
    Item_Animate(lara_item);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->gun_status = LGS_HANDS_BUSY;
    lara->hit_direction = DIR_UNKNOWN;
    lara->extra_anim = true;
}
