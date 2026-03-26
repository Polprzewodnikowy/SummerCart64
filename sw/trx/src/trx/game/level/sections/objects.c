#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/game/const.h>
#include <trx/game/inject.h>
#include <trx/game/level/format/format.h>
#include <trx/game/level/sections/read.h>
#include <trx/game/objects.h>
#include <trx/game/shell.h>

static void M_ReadPosition(XYZ_32 *const pos, VFILE *const file)
{
    pos->x = VFile_ReadS32(file);
    pos->y = VFile_ReadS32(file);
    pos->z = VFile_ReadS32(file);
}

static void M_ReadShade(
    const LEVEL_FORMAT_LOADER *const loader, SHADE *const shade,
    VFILE *const file)
{
    shade->value_1 = VFile_ReadS16(file);
    if (loader->game_version == 1) {
        shade->value_2 = shade->value_1;
    } else {
        shade->value_2 = VFile_ReadS16(file);
    }
}

static void M_ReadBounds16(BOUNDS_16 *const bounds, VFILE *const file)
{
    bounds->min.x = VFile_ReadS16(file);
    bounds->max.x = VFile_ReadS16(file);
    bounds->min.y = VFile_ReadS16(file);
    bounds->max.y = VFile_ReadS16(file);
    bounds->min.z = VFile_ReadS16(file);
    bounds->max.z = VFile_ReadS16(file);
}

