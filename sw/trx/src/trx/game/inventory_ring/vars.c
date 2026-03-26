#include <trx/game/inventory_ring/vars.h>

#include <trx/core/json/util/file.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/catalog/manager.h>
#include <trx/game/shell.h>

CAMERA_INFO g_InvRing_OldCamera = {};
VECTOR *g_InvRing_Items = nullptr;
INV_RING_SOURCE g_InvRing_Source[RT_NUMBER_OF] = {};

__attribute__((constructor)) static void M_Init(void)
{
    g_InvRing_Items = Vector_Create(sizeof(INVENTORY_ITEM *));
}

__attribute__((destructor)) static void M_Shutdown(void)
{
    for (int32_t i = 0; i < g_InvRing_Items->count; i++) {
        INVENTORY_ITEM *const item =
            *(INVENTORY_ITEM **)Vector_Get(g_InvRing_Items, i);
        Memory_Free(item);
    }
    Vector_Free(g_InvRing_Items);
    g_InvRing_Items = nullptr;
}

void InvRing_LoadVars(const char *const path)
{
#define L_READ_INT(key, target) target = JSON_ObjectGetInt(obj, key, target);

    for (int32_t i = 0; i < g_InvRing_Items->count; i++) {
        INVENTORY_ITEM *const item =
            *(INVENTORY_ITEM **)Vector_Get(g_InvRing_Items, i);
        Memory_Free(item);
    }
    Vector_Clear(g_InvRing_Items);
    for (int32_t i = 0; i < RT_NUMBER_OF; i++) {
        g_InvRing_Source[i].count = 0;
    }

    JSON_VALUE *const root = JSONFile_ReadEx(path, true);
    JSON_ARRAY *const arr = JSON_ValueAsArray(root);
    if (arr == nullptr) {
        Shell_ExitSystemFmt("invalid inventory ring vars file: %s", path);
    }
    ASSERT(g_InvRing_Items != nullptr);

    for (size_t i = 0; i < arr->length; i++) {
        JSON_OBJECT *const obj = JSON_ArrayGetObject(arr, i);
        const char *const name =
            JSON_ObjectGetString(obj, "object_id", JSON_INVALID_STRING);
        CATALOG_ID id;
        if (!Catalog_NameToEnum(CATALOG_OBJECTS, name, &id)) {
            Shell_ExitSystemFmt("unknown object_id '%s' in %s", name, path);
        }
        INVENTORY_ITEM *const item = Memory_Alloc(sizeof(*item));
        item->object_id = id;
        L_READ_INT("frames_total", item->frames_total);
        L_READ_INT("current_frame", item->current_frame);
        L_READ_INT("goal_frame", item->goal_frame);
        L_READ_INT("open_frame", item->open_frame);
        L_READ_INT("anim_direction", item->anim_direction);
        L_READ_INT("anim_speed", item->anim_speed);
        L_READ_INT("anim_count", item->anim_count);
        L_READ_INT("x_rot_pt_sel", item->x_rot_pt_sel);
        L_READ_INT("x_rot_pt", item->x_rot_pt);
        L_READ_INT("x_rot_sel", item->x_rot_sel);
        L_READ_INT("x_rot_nosel", item->x_rot_nosel);
        L_READ_INT("x_rot", item->x_rot);
        L_READ_INT("y_rot_sel", item->y_rot_sel);
        L_READ_INT("y_rot", item->y_rot);
        L_READ_INT("y_trans_sel", item->y_trans_sel);
        L_READ_INT("y_trans", item->y_trans);
        L_READ_INT("z_trans_sel", item->z_trans_sel);
        L_READ_INT("z_trans", item->z_trans);
        L_READ_INT("meshes_sel", item->meshes_sel);
        L_READ_INT("meshes_drawn", item->meshes_drawn);
        L_READ_INT("inv_pos", item->inv_pos);
        Vector_Add(g_InvRing_Items, &item);
    }

    JSON_ValueFree(root);
#undef L_READ_INT
}
