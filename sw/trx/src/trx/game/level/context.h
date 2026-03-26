#pragma once

#include <trx/game/output/types.h>

typedef struct LEVEL_FORMAT_LOADER LEVEL_FORMAT_LOADER;

typedef struct {
    struct {
        int32_t anim_count;
        int32_t change_count;
        int32_t range_count;
        int32_t command_count;
        int16_t *commands;
        int32_t bone_count;
        int32_t frame_count;
        int16_t *frames;
    } anims;

    struct {
        int32_t object_count;
        int32_t sprite_count;
        int32_t page_count;
        uint8_t *pages_8;
        RGBA_8888 *pages_32;
    } textures;

    struct {
        int32_t size;
        RGB_888 *data_24;
        RGB_888 *data_32;
    } palette;

    struct {
        int32_t offset_count;
        int32_t *offsets;

        // TR1-specific
        int32_t data_size;
        char *data;
    } samples;

    int32_t mesh_ptr_count;
} LEVEL_CONTEXT_INFO;

typedef struct {
    LEVEL_CONTEXT_INFO info;
    const LEVEL_FORMAT_LOADER *loader;
} LEVEL_CONTEXT;

void Level_Context_Reset(const LEVEL_FORMAT_LOADER *loader);
LEVEL_CONTEXT *Level_Context_Get(void);
LEVEL_CONTEXT_INFO *Level_Context_GetInfo(void);
