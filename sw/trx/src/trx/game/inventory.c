#include <trx/game/inventory.h>

#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/game.h>
#include <trx/game/gun.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/lara.h>
#include <trx/game/objects/vars.h>
#include <trx/game/stats.h>

INVENTORY_MODE g_Inv_Mode = INV_TITLE_MODE;

static int32_t M_GetFlareQuantity(void)
{
    return Game_IsBonusFlagSet(GBF_JAPANESE)
        ? g_Weapons[LGT_FLARE].pickup_qty_alt
        : g_Weapons[LGT_FLARE].pickup_qty;
}

static INVENTORY_ITEM *M_GetGunInvItem(const LARA_GUN_TYPE gun_type)
{
    // clang-format off
    switch (gun_type) {
    case LGT_PISTOLS:      return InvRing_GetByObjectID(O_PISTOL_OPTION);
    case LGT_SHOTGUN:      return InvRing_GetByObjectID(O_SHOTGUN_OPTION);
    case LGT_MAGNUMS:      return InvRing_GetByObjectID(O_MAGNUM_OPTION);
    case LGT_AUTOS:        return InvRing_GetByObjectID(O_AUTOS_OPTION);
    case LGT_DESERT_EAGLE: return InvRing_GetByObjectID(O_DESERT_EAGLE_OPTION);
    case LGT_UZIS:         return InvRing_GetByObjectID(O_UZI_OPTION);
    case LGT_HARPOON:      return InvRing_GetByObjectID(O_HARPOON_OPTION);
    case LGT_M16:          return InvRing_GetByObjectID(O_M16_OPTION);
    case LGT_MP5:          return InvRing_GetByObjectID(O_MP5_OPTION);
    case LGT_GRENADE:      return InvRing_GetByObjectID(O_GRENADE_GUN_OPTION);
    case LGT_ROCKET:       return InvRing_GetByObjectID(O_ROCKET_GUN_OPTION);
    default:               return nullptr;
    }
    // clang-format on
}

static INVENTORY_ITEM *M_GetAmmoInvItem(const LARA_GUN_TYPE gun_type)
{
    // clang-format off
    switch (gun_type) {
    case LGT_PISTOLS:      return InvRing_GetByObjectID(O_PISTOL_AMMO_OPTION);
    case LGT_SHOTGUN:      return InvRing_GetByObjectID(O_SHOTGUN_AMMO_OPTION);
    case LGT_MAGNUMS:      return InvRing_GetByObjectID(O_MAGNUM_AMMO_OPTION);
    case LGT_AUTOS:        return InvRing_GetByObjectID(O_AUTOS_AMMO_OPTION);
    case LGT_DESERT_EAGLE: return InvRing_GetByObjectID(O_DESERT_EAGLE_AMMO_OPTION);
    case LGT_UZIS:         return InvRing_GetByObjectID(O_UZI_AMMO_OPTION);
    case LGT_HARPOON:      return InvRing_GetByObjectID(O_HARPOON_AMMO_OPTION);
    case LGT_M16:          return InvRing_GetByObjectID(O_M16_AMMO_OPTION);
    case LGT_MP5:          return InvRing_GetByObjectID(O_MP5_AMMO_OPTION);
    case LGT_GRENADE:      return InvRing_GetByObjectID(O_GRENADE_AMMO_OPTION);
    case LGT_ROCKET:       return InvRing_GetByObjectID(O_ROCKET_AMMO_OPTION);
    default:               return nullptr;
    }
    // clang-format on
}

static void M_IncreaseAmmo(const LARA_GUN_TYPE gun_type, const int32_t qty)
{
    AMMO_INFO *const ammo = Gun_GetAmmoInfo(gun_type);
    ammo->ammo += qty;
    CLAMPG(ammo->ammo, MAX_QTY);
}

static RING_TYPE M_GetRingType(const INVENTORY_ITEM *const inv_item)
{
    if (inv_item->inv_pos < 100) {
        return RT_MAIN;
    } else if (inv_item->inv_pos < 200) {
        return RT_KEYS;
    } else if (inv_item->inv_pos < 300) {
        return RT_OPTION;
    } else {
        return RT_GLOBE_SELECT;
    }
}

