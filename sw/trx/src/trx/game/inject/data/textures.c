#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/game/const.h>
#include <trx/game/inject.h>
#include <trx/game/level/context.h>
#include <trx/game/level/sections/append.h>
#include <trx/game/objects/common.h>
#include <trx/game/output/const.h>
#include <trx/version.h>

static uint16_t M_RemapRGB8(const RGB_888 rgb)
{
    const LEVEL_CONTEXT_INFO *const level_info = Level_Context_GetInfo();
    uint16_t best_idx = 0;
    int32_t best_diff = INT32_MAX;
    for (int32_t i = 1; i < level_info->palette.size; i++) {
        const RGB_888 test_rgb = level_info->palette.data_24[i];
        const int32_t dr = rgb.r - test_rgb.r;
        const int32_t dg = rgb.g - test_rgb.g;
        const int32_t db = rgb.b - test_rgb.b;
        const int32_t diff = SQUARE(dr) + SQUARE(dg) + SQUARE(db);
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    return best_idx;
}

static void M_HandlePalette(
    const INJECTION *const injection, const int32_t data_count)
{
    uint16_t palette_map[data_count];
    for (int32_t i = 0; i < data_count; i++) {
        const RGB_888 rgb = {
            .r = VFile_ReadU8(injection->fp) * 4,
            .g = VFile_ReadU8(injection->fp) * 4,
            .b = VFile_ReadU8(injection->fp) * 4,
        };
        palette_map[i] = i == 0 ? 0 : M_RemapRGB8(rgb);
    }

    Inject_RegisterPaletteMap(palette_map, data_count);
}

static void M_HandleTexturePages(
    const INJECTION *const injection, const int32_t data_count)
{
    LEVEL_CONTEXT_INFO *const info = Level_Context_GetInfo();
    if (info->textures.pages_32 == nullptr) {
        VFile_Skip(
            injection->fp, data_count * TEXTURE_PAGE_SIZE * sizeof(RGBA_8888));
        VFile_Skip(injection->fp, data_count * TEXTURE_PAGE_SIZE);
        return;
    }

    RGBA_8888 *const output_32 =
        &info->textures.pages_32[info->textures.page_count * TEXTURE_PAGE_SIZE];
    VFile_Read(
        injection->fp, output_32,
        data_count * TEXTURE_PAGE_SIZE * sizeof(RGBA_8888));

    uint8_t *output_8 =
        &info->textures.pages_8[info->textures.page_count * TEXTURE_PAGE_SIZE];
    uint8_t *input_8 = Memory_Alloc(data_count * TEXTURE_PAGE_SIZE);
    VFile_Read(injection->fp, input_8, data_count * TEXTURE_PAGE_SIZE);

    uint8_t *input_ptr = input_8;
    for (int32_t i = 0; i < data_count * TEXTURE_PAGE_SIZE; i++) {
        *output_8++ = Inject_GetPaletteIndex(*input_ptr++);
    }

    Memory_FreePointer(&input_8);
    info->textures.page_count += data_count;
}

static void M_HandleSpriteSequences(
    const INJECTION *const injection, const int32_t data_count)
{
    LEVEL_CONTEXT_INFO *const level_info = Level_Context_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        const INJECTION_OBJECT_INFO obj_info = Inject_ReadObjectPtr(injection);
        const int16_t num_meshes = VFile_ReadS16(injection->fp);
        const int16_t mesh_idx = VFile_ReadS16(injection->fp);

        if (obj_info.type == OBJ_TYPE_OBJECT) {
            OBJECT *const obj = Object_TryGet(obj_info.id);
            if (obj == nullptr) {
                LOG_WARNING("Invalid object %d", obj_info.id);
            } else {
                obj->mesh_count = num_meshes;
                obj->mesh_idx = mesh_idx + level_info->textures.sprite_count;
                obj->loaded = true;
            }
        } else if (obj_info.type == OBJ_TYPE_STATIC2D) {
            STATIC_OBJECT_2D *const obj = Object_Get2DStatic(obj_info.id);
            if (obj == nullptr) {
                LOG_WARNING("Invalid static 2D %d", obj_info.id);
                continue;
            }
            obj->frame_count = ABS(num_meshes);
            obj->texture_idx = mesh_idx + level_info->textures.sprite_count;
            obj->loaded = true;
        } else {
            LOG_WARNING("Invalid object type %d", obj_info.type);
        }
        level_info->textures.sprite_count += ABS(num_meshes);
    }
}

static void M_HandleTextureData(
    const INJECTION_CONTEXT *const ctx, const INJECTION_CHUNK chunk)
{
    for (int32_t i = 0; i < chunk.num_blocks; i++) {
        const INJECTION_DATA_TYPE data_type =
            VFile_ReadS32(chunk.injection->fp);
        const int32_t data_count = VFile_ReadS32(chunk.injection->fp);
        const int32_t data_size = VFile_ReadS32(chunk.injection->fp);

        if (ctx->mode == INJECTION_MODE_STATS) {
            VFile_Skip(chunk.injection->fp, data_size);
            continue;
        }

        switch (data_type) {
        case IDT_PALETTE:
            M_HandlePalette(chunk.injection, data_count);
            break;
        case IDT_TEXTURE_PAGES:
            M_HandleTexturePages(chunk.injection, data_count);
            break;
        default:
            LOG_WARNING("Unknown data type: %d", data_type);
            VFile_Skip(chunk.injection->fp, data_size);
            break;
        }
    }
}

static void M_HandleTextureInfo(
    const INJECTION_CONTEXT *const ctx, const INJECTION_CHUNK chunk)
{
    LEVEL_CONTEXT_INFO *const level_info = Level_Context_GetInfo();
    const LEVEL_CONTEXT_INFO cached_info = Inject_GetCachedInfo();
    const int32_t page_base = cached_info.textures.page_count;

    for (int32_t i = 0; i < chunk.num_blocks; i++) {
        const INJECTION_DATA_TYPE data_type =
            VFile_ReadS32(chunk.injection->fp);
        const int32_t data_count = VFile_ReadS32(chunk.injection->fp);
        const int32_t data_size = VFile_ReadS32(chunk.injection->fp);

        if (ctx->mode == INJECTION_MODE_STATS) {
            VFile_Skip(chunk.injection->fp, data_size);
            continue;
        }

        switch (data_type) {
        case IDT_OBJECT_TEXTURES:
            Level_Section_AppendObjectTextures(
                level_info->textures.object_count, page_base, data_count,
                chunk.injection->fp, false);
            level_info->textures.object_count += data_count;
            break;
        case IDT_SPRITE_TEXTURES:
            Level_Section_AppendSpriteTextures(
                level_info->textures.sprite_count, page_base, data_count,
                chunk.injection->fp);
            break;
        case IDT_SPRITE_SEQUENCES:
            M_HandleSpriteSequences(chunk.injection, data_count);
            break;
        default:
            LOG_WARNING("Unknown data type: %d", data_type);
            VFile_Skip(chunk.injection->fp, data_size);
            break;
        }
    }
}

REGISTER_INJECTOR(ICT_TEXTURE_DATA, M_HandleTextureData)
REGISTER_INJECTOR(ICT_TEXTURE_INFO, M_HandleTextureInfo)
