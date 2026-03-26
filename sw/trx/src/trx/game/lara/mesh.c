#include <trx/game/lara/mesh.h>

#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/gun.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/game/savegame.h>

static OBJECT_MESH *m_Meshes[LM_NUMBER_OF] = {};

static LARA_GUN_TYPE M_DetermineHolsterGun(void)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->holsters_gun_type == LGT_UNARMED) {
        if (lara_info->gun_type != LGT_UNARMED
            && !Gun_IsRifleType(lara_info->gun_type)) {
            return lara_info->gun_type;
        } else if (Inv_RequestItem(O_PISTOL_ITEM)) {
            return LGT_PISTOLS;
        } else if (Inv_RequestItem(O_MAGNUM_ITEM)) {
            return LGT_MAGNUMS;
        } else if (Inv_RequestItem(O_AUTOS_ITEM)) {
            return LGT_AUTOS;
        } else if (Inv_RequestItem(O_DESERT_EAGLE_ITEM)) {
            return LGT_DESERT_EAGLE;
        } else if (Inv_RequestItem(O_UZI_ITEM)) {
            return LGT_UZIS;
        }
    }
    return lara_info->holsters_gun_type;
}

static LARA_GUN_TYPE M_DetermineBackGun(void)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->back_gun_type != LGT_UNARMED) {
        return lara_info->back_gun_type;
    }

    if (Inv_RequestItem(O_SHOTGUN_ITEM)) {
        return LGT_SHOTGUN;
    } else if (Inv_RequestItem(O_M16_ITEM)) {
        return LGT_M16;
    } else if (Inv_RequestItem(O_MP5_ITEM)) {
        return LGT_MP5;
    } else if (Inv_RequestItem(O_GRENADE_GUN_ITEM)) {
        return LGT_GRENADE;
    } else if (Inv_RequestItem(O_ROCKET_GUN_ITEM)) {
        return LGT_ROCKET;
    } else if (Inv_RequestItem(O_HARPOON_ITEM)) {
        return LGT_HARPOON;
    }
    return LGT_UNARMED;
}

static void M_InitialiseCutsceneLevel(void)
{
    Lara_Skin_SetGunEquipment(LM_THIGH_L, LGT_PISTOLS);
    Lara_Skin_SetGunEquipment(LM_THIGH_R, LGT_PISTOLS);
}

static void M_InitialiseNormalLevel(const GF_LEVEL *const level)
{
    const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);

    const LARA_GUN_TYPE holster_gun = M_DetermineHolsterGun();
    if (holster_gun != LGT_UNARMED && holster_gun != LGT_FLARE) {
        Gun_SetLaraHolsterLMesh(holster_gun);
        Gun_SetLaraHolsterRMesh(holster_gun);
    }

    const LARA_GUN_TYPE back_gun = M_DetermineBackGun();
    if (back_gun != LGT_UNARMED) {
        Gun_SetLaraBackMesh(back_gun);
    }

    if (resume != nullptr && resume->equipped_gun_type == LGT_FLARE) {
        Lara_Skin_SetGunEquipment(LM_HAND_L, LGT_FLARE);
    }
}

void Lara_Mesh_Initialise(const GF_LEVEL *const level)
{
    const OBJECT *const skin_obj = Object_Get(O_LARA_SKIN);
    if (skin_obj->loaded) {
        OBJECT *const lara_obj = Object_Get(O_LARA);
        lara_obj->mesh_idx = skin_obj->mesh_idx;
    }

    if (level->type == GFL_CUTSCENE) {
        M_InitialiseCutsceneLevel();
    } else {
        M_InitialiseNormalLevel(level);
    }
}

void Lara_Mesh_SwapSingle(const LARA_MESH mesh, const OBJECT_ID obj_id)
{
    const OBJECT *const obj = Object_Get(obj_id);
    Lara_Mesh_Set(mesh, Object_GetMesh(obj->mesh_idx + mesh));
}

void Lara_Mesh_SwapAll(const OBJECT_ID obj_id)
{
    if (!Object_Get(obj_id)->loaded) {
        return;
    }

    for (LARA_MESH mesh = LM_FIRST; mesh < LM_NUMBER_OF; mesh++) {
        Lara_Mesh_SwapSingle(mesh, obj_id);
    }
}

void Lara_Mesh_Set(const LARA_MESH mesh, OBJECT_MESH *const mesh_ptr)
{
    m_Meshes[mesh] = mesh_ptr;
}

OBJECT_MESH *Lara_Mesh_Get(const LARA_MESH mesh)
{
    return m_Meshes[mesh];
}

RGB_F Lara_GetMeshTint(const GAME_VECTOR pos)
{
    if (!g_Config.visuals.enable_responsive_mesh_tint || g_Camera.underwater) {
        return Output_GetTint();
    }

    int16_t room_num = pos.room_num;
    Room_GetSector(pos.pos, &room_num);
    const int32_t water_height = Room_GetWaterHeight(pos.pos, room_num);

    if (!Room_Get(room_num)->flags.underwater) {
        return COLOR_RGB_F_WHITE;
    } else if (water_height == NO_HEIGHT) {
        return Output_GetWaterColor();
    } else if (pos.y > water_height) {
        return Output_GetWaterColor();
    } else {
        return COLOR_RGB_F_WHITE;
    }
}

int32_t Lara_GetMeshIndex(const ITEM *const item, const int32_t mesh_idx)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    const int32_t fallback = obj->mesh_idx + mesh_idx;

    const OBJECT_MESH *const mesh = Lara_Mesh_Get(mesh_idx);
    if (mesh == nullptr) {
        return fallback;
    }

    const int32_t resolved = Object_GetMeshIndex(mesh);
    if (resolved < 0) {
        return fallback;
    }

    return resolved;
}