static void M_AddGun(const LARA_GUN_TYPE gun_type)
{
    const OBJECT_ID gun_object = Gun_GetGunObject(gun_type);
    const OBJECT_ID ammo_object = Gun_GetAmmoObject(gun_type);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const int32_t ammo_qty = Gun_GetAmmoQuantity(gun_type);
    for (int32_t i = Inv_RequestItem(ammo_object); i > 0; i--) {
        Inv_RemoveItem(ammo_object);
        M_IncreaseAmmo(gun_type, ammo_qty);
    }
    M_IncreaseAmmo(gun_type, ammo_qty);
    Inv_InsertItem(M_GetGunInvItem(gun_type));
    if (lara->last_gun_type == LGT_UNARMED) {
        lara->last_gun_type = gun_type;
    }
    Item_GlobalReplace(gun_object, ammo_object);
}

static void M_AddAmmo(const LARA_GUN_TYPE gun_type)
{
    const OBJECT_ID gun_object = Gun_GetGunObject(gun_type);
    const int32_t ammo_qty = Gun_GetAmmoQuantity(gun_type);
    if (Inv_RequestItem(gun_object)) {
        M_IncreaseAmmo(gun_type, ammo_qty);
    } else {
        Inv_InsertItem(M_GetAmmoInvItem(gun_type));
    }
}

bool Inv_AddItemNTimes(const OBJECT_ID object_id, const int32_t qty)
{
    bool result = false;
    for (int32_t i = 0; i < qty; i++) {
        result |= Inv_AddItem(object_id);
    }
    return result;
}

OBJECT_ID Inv_GetItemOption(const OBJECT_ID object_id)
{
    if (Object_IsType(object_id, g_InvObjects)) {
        return object_id;
    }
    return Object_GetCognate(object_id, g_ItemToInvObjectMap);
}

OBJECT_ID Inv_GetItemPickup(const OBJECT_ID object_id)
{
    if (Object_IsType(object_id, g_InvObjects)) {
        return Object_GetCognateInverse(object_id, g_ItemToInvObjectMap);
    }
    return object_id;
}

void Inv_InsertItem(INVENTORY_ITEM *const inv_item)
{
    Inv_InsertItemEx(inv_item, 1);
}

void Inv_InsertItemEx(INVENTORY_ITEM *const inv_item, const int32_t qty)
{
    ASSERT(inv_item != nullptr);
    INV_RING_SOURCE *const source = &g_InvRing_Source[M_GetRingType(inv_item)];

    int32_t n;
    for (n = 0; n < source->count; n++) {
        if (source->items[n]->inv_pos > inv_item->inv_pos) {
            break;
        }
    }

    for (int32_t i = source->count; i > n - 1; i--) {
        source->items[i + 1] = source->items[i];
        source->qtys[i + 1] = source->qtys[i];
    }
    source->items[n] = inv_item;
    source->qtys[n] = MIN(qty, MAX_QTY);
    source->count++;
}

bool Inv_RemoveItem(const OBJECT_ID object_id)
{
    const OBJECT_ID inv_object_id = Inv_GetItemOption(object_id);
    for (RING_TYPE ring_type = 0; ring_type < RT_NUMBER_OF; ring_type++) {
        INV_RING_SOURCE *const source = &g_InvRing_Source[ring_type];
        for (int32_t i = 0; i < source->count; i++) {
            if (source->items[i]->object_id != inv_object_id) {
                continue;
            }

            source->qtys[i]--;

            if (g_Config.gameplay.fix_item_duplication_glitch) {
                for (int32_t j = i; j < source->count; j++) {
                    if (j == source->current) {
                        source->current = 0;
                    }
                }
            }

            if (source->qtys[i] == 0) {
                source->count--;
                for (int32_t j = i; j < source->count; j++) {
                    source->items[j] = source->items[j + 1];
                    source->qtys[j] = source->qtys[j + 1];
                }
            }
            return true;
        }
    }
    return false;
}

