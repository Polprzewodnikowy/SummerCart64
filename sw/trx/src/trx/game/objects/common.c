#include <trx/game/objects/common.h>

#include <trx/core/memory.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/anims.h>
#include <trx/game/catalog/manager.h>
#include <trx/game/const.h>
#include <trx/game/game_buf.h>
#include <trx/game/lara/common.h>
#include <trx/game/objects/vars.h>
#include <trx/game/output/common.h>

static OBJECT m_Objects[O_NUMBER_OF] = {};
static STATIC_OBJECT_3D *m_StaticObjects3D = nullptr;
static STATIC_OBJECT_2D *m_StaticObjects2D = nullptr;
static int32_t m_StaticObjects3DCount = 0;
static int32_t m_StaticObjects2DCount = 0;
static OBJECT_MESH **m_MeshPointers = nullptr;
static int32_t m_MeshCount = 0;

void Object_Reset(void)
{
    for (int32_t i = O_FIRST; i < O_NUMBER_OF; i++) {
        m_Objects[i].loaded = false;
    }

    m_StaticObjects3D = nullptr;
    m_StaticObjects2D = nullptr;
    m_StaticObjects3DCount = 0;
    m_StaticObjects2DCount = 0;
    m_MeshPointers = nullptr;
    m_MeshCount = 0;
}

void Object_InitialiseStaticObjects3D(const int32_t count)
{
    ASSERT(count >= 0);
    m_StaticObjects3DCount = count;
    m_StaticObjects3D =
        GameBuf_Alloc(sizeof(STATIC_OBJECT_3D) * count, GBUF_STATIC_OBJECTS_3D);
}

void Object_InitialiseStaticObjects2D(const int32_t count)
{
    ASSERT(count >= 0);
    m_StaticObjects2DCount = count;
    m_StaticObjects2D =
        GameBuf_Alloc(sizeof(STATIC_OBJECT_2D) * count, GBUF_STATIC_OBJECTS_2D);
}

int32_t Object_GetStaticObjects3DCount(void)
{
    return m_StaticObjects3DCount;
}

int32_t Object_GetStaticObjects2DCount(void)
{
    return m_StaticObjects2DCount;
}

OBJECT *Object_TryGet(const OBJECT_ID object_id)
{
    if (object_id < O_FIRST || object_id >= O_NUMBER_OF) {
        return nullptr;
    }
    return &m_Objects[object_id];
}

OBJECT *Object_Get(const OBJECT_ID object_id)
{
    ASSERT(object_id >= O_FIRST && object_id < O_NUMBER_OF);
    return &m_Objects[object_id];
}

OBJECT *Object_GetByGameID(const int32_t game_id)
{
    OBJECT_ID object_id = Object_FromGameID(game_id);
    if (object_id == NO_OBJECT) {
        return nullptr;
    }
    return &m_Objects[object_id];
}

STATIC_OBJECT_3D *Object_Get3DStatic(const int32_t static_id)
{
    return &m_StaticObjects3D[static_id];
}

bool Object_IsValidStatid3D(const int32_t static_id)
{
    return static_id >= 0 && static_id < m_StaticObjects3DCount;
}

STATIC_OBJECT_2D *Object_Get2DStatic(const int32_t static_id)
{
    if (m_StaticObjects2D == nullptr) {
        return nullptr;
    }
    if (static_id < 0 || static_id >= m_StaticObjects2DCount) {
        return nullptr;
    }
    return &m_StaticObjects2D[static_id];
}

OBJECT_ID Object_FromGameID(const int32_t game_id)
{
    int32_t out;
    if (Catalog_GameIDToEnum(CATALOG_OBJECTS, game_id, &out)) {
        return out;
    }
    return NO_OBJECT;
}

int32_t Object_ToGameID(const OBJECT_ID object_id)
{
    int32_t out;
    if (Catalog_EnumToGameID(CATALOG_OBJECTS, object_id, &out)) {
        return out;
    }
    return -1;
}

