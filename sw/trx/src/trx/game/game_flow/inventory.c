#include <trx/game/game_flow/inventory.h>

#include <trx/config.h>
#include <trx/game/gun.h>
#include <trx/game/inventory.h>
#include <trx/game/objects.h>
#include <trx/game/overlay.h>
#include <trx/game/savegame.h>
#include <trx/game/stats.h>

static int8_t m_SecretInvItems[O_NUMBER_OF] = {};
static int8_t m_Add2InvItems[O_NUMBER_OF] = {};
static bool m_RemoveWeapons = false;
static bool m_RemoveAmmo = false;
static bool m_RemoveFlares = false;
static bool m_RemoveMedipacks = false;
static bool m_RemoveScions = false;

static bool M_CanHaveItem(const OBJECT_ID object_id)
{
    if (Object_IsType(object_id, g_GunObjects) && object_id != O_PISTOL_ITEM
        && g_Config.gameplay.disable_extra_guns) {
        return false;
    }
    if ((object_id == O_SMALL_MEDIPACK_ITEM
         || object_id == O_LARGE_MEDIPACK_ITEM)
        && g_Config.gameplay.disable_medpacks) {
        return false;
    }
    return true;
}

static bool M_ResumeInfo_HasWeapon(
    const RESUME_INFO *const resume, const LARA_GUN_TYPE gun_type)
{
    switch (gun_type) {
        // clang-format off
    case LGT_PISTOLS:      return resume->flags.has_pistols;
    case LGT_MAGNUMS:      return resume->flags.has_magnums;
    case LGT_AUTOS:        return resume->flags.has_autos;
    case LGT_DESERT_EAGLE: return resume->flags.has_desert_eagle;
    case LGT_UZIS:         return resume->flags.has_uzis;
    case LGT_SHOTGUN:      return resume->flags.has_shotgun;
    case LGT_HARPOON:      return resume->flags.has_harpoon;
    case LGT_M16:          return resume->flags.has_m16;
    case LGT_MP5:          return resume->flags.has_mp5;
    case LGT_GRENADE:      return resume->flags.has_grenade;
    case LGT_ROCKET:       return resume->flags.has_rocket;
    default: return false;
        // clang-format on
    }
}

static void M_ResumeInfo_SetWeapon(
    RESUME_INFO *const resume, const LARA_GUN_TYPE gun_type,
    const bool has_weapon)
{
    switch (gun_type) {
        // clang-format off
    case LGT_PISTOLS:      resume->flags.has_pistols = has_weapon; break;
    case LGT_MAGNUMS:      resume->flags.has_magnums = has_weapon; break;
    case LGT_AUTOS:        resume->flags.has_autos = has_weapon; break;
    case LGT_DESERT_EAGLE: resume->flags.has_desert_eagle = has_weapon; break;
    case LGT_UZIS:         resume->flags.has_uzis = has_weapon; break;
    case LGT_SHOTGUN:      resume->flags.has_shotgun = has_weapon; break;
    case LGT_HARPOON:      resume->flags.has_harpoon = has_weapon; break;
    case LGT_M16:          resume->flags.has_m16 = has_weapon; break;
    case LGT_MP5:          resume->flags.has_mp5 = has_weapon; break;
    case LGT_GRENADE:      resume->flags.has_grenade = has_weapon; break;
    case LGT_ROCKET:       resume->flags.has_rocket = has_weapon; break;
    default: break;
        // clang-format on
    }
}

static void M_ResumeInfo_AddAmmo(
    RESUME_INFO *const resume, const LARA_GUN_TYPE gun_type,
    const int32_t ammo_qty)
{
    switch (gun_type) {
        // clang-format off
    case LGT_MAGNUMS:      resume->magnum_ammo += ammo_qty; break;
    case LGT_AUTOS:        resume->autos_ammo += ammo_qty; break;
    case LGT_DESERT_EAGLE: resume->desert_eagle_ammo += ammo_qty; break;
    case LGT_UZIS:         resume->uzi_ammo += ammo_qty; break;
    case LGT_SHOTGUN:      resume->shotgun_ammo += ammo_qty; break;
    case LGT_HARPOON:      resume->harpoon_ammo += ammo_qty; break;
    case LGT_M16:          resume->m16_ammo += ammo_qty; break;
    case LGT_MP5:          resume->mp5_ammo += ammo_qty; break;
    case LGT_GRENADE:      resume->grenade_ammo += ammo_qty; break;
    case LGT_ROCKET:       resume->rocket_ammo += ammo_qty; break;
    default: break;
        // clang-format on
    }
}

