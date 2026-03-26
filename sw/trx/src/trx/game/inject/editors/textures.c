#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/game/inject.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>

static void M_TextureEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    const LEVEL_CONTEXT_INFO *const level_info = Level_Context_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        const uint16_t target_page = VFile_ReadU16(injection->fp);
        const uint8_t target_x = VFile_ReadU8(injection->fp);
        const uint8_t target_y = VFile_ReadU8(injection->fp);
        const uint16_t source_width = VFile_ReadU16(injection->fp);
        const uint16_t source_height = VFile_ReadU16(injection->fp);

        const int32_t size = source_width * source_height * sizeof(RGBA_8888);
        RGBA_8888 *source_img = Memory_Alloc(size);
        VFile_Read(injection->fp, source_img, size);

        if (target_page >= level_info->textures.page_count) {
            LOG_WARNING("Texture page %d is beyond level range", target_page);
            continue;
        }

        RGBA_8888 *page =
            &level_info->textures.pages_32[target_page * TEXTURE_PAGE_SIZE];
        for (int32_t y = 0; y < source_height; y++) {
            for (int32_t x = 0; x < source_width; x++) {
                const int32_t target_pixel =
                    (y + target_y) * TEXTURE_PAGE_WIDTH + x + target_x;
                RGBA_8888 *const target_rgb = &page[target_pixel];
                *target_rgb = source_img[y * source_width + x];
            }
        }

        Memory_FreePointer(&source_img);
    }
}

static void M_SpriteEdits(
    const INJECTION_CONTEXT *const ctx, const INJECTION *const injection,
    const int32_t data_count)
{
    for (int32_t i = 0; i < data_count; i++) {
        const INJECTION_OBJECT_INFO obj_info = Inject_ReadObjectPtr(injection);
        int16_t x0 = VFile_ReadS16(injection->fp);
        int16_t y0 = VFile_ReadS16(injection->fp);
        int16_t x1 = VFile_ReadS16(injection->fp);
        int16_t y1 = VFile_ReadS16(injection->fp);

        const OBJECT *const obj = Object_TryGet(obj_info.id);
        if (obj == nullptr || !obj->loaded) {
            continue;
        }
        if (obj->mesh_idx < 0
            || obj->mesh_idx >= Output_GetSpriteTextureCount()) {
            LOG_WARNING(
                "Invalid sprite texture index %d for object %d", obj->mesh_idx,
                obj_info.id);
            continue;
        }
        SPRITE_TEXTURE *const sprite_texture =
            Output_GetSpriteTexture(obj->mesh_idx);
        sprite_texture->x0 = x0;
        sprite_texture->x1 = x1;
        sprite_texture->y0 = y0;
        sprite_texture->y1 = y1;
    }
}

REGISTER_INJECT_EDITOR(IDT_TEXTURE_EDITS, M_TextureEdits)
REGISTER_INJECT_EDITOR(IDT_SPRITE_EDITS, M_SpriteEdits)
