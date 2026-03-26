#include <trx/config.h>
#include <trx/core/colors.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/gun.h>
#include <trx/game/lara.h>
#include <trx/game/output.h>
#include <trx/version.h>

#define M_SHOTGUN_AMMO_CLIP 6

static bool M_IsGunType(
    const LARA_GUN_TYPE gun_type, const WEAPON_TYPE weapon_type)
{
    return g_Weapons[gun_type].type == weapon_type;
}

void Gun_AddDynamicLight(void)
{
    if (!g_Config.visuals.enable_gun_lighting) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t c = Math_Cos(lara_item->rot.y);
    const int32_t s = Math_Sin(lara_item->rot.y);
    const XYZ_32 pos = {
        .x = lara_item->pos.x + (s >> (W2V_SHIFT - 10)),
        .y = lara_item->pos.y - WALL_L / 2,
        .z = lara_item->pos.z + (c >> (W2V_SHIFT - 10)),
    };
    if (g_TRVersion >= 3) {
        Output_AddDynamicLightRGB(pos, 12, (RGB_888) { 192, 144, 0 });
    } else {
        Output_AddDynamicLight(pos, 12, 11);
    }
}

OBJECT_ID Gun_GetLaraAnim(const LARA_GUN_TYPE gun_type)
{
    return M_IsGunType(gun_type, WEAPON_TYPE_DUAL_PISTOLS)
        ? O_LARA_PISTOLS
        : Gun_GetWeaponAnim(gun_type);
}

OBJECT_ID Gun_GetWeaponAnim(const LARA_GUN_TYPE gun_type)
{
    // clang-format off
    switch (gun_type) {
    case LGT_UNKNOWN:      return O_LARA;
    case LGT_UNARMED:      return O_LARA;
    case LGT_PISTOLS:      return O_LARA_PISTOLS;
    case LGT_MAGNUMS:      return O_LARA_MAGNUMS;
    case LGT_AUTOS:        return O_LARA_AUTOS;
    case LGT_DESERT_EAGLE: return O_LARA_DESERT_EAGLE;
    case LGT_UZIS:         return O_LARA_UZIS;
    case LGT_SHOTGUN:      return O_LARA_SHOTGUN;
    case LGT_M16:          return O_LARA_M16;
    case LGT_MP5:          return O_LARA_MP5;
    case LGT_GRENADE:      return O_LARA_GRENADE_GUN;
    case LGT_ROCKET:       return O_LARA_ROCKET_GUN;
    case LGT_HARPOON:      return O_LARA_HARPOON_GUN;
    case LGT_FLARE:        return O_LARA_FLARE;
    default:               return NO_OBJECT;
    }
    // clang-format on
}

LARA_GUN_TYPE Gun_GetType(const OBJECT_ID obj_id)
{
    // clang-format off
    switch (obj_id) {
    case O_PISTOL_ITEM:       return LGT_PISTOLS;
    case O_MAGNUM_ITEM:       return LGT_MAGNUMS;
    case O_AUTOS_ITEM:        return LGT_AUTOS;
    case O_DESERT_EAGLE_ITEM: return LGT_DESERT_EAGLE;
    case O_UZI_ITEM:          return LGT_UZIS;
    case O_SHOTGUN_ITEM:      return LGT_SHOTGUN;
    case O_HARPOON_ITEM:      return LGT_HARPOON;
    case O_M16_ITEM:          return LGT_M16;
    case O_MP5_ITEM:          return LGT_MP5;
    case O_GRENADE_GUN_ITEM:  return LGT_GRENADE;
    case O_ROCKET_GUN_ITEM:   return LGT_ROCKET;
    default:                  return LGT_UNARMED;
    }
    // clang-format on
}

OBJECT_ID Gun_GetGunObject(const LARA_GUN_TYPE gun_type)
{
    // clang-format off
    switch (gun_type) {
    case LGT_PISTOLS:      return O_PISTOL_ITEM;
    case LGT_MAGNUMS:      return O_MAGNUM_ITEM;
    case LGT_AUTOS:        return O_AUTOS_ITEM;
    case LGT_DESERT_EAGLE: return O_DESERT_EAGLE_ITEM;
    case LGT_UZIS:         return O_UZI_ITEM;
    case LGT_SHOTGUN:      return O_SHOTGUN_ITEM;
    case LGT_HARPOON:      return O_HARPOON_ITEM;
    case LGT_M16:          return O_M16_ITEM;
    case LGT_MP5:          return O_MP5_ITEM;
    case LGT_GRENADE:      return O_GRENADE_GUN_ITEM;
    case LGT_ROCKET:       return O_ROCKET_GUN_ITEM;
    default:               return NO_OBJECT;
    }
    // clang-format on
}