bool Object_IsType(const OBJECT_ID object_id, const OBJECT_ID *test_arr)
{
    for (int32_t i = 0; test_arr[i] != NO_OBJECT; i++) {
        if (test_arr[i] == object_id) {
            return true;
        }
    }
    return false;
}

OBJECT_ID Object_GetCognate(OBJECT_ID key_id, const GAME_OBJECT_PAIR *test_map)
{
    const GAME_OBJECT_PAIR *pair = &test_map[0];
    while (pair->key_id != NO_OBJECT) {
        if (pair->key_id == key_id) {
            return pair->value_id;
        }
        pair++;
    }

    return NO_OBJECT;
}

OBJECT_ID Object_GetCognateInverse(
    OBJECT_ID value_id, const GAME_OBJECT_PAIR *test_map)
{
    const GAME_OBJECT_PAIR *pair = &test_map[0];
    while (pair->key_id != NO_OBJECT) {
        if (pair->value_id == value_id) {
            return pair->key_id;
        }
        pair++;
    }

    return NO_OBJECT;
}

void Object_InitialiseMeshes(const int32_t mesh_count)
{
    m_MeshPointers =
        GameBuf_Alloc(sizeof(OBJECT_MESH *) * mesh_count, GBUF_MESH_POINTERS);
    m_MeshCount = 0;
}

void Object_StoreMesh(OBJECT_MESH *const mesh)
{
    m_MeshPointers[m_MeshCount] = mesh;
    m_MeshCount++;
}

OBJECT_MESH *Object_GetMesh(const int32_t index)
{
    return m_MeshPointers[index];
}

int32_t Object_GetItemMeshIndex(const ITEM *const item, const int32_t mesh_idx)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    const int32_t fallback = obj->mesh_idx + mesh_idx;

    if (obj->get_mesh_index_func == nullptr) {
        return fallback;
    }

    const int32_t resolved = obj->get_mesh_index_func(item, mesh_idx);
    if (resolved < 0) {
        return fallback;
    }

    return resolved;
}

int32_t Object_GetMeshIndex(const OBJECT_MESH *const mesh)
{
    for (int32_t i = 0; i < m_MeshCount; i++) {
        if (mesh == m_MeshPointers[i]) {
            return i;
        }
    }
    return -1;
}

int32_t Object_GetMeshCount(void)
{
    return m_MeshCount;
}

OBJECT_MESH *Object_FindMesh(const int32_t data_offset)
{
    for (int32_t i = 0; i < m_MeshCount; i++) {
        OBJECT_MESH *const mesh = m_MeshPointers[i];
        if (Object_GetMeshOffset(mesh) == data_offset) {
            return mesh;
        }
    }

    return nullptr;
}

int32_t Object_GetMeshOffset(const OBJECT_MESH *const mesh)
{
    return (int32_t)(intptr_t)mesh->priv;
}

void Object_SetMeshOffset(OBJECT_MESH *const mesh, const int32_t data_offset)
{
    mesh->priv = (void *)(intptr_t)data_offset;
}

void Object_SwapMesh(
    const OBJECT_ID object1_id, const OBJECT_ID object2_id,
    const int32_t mesh_num)
{
    Object_SwapMeshEx(object1_id, object2_id, mesh_num, mesh_num);
}

void Object_SwapAllMeshes(
    const OBJECT_ID object1_id, const OBJECT_ID object2_id)
{
    const OBJECT *const obj1 = Object_Get(object1_id);
    const OBJECT *const obj2 = Object_Get(object2_id);
    if (!obj1->loaded || !obj2->loaded) {
        return;
    }

    const int32_t mesh_count = MIN(obj1->mesh_count, obj2->mesh_count);
    for (int32_t i = 0; i < mesh_count; i++) {
        Object_SwapMeshEx(object1_id, object2_id, i, i);
    }
}