static void M_ResumeInfo_AddItem(
    RESUME_INFO *const resume, const OBJECT_ID object_id, const int32_t qty)
{
    switch (object_id) {
    case O_SMALL_MEDIPACK_ITEM:
    case O_SMALL_MEDIPACK_OPTION:
        resume->small_medipacks += qty;
        break;
    case O_LARGE_MEDIPACK_ITEM:
    case O_LARGE_MEDIPACK_OPTION:
        resume->large_medipacks += qty;
        break;
    case O_FLAREBOX_ITEM:
    case O_FLAREBOX_OPTION:
    case O_FLARE_ITEM:
        resume->flares += qty;
        break;
    default:
        break;
    }
}

static void M_ModifyResumeInfo_GunOrAmmo(
    RESUME_INFO *const resume, const LARA_GUN_TYPE gun_type)
{
    const OBJECT_ID gun_object_id = Gun_GetGunObject(gun_type);
    const OBJECT_ID ammo_object_id = Gun_GetAmmoObject(gun_type);
    const int32_t ammo_qty = Gun_GetAmmoQuantity(gun_type);
    AMMO_INFO *const ammo_info = Gun_GetAmmoInfo(gun_type);

    if (!M_CanHaveItem(gun_object_id) || !M_CanHaveItem(ammo_object_id)) {
        return;
    }

    M_ResumeInfo_AddAmmo(
        resume, gun_type, ammo_qty * m_Add2InvItems[ammo_object_id]);
    if (!M_ResumeInfo_HasWeapon(resume, gun_type)
        && m_Add2InvItems[gun_object_id] > 0) {
        M_ResumeInfo_SetWeapon(resume, gun_type, true);
        M_ResumeInfo_AddAmmo(resume, gun_type, ammo_qty);
    }
}

static void M_ModifyResumeInfo_Item(
    RESUME_INFO *const resume, const OBJECT_ID object_id)
{
    if (!M_CanHaveItem(object_id)) {
        return;
    }

    M_ResumeInfo_AddItem(resume, object_id, m_Add2InvItems[object_id]);
}

static void M_CollectNewPickup(const OBJECT_ID object_id)
{
    Overlay_AddDisplayPickup(object_id);
    Stats_AddPickup();
}

static void M_ModifyInventory_GunOrAmmo(
    const GF_INV_TYPE type, const LARA_GUN_TYPE gun_type)
{
    const OBJECT_ID gun_object_id = Gun_GetGunObject(gun_type);
    const OBJECT_ID ammo_object_id = Gun_GetAmmoObject(gun_type);
    const int32_t ammo_qty = Gun_GetAmmoQuantity(gun_type);
    AMMO_INFO *const ammo_info = Gun_GetAmmoInfo(gun_type);

    if (!M_CanHaveItem(gun_object_id) || !M_CanHaveItem(ammo_object_id)) {
        return;
    }

    if (Inv_RequestItem(gun_object_id)) {
        if (type == GF_INV_SECRET) {
            // Convert already collected guns into ammo to maintain stats
            // accuracy.
            const int32_t count = m_SecretInvItems[ammo_object_id]
                + m_SecretInvItems[gun_object_id];
            ammo_info->ammo += ammo_qty * count;
            for (int32_t i = 0; i < count; i++) {
                M_CollectNewPickup(ammo_object_id);
            }
        } else if (type == GF_INV_REGULAR) {
            ammo_info->ammo += ammo_qty * m_Add2InvItems[ammo_object_id];
        }
    } else if (
        (type == GF_INV_REGULAR && m_Add2InvItems[gun_object_id] > 0)
        || (type == GF_INV_SECRET && m_SecretInvItems[gun_object_id] > 0)) {

        Inv_AddItem(gun_object_id);

        if (type == GF_INV_SECRET) {
            ammo_info->ammo += ammo_qty * m_SecretInvItems[ammo_object_id];
            M_CollectNewPickup(gun_object_id);
            for (int32_t i = 0; i < m_SecretInvItems[ammo_object_id]; i++) {
                M_CollectNewPickup(ammo_object_id);
            }
        } else if (type == GF_INV_REGULAR) {
            ammo_info->ammo += ammo_qty * m_Add2InvItems[ammo_object_id];
        }
    } else if (type == GF_INV_SECRET) {
        for (int32_t i = 0; i < m_SecretInvItems[ammo_object_id]; i++) {
            Inv_AddItem(ammo_object_id);
            M_CollectNewPickup(ammo_object_id);
        }
    } else if (type == GF_INV_REGULAR) {
        for (int32_t i = 0; i < m_Add2InvItems[ammo_object_id]; i++) {
            Inv_AddItem(ammo_object_id);
        }
    }
}