int32_t Inv_RequestItem(const OBJECT_ID object_id)
{
    const OBJECT_ID inv_object_id = Inv_GetItemOption(object_id);
    for (RING_TYPE ring_type = 0; ring_type < RT_NUMBER_OF; ring_type++) {
        INV_RING_SOURCE *const source = &g_InvRing_Source[ring_type];
        for (int32_t i = 0; i < source->count; i++) {
            if (source->items[i] != nullptr
                && source->items[i]->object_id == inv_object_id) {
                return source->qtys[i];
            }
        }
    }
    return 0;
}

void Inv_ClearSelection(void)
{
    g_InvRing_Source[RT_MAIN].current = 0;
    g_InvRing_Source[RT_KEYS].current = 0;
}

void Inv_RemoveAllItems(void)
{
    g_InvRing_Source[RT_MAIN].count = 0;
    g_InvRing_Source[RT_KEYS].count = 0;
    g_InvRing_Source[RT_GLOBE_SELECT].count = 0;

    // Reset main ring
    Inv_AddItem(O_STOPWATCH_OPTION);
    Inv_AddItem(O_COMPASS_OPTION);
    Inv_AddItem(O_GLOBE_SELECT_OPTION);

    Inv_ClearSelection();
}

bool Inv_AddItem(const OBJECT_ID object_id)
{
    const OBJECT_ID inv_object_id = Inv_GetItemOption(object_id);
    const OBJECT_ID pickup_object_id = Inv_GetItemPickup(object_id);
    const OBJECT *const object =
        Object_Get(inv_object_id == NO_OBJECT ? object_id : inv_object_id);
    if (!object->loaded) {
        return false;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (Object_IsType(pickup_object_id, g_GunObjects)) {
        Gun_UpdateLaraMeshes(pickup_object_id);
        if (lara->gun_type == LGT_UNARMED) {
            lara->gun_type = Gun_GetType(pickup_object_id);
            const bool hands_busy = lara->gun_status == LGS_HANDS_BUSY;
            lara->gun_status = LGS_ARMLESS;
            Gun_InitialiseNewWeapon();
            if (hands_busy) {
                lara->gun_status = LGS_HANDS_BUSY;
            }
        }
    }

    const int32_t qty = object_id == O_FLAREBOX_ITEM ? M_GetFlareQuantity() : 1;
    for (RING_TYPE ring_type = 0; ring_type < RT_NUMBER_OF; ring_type++) {
        INV_RING_SOURCE *const source = &g_InvRing_Source[ring_type];
        for (int32_t i = 0; i < source->count; i++) {
            if (source->items[i]->object_id == inv_object_id) {
                source->qtys[i] += qty;
                CLAMPG(source->qtys[i], MAX_QTY);
                return true;
            }
        }
    }

    // Pistols
    if (inv_object_id == O_PISTOL_OPTION) {
        Inv_InsertItem(InvRing_GetByObjectID(O_PISTOL_OPTION));
        if (lara->last_gun_type == LGT_UNARMED) {
            lara->last_gun_type = LGT_PISTOLS;
        }
        return true;
    }

    // Other guns
    if (Object_IsType(pickup_object_id, g_GunObjects)) {
        M_AddGun(Gun_GetType(pickup_object_id));
        return true;
    }
    if (Object_IsType(pickup_object_id, g_GunAmmoObjects)) {
        M_AddAmmo(Gun_GetType(
            Object_GetCognateInverse(pickup_object_id, g_GunAmmoObjectMap)));
        return true;
    }

    // Other cases
    for (int32_t i = 0; i < g_InvRing_Items->count; i++) {
        INVENTORY_ITEM *const inv_item =
            *(INVENTORY_ITEM **)Vector_Get(g_InvRing_Items, i);
        if (inv_item->object_id == object_id
            || inv_item->object_id == inv_object_id) {
            Inv_InsertItemEx(inv_item, qty);
            return true;
        }
    }
    return false;
}

bool Inv_AddPickup(const ITEM *const item)
{
    if (Object_IsType(item->object_id, g_SecretObjects)) {
        Stats_MarkSecretCollected(item);
        if (Stats_CheckAllLevelSecretsPickedUp()) {
            GF_InventoryModifier_Apply(Game_GetCurrentLevel(), GF_INV_SECRET);
        }
        return true;
    }

    return Inv_AddItem(item->object_id);
}
