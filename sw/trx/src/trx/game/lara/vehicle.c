#include <trx/game/lara/vehicle.h>

#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/lara/skin/common.h>

static int16_t m_VehicleItemNum = NO_ITEM;

bool Lara_Vehicle_IsMounted(void)
{
    return m_VehicleItemNum != NO_ITEM;
}

bool Lara_Vehicle_IsOnType(const OBJECT_ID obj_id)
{
    if (!Lara_Vehicle_IsMounted()) {
        return false;
    }

    const ITEM *const vehicle = Lara_Vehicle_GetItem();
    return vehicle->object_id == obj_id;
}

void Lara_Vehicle_SetIndex(const int16_t item_num)
{
    m_VehicleItemNum = item_num;
}

int16_t Lara_Vehicle_GetIndex(void)
{
    return m_VehicleItemNum;
}

ITEM *Lara_Vehicle_GetItem(void)
{
    return m_VehicleItemNum == NO_ITEM ? nullptr : Item_Get(m_VehicleItemNum);
}

void Lara_Vehicle_Dismount(void)
{
    if (!Lara_Vehicle_IsMounted()) {
        return;
    }

    ITEM *const lara_item = Lara_GetItem();
    ITEM *const vehicle = Lara_Vehicle_GetItem();
    Item_SwitchToAnim(vehicle, 0, 0);
    Lara_Vehicle_SetIndex(NO_ITEM);

    lara_item->current_anim_state = LS(LS_STOP);
    lara_item->goal_anim_state = LS(LS_STOP);
    Item_SwitchToAnim(lara_item, LA(LA_STAND_STILL), 0);

    lara_item->rot.x = 0;
    lara_item->rot.z = 0;

    const LARA_SKIN_EQUIPMENT *const hand_r_equipment =
        Lara_Skin_GetEquipment(LM_HAND_R);
    if (hand_r_equipment->type == EQUIPMENT_TYPE_EXTRA
        && hand_r_equipment->data == EXTRA_MESH_OAR) {
        Lara_Skin_ClearEquipment(LM_HAND_R);
        Item_SetMeshVisibleMask(lara_item, INT32_MAX, true);
    }
}