static void M_ModifyInventory_Item(
    const GF_INV_TYPE type, const OBJECT_ID object_id)
{
    int32_t qty = 0;
    if (type == GF_INV_SECRET) {
        qty = m_SecretInvItems[object_id];
    } else if (type == GF_INV_REGULAR) {
        qty = m_Add2InvItems[object_id];
    }

    // Check for gameplay mods from secret rewards
    if (!M_CanHaveItem(object_id)) {
        qty = 0;
    }

    for (int32_t i = 0; i < qty; i++) {
        if (Inv_AddItem(object_id) && type == GF_INV_SECRET) {
            M_CollectNewPickup(object_id);
        }
    }
}

void GF_InventoryModifier_Scan(const GF_LEVEL *const level)
{
    for (int32_t i = O_FIRST; i < O_NUMBER_OF; i++) {
        m_SecretInvItems[i] = 0;
        m_Add2InvItems[i] = 0;
    }
    m_RemoveWeapons = false;
    m_RemoveAmmo = false;
    m_RemoveFlares = false;
    m_RemoveMedipacks = false;
    m_RemoveScions = false;

    if (level == nullptr) {
        return;
    }
    for (int32_t i = 0; i < level->sequence.length; i++) {
        const GF_SEQUENCE_EVENT *const event = &level->sequence.events[i];
        if (event->type == GFS_ADD_ITEM
            || event->type == GFS_ADD_SECRET_REWARD) {
            const GF_ADD_ITEM_DATA *const data = event->data;
            if (data->object_id < O_FIRST || data->object_id >= O_NUMBER_OF) {
                continue;
            }
            if (data->inv_type == GF_INV_SECRET) {
                m_SecretInvItems[data->object_id] += data->quantity;
            } else if (data->inv_type == GF_INV_REGULAR) {
                m_Add2InvItems[data->object_id] += data->quantity;
            }
        } else if (event->type == GFS_REMOVE_WEAPONS) {
            m_RemoveWeapons = true;
        } else if (event->type == GFS_REMOVE_AMMO) {
            m_RemoveAmmo = true;
        } else if (event->type == GFS_REMOVE_FLARES) {
            m_RemoveFlares = true;
        } else if (event->type == GFS_REMOVE_MEDIPACKS) {
            m_RemoveMedipacks = true;
        } else if (event->type == GFS_REMOVE_SCIONS) {
            m_RemoveScions = true;
        }
    }
}

int32_t GF_GetSecretRewardCount(const GF_LEVEL *const level)
{
    int32_t sum = 0;
    if (level == nullptr) {
        return sum;
    }
    for (int32_t i = 0; i < level->sequence.length; i++) {
        const GF_SEQUENCE_EVENT *const event = &level->sequence.events[i];
        if (event->type == GFS_ADD_SECRET_REWARD) {
            const GF_ADD_ITEM_DATA *const data = event->data;
            if (data->inv_type == GF_INV_SECRET) {
                sum += data->quantity;
            }
        }
    }
    return sum;
}