void Object_SwapMeshEx(
    const OBJECT_ID object1_id, const OBJECT_ID object2_id,
    const int32_t mesh_num1, const int32_t mesh_num2)
{
    const OBJECT *const obj1 = Object_Get(object1_id);
    const OBJECT *const obj2 = Object_Get(object2_id);
    if (!obj1->loaded || !obj2->loaded) {
        return;
    }

    const int32_t mesh_idx1 = obj1->mesh_idx + mesh_num1;
    const int32_t mesh_idx2 = obj2->mesh_idx + mesh_num2;
    SWAP(m_MeshPointers[mesh_idx1], m_MeshPointers[mesh_idx2]);

    Output_DispatchObjectMeshSwap(mesh_idx1, mesh_idx2);
}

ANIM *Object_GetAnim(const OBJECT *const obj, const int32_t anim_idx)
{
    return Anim_GetAnim(obj->anim_idx + anim_idx);
}

ANIM_BONE *Object_GetBone(const OBJECT *const obj, const int32_t bone_idx)
{
    return Anim_GetBone(obj->bone_idx + bone_idx);
}

ANIM_BONE *Object_TryGetBone(const OBJECT *const obj, const int32_t bone_idx)
{
    return Anim_TryGetBone(obj->bone_idx + bone_idx);
}

OBJECT_ID Object_FindReceptacleKey(const OBJECT_ID receptacle_obj_id)
{
    return Object_GetCognateInverse(
        receptacle_obj_id, g_KeyItemToReceptacleMap);
}

int16_t Object_FindReceptacle(const OBJECT_ID object_id)
{
    // Iterate through all matching receptacles
    const GAME_OBJECT_PAIR *const map = g_KeyItemToReceptacleMap;
    for (int32_t i = 0; map[i].key_id != NO_OBJECT; i++) {
        if (map[i].key_id != object_id) {
            continue;
        }

        // Iterate through all level items that match this receptacle
        const OBJECT_ID receptacle_to_check = map[i].value_id;
        for (int16_t item_num = 0; item_num < Item_GetLevelCount();
             item_num++) {
            const ITEM *const item = Item_Get(item_num);
            if (item->object_id != receptacle_to_check) {
                continue;
            }

            const OBJECT *const obj = Object_Get(item->object_id);
            if (obj->is_usable_func != nullptr
                && !obj->is_usable_func(item_num)) {
                continue;
            }

            // If Lara is standing near one, that's our keyhole
            if (Lara_TestPosition(item, obj->bounds_func())) {
                return item_num;
            }
        }
    }

    return NO_ITEM;
}

bool Object_CanInterpolate(
    const ITEM *const item, const int32_t frame_a, const int32_t frame_b)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    return item->enable_interpolation && obj->enable_interpolation
        && item->prev_frame_num != item->frame_num;
}

void Object_SetReflective(const OBJECT_ID obj_id, const bool enabled)
{
    const OBJECT *const obj = Object_Get(obj_id);
    for (int32_t i = 0; i < obj->mesh_count; i++) {
        Object_SetMeshReflective(obj_id, i, enabled);
    }
}

void Object_SetMeshReflective(
    const OBJECT_ID obj_id, const int32_t mesh_idx, const bool enabled)
{
    const OBJECT *const obj = Object_Get(obj_id);
    if (!obj->loaded) {
        return;
    }

    Object_SetMeshReflectiveEx(obj->mesh_idx + mesh_idx, enabled);
}

void Object_SetMeshReflectiveEx(const int32_t abs_mesh_idx, const bool enabled)
{
    OBJECT_MESH *const mesh = Object_GetMesh(abs_mesh_idx);
    mesh->enable_reflections = enabled;
    for (int32_t i = 0; i < mesh->all_faces.count; i++) {
        mesh->all_faces.data[i].enable_reflections = enabled;
    }
    Output_DispatchObjectMeshUpdate(abs_mesh_idx);
}