void Level_Section_ReadObjects(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const LEVEL_FORMAT_LOADER *const loader = ctx->loader;
    const int32_t num_objects = VFile_ReadS32(file);
    LOG_INFO("objects: %d", num_objects);
    for (int32_t i = 0; i < num_objects; i++) {
        OBJECT fallback_obj = {};
        const int32_t game_obj_id = VFile_ReadS32(file);
        OBJECT *obj = Object_GetByGameID(game_obj_id);
        if (obj == nullptr) {
            if (loader->game_version == 3) {
                // TODO: remove this check after we implement the items
                obj = &fallback_obj;
            } else {
                Shell_ExitSystemFmt("Invalid object ID: %d", game_obj_id);
            }
        }
        obj->mesh_count = VFile_ReadS16(file);
        obj->mesh_idx = VFile_ReadS16(file);
        obj->bone_idx = VFile_ReadS32(file) / ANIM_BONE_SIZE;
        obj->frame_ofs = VFile_ReadU32(file);
        obj->frame_base = nullptr;
        obj->anim_idx = VFile_ReadS16(file);
        obj->loaded = true;
    }

    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_ReadStaticObjects(
    LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_objects = VFile_ReadS32(file);
    LOG_INFO("static objects: %d", num_objects);

    typedef struct {
        int32_t static_id;
        int16_t mesh_idx;
        BOUNDS_16 draw_bounds;
        BOUNDS_16 collision_bounds;
        uint16_t flags;
    } M_STATIC_OBJ_3D_TEMP;

    M_STATIC_OBJ_3D_TEMP *tmp_statics =
        Memory_Alloc(sizeof(M_STATIC_OBJ_3D_TEMP) * num_objects);

    int32_t max_static_id = -1;
    for (int32_t i = 0; i < num_objects; i++) {
        tmp_statics[i].static_id = VFile_ReadS32(file);
        if (tmp_statics[i].static_id < 0) {
            Shell_ExitSystemFmt(
                "Invalid static ID: %d", tmp_statics[i].static_id);
        }
        max_static_id = MAX(max_static_id, tmp_statics[i].static_id);

        tmp_statics[i].mesh_idx = VFile_ReadS16(file);
        M_ReadBounds16(&tmp_statics[i].draw_bounds, file);
        M_ReadBounds16(&tmp_statics[i].collision_bounds, file);
        tmp_statics[i].flags = VFile_ReadU16(file);
    }

    LOG_INFO("max static id: %d", max_static_id);
    int32_t injection_max_id = Inject_GetMaxStaticObject3DId();
    if (injection_max_id < 0) {
        injection_max_id = -1;
    }
    const int32_t capacity = MAX(max_static_id, injection_max_id) + 1;
    Object_InitialiseStaticObjects3D(capacity);

    for (int32_t i = 0; i < num_objects; i++) {
        STATIC_OBJECT_3D *const obj =
            Object_Get3DStatic(tmp_statics[i].static_id);
        obj->mesh_idx = tmp_statics[i].mesh_idx;
        obj->loaded = true;
        obj->draw_bounds = tmp_statics[i].draw_bounds;
        obj->collision_bounds = tmp_statics[i].collision_bounds;

        obj->collidable = (tmp_statics[i].flags & 1) == 0;
        obj->visible = (tmp_statics[i].flags & 2) != 0;

        Object_GetMesh(obj->mesh_idx)->enable_caustics = obj->visible;
    }

    Memory_FreePointer(&tmp_statics);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_ReadSpriteSequences(
    LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const int32_t num_sequences = VFile_ReadS32(file);
    LOG_DEBUG("sprite sequences: %d", num_sequences);

    int32_t injection_max_id = Inject_GetMaxStaticObject2DId();
    if (injection_max_id < 0) {
        injection_max_id = -1;
    }
    const int32_t capacity = MAX(num_sequences - 1, injection_max_id) + 1;
    Object_InitialiseStaticObjects2D(capacity);

    int32_t static_id = 0;
    for (int32_t i = 0; i < num_sequences; i++) {
        const int32_t id = VFile_ReadS32(file);
        const int16_t num_meshes = VFile_ReadS16(file);
        const int16_t mesh_idx = VFile_ReadS16(file);

        // In OG, a sprite was determined as either a game or static type based
        // on the original total game object count. As IDs are freely assignable
        // in TRX, a defined list of game sprites must instead be referred to.
        const OBJECT_ID object_id = Object_FromGameID(id);
        if (object_id != NO_OBJECT
            && Object_IsType(object_id, g_GameSpriteObjects)) {
            OBJECT *const obj = Object_Get(object_id);
            obj->mesh_count = num_meshes;
            obj->mesh_idx = mesh_idx;
            obj->anim_idx = NO_ANIM;
            obj->loaded = true;
        } else {
            STATIC_OBJECT_2D *const obj = Object_Get2DStatic(static_id);
            if (obj == nullptr) {
                Shell_ExitSystemFmt("Invalid sprite slot (%d)", id);
                break;
            }
            obj->frame_count = ABS(num_meshes);
            obj->texture_idx = mesh_idx;
            obj->loaded = true;
            static_id++;
        }
    }

    Benchmark_End(&benchmark, nullptr);
}

void Level_Section_ReadItems(LEVEL_CONTEXT *const ctx, VFILE *const file)
{
    BENCHMARK benchmark = Benchmark_Start();
    const LEVEL_FORMAT_LOADER *const loader = ctx->loader;
    const int32_t num_items = VFile_ReadS32(file);
    LOG_INFO("items: %d", num_items);
    if (num_items > MAX_ITEMS) {
        Shell_ExitSystem("Too many items");
        goto finish;
    }

    Item_InitialiseItems(num_items);
    for (int32_t i = 0; i < num_items; i++) {
        ITEM *const item = Item_Get(i);
        const int16_t obj_id = VFile_ReadS16(file);
        item->object_id = Object_FromGameID(obj_id);
        if (item->object_id == NO_OBJECT) {
            if (loader->game_version == 3) {
                // TODO: remove this check after we implement the items
                LOG_ERROR("Unsupported object #%d", obj_id);
                item->object_id = O_DUMMY;
            } else {
                Shell_ExitSystemFmt(
                    "Bad object number (%d) on item %d", obj_id, i);
                goto finish;
            }
        }

        item->room_num = VFile_ReadS16(file);
        M_ReadPosition(&item->pos, file);
        item->rot.y = VFile_ReadS16(file);
        M_ReadShade(loader, &item->shade, file);
        item->flags = VFile_ReadS16(file);
    }

finish:
    Benchmark_End(&benchmark, nullptr);
}