void GF_InventoryModifier_ApplyToResumeInfo(const GF_LEVEL *const level)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);

    if (m_RemoveWeapons) {
        resume->flags.has_pistols = false;
        resume->flags.has_magnums = false;
        resume->flags.has_autos = false;
        resume->flags.has_desert_eagle = false;
        resume->flags.has_uzis = false;
        resume->flags.has_shotgun = false;
        resume->flags.has_m16 = false;
        resume->flags.has_mp5 = false;
        resume->flags.has_grenade = false;
        resume->flags.has_rocket = false;
        resume->flags.has_harpoon = false;
        resume->holsters_gun_type = LGT_UNARMED;
        resume->back_gun_type = LGT_UNARMED;
        resume->equipped_gun_type = LGT_UNARMED;
        resume->gun_status = LGS_ARMLESS;
    }

    if (!resume->flags.has_pistols && m_Add2InvItems[O_PISTOL_ITEM]) {
        resume->flags.has_pistols = true;
        if (resume->equipped_gun_type == LGT_UNARMED) {
            resume->equipped_gun_type = LGT_PISTOLS;
        }
    }

    if (m_RemoveAmmo) {
        resume->pistol_ammo = 0;
        resume->magnum_ammo = 0;
        resume->autos_ammo = 0;
        resume->desert_eagle_ammo = 0;
        resume->uzi_ammo = 0;
        resume->shotgun_ammo = 0;
        resume->m16_ammo = 0;
        resume->mp5_ammo = 0;
        resume->grenade_ammo = 0;
        resume->rocket_ammo = 0;
        resume->harpoon_ammo = 0;
    }

    if (m_RemoveScions) {
        resume->num_scions = 0;
        resume->num_quest_item_1 = 0;
        resume->num_quest_item_2 = 0;
        resume->num_quest_item_3 = 0;
        resume->num_quest_item_4 = 0;
        m_RemoveScions = false;
    }

    if (m_RemoveFlares) {
        resume->flares = 0;
        m_RemoveFlares = false;
    }

    if (m_RemoveMedipacks) {
        resume->large_medipacks = 0;
        resume->small_medipacks = 0;
        m_RemoveMedipacks = false;
    }

    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_PISTOLS);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_MAGNUMS);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_AUTOS);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_DESERT_EAGLE);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_UZIS);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_SHOTGUN);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_HARPOON);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_M16);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_MP5);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_GRENADE);
    M_ModifyResumeInfo_GunOrAmmo(resume, LGT_ROCKET);

    M_ModifyResumeInfo_Item(resume, O_SMALL_MEDIPACK_ITEM);
    M_ModifyResumeInfo_Item(resume, O_LARGE_MEDIPACK_ITEM);
    M_ModifyResumeInfo_Item(resume, O_FLARE_ITEM);
}

void GF_InventoryModifier_Apply(
    const GF_LEVEL *const level, const GF_INV_TYPE type)
{
    RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);

    // For GF_INV_REGULAR, we must ignore weapons, ammo, medpacks and flares,
    // as these are handled by RESUME_INFO and
    // GF_InventoryModifier_ApplyToResumeInfo and Lara_InitialiseInventory.

    if (type == GF_INV_SECRET) {
        if (m_Add2InvItems[O_PISTOL_ITEM]) {
            Inv_AddItem(O_PISTOL_ITEM);
            if (resume->equipped_gun_type == LGT_UNARMED) {
                resume->equipped_gun_type = LGT_PISTOLS;
            }
        }

        M_ModifyInventory_GunOrAmmo(type, LGT_MAGNUMS);
        M_ModifyInventory_GunOrAmmo(type, LGT_AUTOS);
        M_ModifyInventory_GunOrAmmo(type, LGT_DESERT_EAGLE);
        M_ModifyInventory_GunOrAmmo(type, LGT_UZIS);
        M_ModifyInventory_GunOrAmmo(type, LGT_SHOTGUN);
        M_ModifyInventory_GunOrAmmo(type, LGT_HARPOON);
        M_ModifyInventory_GunOrAmmo(type, LGT_M16);
        M_ModifyInventory_GunOrAmmo(type, LGT_MP5);
        M_ModifyInventory_GunOrAmmo(type, LGT_GRENADE);
        M_ModifyInventory_GunOrAmmo(type, LGT_ROCKET);
    }

    M_ModifyInventory_Item(type, O_PICKUP_ITEM_1);
    M_ModifyInventory_Item(type, O_PICKUP_ITEM_2);
    M_ModifyInventory_Item(type, O_PUZZLE_ITEM_1);
    M_ModifyInventory_Item(type, O_PUZZLE_ITEM_2);
    M_ModifyInventory_Item(type, O_PUZZLE_ITEM_3);
    M_ModifyInventory_Item(type, O_PUZZLE_ITEM_4);
    M_ModifyInventory_Item(type, O_KEY_ITEM_1);
    M_ModifyInventory_Item(type, O_KEY_ITEM_2);
    M_ModifyInventory_Item(type, O_KEY_ITEM_3);
    M_ModifyInventory_Item(type, O_KEY_ITEM_4);
    M_ModifyInventory_Item(type, O_LEADBAR_ITEM);
    M_ModifyInventory_Item(type, O_SCION_ITEM_1);
    M_ModifyInventory_Item(type, O_SCION_ITEM_2);

    if (type == GF_INV_SECRET) {
        M_ModifyInventory_Item(type, O_SMALL_MEDIPACK_ITEM);
        M_ModifyInventory_Item(type, O_LARGE_MEDIPACK_ITEM);
        M_ModifyInventory_Item(type, O_FLARE_ITEM);
    }
}