OBJECT_ID Gun_GetAmmoObject(const LARA_GUN_TYPE gun_type)
{
    // clang-format off
    switch (gun_type) {
    case LGT_PISTOLS:      return O_PISTOL_AMMO_ITEM;
    case LGT_MAGNUMS:      return O_MAGNUM_AMMO_ITEM;
    case LGT_AUTOS:        return O_AUTOS_AMMO_ITEM;
    case LGT_DESERT_EAGLE: return O_DESERT_EAGLE_AMMO_ITEM;
    case LGT_UZIS:         return O_UZI_AMMO_ITEM;
    case LGT_SHOTGUN:      return O_SHOTGUN_AMMO_ITEM;
    case LGT_HARPOON:      return O_HARPOON_AMMO_ITEM;
    case LGT_M16:          return O_M16_AMMO_ITEM;
    case LGT_MP5:          return O_MP5_AMMO_ITEM;
    case LGT_GRENADE:      return O_GRENADE_AMMO_ITEM;
    case LGT_ROCKET:       return O_ROCKET_AMMO_ITEM;
    default:               return NO_OBJECT;
    }
    // clang-format on
}

int32_t Gun_GetAmmoQuantity(const LARA_GUN_TYPE gun_type)
{
    return MAX(1, g_Weapons[gun_type].pickup_qty)
        * Gun_GetAmmoClipCount(gun_type);
}

int32_t Gun_GetAmmoClipCount(const LARA_GUN_TYPE gun_type)
{
    return gun_type == LGT_SHOTGUN ? M_SHOTGUN_AMMO_CLIP : 1;
}

int32_t Gun_GetAmmoShellCount(const LARA_GUN_TYPE gun_type)
{
    return Gun_GetAmmoQuantity(gun_type) / Gun_GetAmmoClipCount(gun_type);
}

AMMO_INFO *Gun_GetAmmoInfo(const LARA_GUN_TYPE gun_type)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info == nullptr) {
        return nullptr;
    }
    // clang-format off
    switch (gun_type) {
    case LGT_PISTOLS:      return &lara_info->pistol_ammo;
    case LGT_MAGNUMS:      return &lara_info->magnum_ammo;
    case LGT_AUTOS:        return &lara_info->autos_ammo;
    case LGT_DESERT_EAGLE: return &lara_info->desert_eagle_ammo;
    case LGT_UZIS:         return &lara_info->uzi_ammo;
    case LGT_SHOTGUN:      return &lara_info->shotgun_ammo;
    case LGT_HARPOON:      return &lara_info->harpoon_ammo;
    case LGT_M16:          return &lara_info->m16_ammo;
    case LGT_MP5:          return &lara_info->mp5_ammo;
    case LGT_GRENADE:      return &lara_info->grenade_ammo;
    case LGT_ROCKET:       return &lara_info->rocket_ammo;
    case LGT_SKIDOO:       return &lara_info->pistol_ammo;
    default:               return nullptr;
    }
    // clang-format on
}

bool Gun_IsRifleType(const LARA_GUN_TYPE gun_type)
{
    return M_IsGunType(gun_type, WEAPON_TYPE_RIFLE);
}

void Gun_SetLaraHandLMesh(const LARA_GUN_TYPE weapon_type)
{
    Lara_Skin_SetGunEquipment(LM_HAND_L, weapon_type);
}

void Gun_SetLaraHandRMesh(const LARA_GUN_TYPE weapon_type)
{
    Lara_Skin_SetGunEquipment(LM_HAND_R, weapon_type);
}

void Gun_SetLaraBackMesh(const LARA_GUN_TYPE weapon_type)
{
    Lara_Skin_SetGunEquipment(LM_TORSO, weapon_type);
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    lara_info->back_gun_type = weapon_type;
}

void Gun_SetLaraHolsterLMesh(const LARA_GUN_TYPE weapon_type)
{
    Lara_Skin_SetGunEquipment(LM_THIGH_L, weapon_type);
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    lara_info->holsters_gun_type = weapon_type;
}

void Gun_SetLaraHolsterRMesh(const LARA_GUN_TYPE weapon_type)
{
    Lara_Skin_SetGunEquipment(LM_THIGH_R, weapon_type);
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    lara_info->holsters_gun_type = weapon_type;
}
