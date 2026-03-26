#include <trx/game/output/textures.h>

#include <trx/core/hash.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/game_buf.h>
#include <trx/game/game_flow.h>
#include <trx/game/level/cache.h>
#include <trx/game/objects/common.h>
#include <trx/game/output.h>
#include <trx/game/output/vertex_range.h>
#include <trx/gl/utils.h>
#include <trx/version.h>

#include <SDL2/SDL_mutex.h>
#include <string.h>

typedef struct {
    OUTPUT_UVW corners[4];
} M_UVW_PACK;

static struct {
    VECTOR *objects;
    VECTOR *sprites;
} m_AnimationRanges;

static struct {
    GLuint tex_atlas;
    GLuint tex_env_map;

    struct {
        int32_t count;
        int32_t count_objects;
        int32_t count_sprites;
        M_UVW_PACK *data;
        M_UVW_PACK *data_objects;
        M_UVW_PACK *data_sprites;

        bool *animated;
        bool *animated_objects;
        bool *animated_sprites;
        bool *has_transparency_objects;

        uint16_t *flags;
        uint16_t *flags_objects;
        uint16_t *flags_sprites;
    } uvws;

    struct {
        OUTPUT_TEXTURE_SIZE *data;
        OUTPUT_TEXTURE_SIZE *data_objects;
        OUTPUT_TEXTURE_SIZE *data_sprites;
    } atlas_sizes;
} m_Priv = {};

static int32_t m_TexturePageCount = 0;
static uint8_t *m_TexturePages8 = nullptr;
static RGBA_8888 *m_TexturePages32 = nullptr;
static SDL_mutex **m_TexturePageLocks = nullptr;

static int32_t m_PaletteSize = 0;
static RGB_888 *m_Palette8 = nullptr;
static RGB_888 *m_Palette16 = nullptr;

static LIGHT_MAP m_LightMap[32];
static SHADE_MAP m_ShadeMap[256];

static int32_t m_ObjectTextureCount = 0;
static int32_t m_SpriteTextureCount = 0;
static OBJECT_TEXTURE *m_ObjectTextures = nullptr;
static SPRITE_TEXTURE *m_SpriteTextures = nullptr;
static ANIMATED_TEXTURE_RANGE *m_AnimTextureRanges = nullptr;

#define M_TRANSPARENCY_CACHE_VERSION 1
static uint64_t M_ComputeTransparencyChecksum(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level == nullptr) {
        return 0;
    }

    uint64_t hash = LevelCache_InitChecksum(
        "object_transparency_cache", M_TRANSPARENCY_CACHE_VERSION);
    hash = LevelCache_UpdateLevelChecksum(hash, level);
    hash = Hash_FNV1a64_UpdateU32(hash, Output_GetObjectTextureCount());
    return hash;
}

static const char *M_GetTransparencyCacheFilename(void)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    const char *const level_key = LevelCache_GetLevelKey(level);
    if (level_key == nullptr) {
        return nullptr;
    }

    return String_FormatStatic("object_transparency_%s.cache.dat", level_key);
}

static bool M_TryLoadTransparencyCache(void)
{
    const int32_t texture_count = Output_GetObjectTextureCount();
    const uint64_t expected_checksum = M_ComputeTransparencyChecksum();
    const char *const cache_filename = M_GetTransparencyCacheFilename();
    if (cache_filename == nullptr || expected_checksum == 0
        || texture_count <= 0) {
        return false;
    }

    MYFILE *const file =
        LevelCache_OpenBinaryRead(cache_filename, expected_checksum);
    if (file == nullptr) {
        return false;
    }

    const int32_t version = File_ReadS32(file);
    const int32_t cached_texture_count = File_ReadS32(file);
    if (version != M_TRANSPARENCY_CACHE_VERSION
        || cached_texture_count != texture_count
        || !File_ReadData(
            file, m_Priv.uvws.has_transparency_objects,
            sizeof(bool) * (size_t)texture_count)) {
        File_Close(file);
        return false;
    }

    File_Close(file);
    return true;
}

static void M_WriteTransparencyCache(void)
{
    const int32_t texture_count = Output_GetObjectTextureCount();
    const uint64_t checksum = M_ComputeTransparencyChecksum();
    const char *const cache_filename = M_GetTransparencyCacheFilename();
    if (cache_filename == nullptr || checksum == 0 || texture_count <= 0) {
        return;
    }

    MYFILE *const file = LevelCache_OpenBinaryWrite(cache_filename, checksum);
    if (file == nullptr) {
        return;
    }

    File_WriteS32(file, M_TRANSPARENCY_CACHE_VERSION);
    File_WriteS32(file, texture_count);
    File_WriteData(
        file, m_Priv.uvws.has_transparency_objects,
        sizeof(bool) * (size_t)texture_count);
    File_Close(file);
}

static float M_NormalizeObjectUV(const uint16_t uv)
{
    return uv / 65535.0f;
}

static void M_PrepareObjectAnimationRanges(void)
{
    size_t required_size = 0;
    for (const ANIMATED_TEXTURE_RANGE *src_range =
             Output_GetAnimatedTextureRange(0);
         src_range != nullptr; src_range = src_range->next_range) {
        required_size += src_range->num_textures;
    }

    Vector_Clear(m_AnimationRanges.objects);
    Vector_EnsureCapacity(m_AnimationRanges.objects, required_size);

    for (const ANIMATED_TEXTURE_RANGE *src_range =
             Output_GetAnimatedTextureRange(0);
         src_range != nullptr; src_range = src_range->next_range) {
        for (int32_t i = 0; i < src_range->num_textures; i++) {
            Vector_Add(
                m_AnimationRanges.objects,
                &(OUTPUT_VERTEX_RANGE) {
                    .vertex_start = src_range->textures[i],
                    .vertex_count = 1,
                });
        }
    }
    Output_GlueVertexRanges(m_AnimationRanges.objects);
}

static void M_PrepareSpriteAnimationRanges(void)
{
    size_t required_size = 0;
    const int32_t static_2d_count = Object_GetStaticObjects2DCount();
    for (int32_t i = 0; i < static_2d_count; i++) {
        const STATIC_OBJECT_2D *const obj = Object_Get2DStatic(i);
        if (obj == nullptr || !obj->loaded || obj->frame_count == 1) {
            continue;
        }
        required_size++;
    }

    Vector_Clear(m_AnimationRanges.sprites);
    Vector_EnsureCapacity(m_AnimationRanges.sprites, required_size);

    for (int32_t i = 0; i < static_2d_count; i++) {
        const STATIC_OBJECT_2D *const obj = Object_Get2DStatic(i);
        if (obj == nullptr || !obj->loaded || obj->frame_count == 1) {
            continue;
        }
        Vector_Add(
            m_AnimationRanges.sprites,
            &(OUTPUT_VERTEX_RANGE) {
                .vertex_start = obj->texture_idx,
                .vertex_count = obj->frame_count,
            });
    }
    Output_GlueVertexRanges(m_AnimationRanges.sprites);
}

static void M_PrepareAnimationRanges(void)
{
    M_PrepareObjectAnimationRanges();
    M_PrepareSpriteAnimationRanges();

    for (int32_t i = 0; i < Output_GetObjectTextureCount(); i++) {
        m_Priv.uvws.animated_objects[i] = false;
        for (int32_t j = 0; j < m_AnimationRanges.objects->count; j++) {
            const OUTPUT_VERTEX_RANGE *const dst_range =
                Vector_Get(m_AnimationRanges.objects, j);
            const int32_t range_start = dst_range->vertex_start;
            const int32_t range_end = range_start + dst_range->vertex_count;
            if (i >= range_start && i < range_end) {
                m_Priv.uvws.animated_objects[i] = true;
                break;
            }
        }
    }

    for (int32_t i = 0; i < Output_GetSpriteTextureCount(); i++) {
        m_Priv.uvws.animated_sprites[i] = false;
        for (int32_t j = 0; j < m_AnimationRanges.sprites->count; j++) {
            const OUTPUT_VERTEX_RANGE *const dst_range =
                Vector_Get(m_AnimationRanges.sprites, j);
            const int32_t range_start = dst_range->vertex_start;
            const int32_t range_end = range_start + dst_range->vertex_count;
            if (i >= range_start && i < range_end) {
                m_Priv.uvws.animated_sprites[i] = true;
                break;
            }
        }
    }
}

static void M_FillAtlasObjectSize(const int32_t i)
{
    OUTPUT_TEXTURE_SIZE *const size = &m_Priv.atlas_sizes.data_objects[i];
    const OBJECT_TEXTURE *const texture = Output_GetObjectTexture(i);
    size->x0 = texture->uv[0].u;
    size->y0 = texture->uv[0].v;
    size->x1 = texture->uv[0].u;
    size->y1 = texture->uv[0].v;
    for (int32_t j = 1; j < texture->uv_count; j++) {
        size->x0 = MIN(size->x0, texture->uv[j].u);
        size->y0 = MIN(size->y0, texture->uv[j].v);
        size->x1 = MAX(size->x1, texture->uv[j].u);
        size->y1 = MAX(size->y1, texture->uv[j].v);
    }
    size->x0 = M_NormalizeObjectUV(size->x0);
    size->y0 = M_NormalizeObjectUV(size->y0);
    size->x1 = M_NormalizeObjectUV(size->x1);
    size->y1 = M_NormalizeObjectUV(size->y1);
}

static void M_FillAtlasSpriteSize(const int32_t i)
{
    OUTPUT_TEXTURE_SIZE *const size = &m_Priv.atlas_sizes.data_sprites[i];
    const SPRITE_TEXTURE *const sprite = Output_GetSpriteTexture(i);
    const float adj = 0.1 / 256.0f;
    const float u0 = (sprite->offset & 0xFF) / 256.0f + adj;
    const float v0 = (sprite->offset >> 8) / 256.0f + adj;
    const float u1 = u0 + sprite->width / 65536.0f - 2 * adj;
    const float v1 = v0 + sprite->height / 65536.0f - 2 * adj;
    size->x0 = u0;
    size->y0 = v0;
    size->x1 = u1;
    size->y1 = v1;
}

static void M_FillObjectUVW(const int32_t i)
{
    const OBJECT_TEXTURE *const texture = Output_GetObjectTexture(i);
    OUTPUT_UVW *const corners = m_Priv.uvws.data_objects[i].corners;
    for (int32_t j = 0; j < 4; j++) {
        corners[j].u = M_NormalizeObjectUV(texture->uv[j].u);
        corners[j].v = M_NormalizeObjectUV(texture->uv[j].v);
        corners[j].w = texture->tex_page;
    }
}

static void M_FillSpriteUVW(const int32_t i)
{
    const SPRITE_TEXTURE *const sprite = Output_GetSpriteTexture(i);
    const float adj = 0.1 / 256.0f;
    const float u0 = (sprite->offset & 0xFF) / 256.0f + adj;
    const float v0 = (sprite->offset >> 8) / 256.0f + adj;
    const float u1 = u0 + sprite->width / 65536.0f - 2 * adj;
    const float v1 = v0 + sprite->height / 65536.0f - 2 * adj;
    OUTPUT_UVW *const corners = m_Priv.uvws.data_sprites[i].corners;
    // clang-format off
    corners[0].u = u0; corners[0].v = v0; corners[0].w = sprite->tex_page;
    corners[1].u = u1; corners[1].v = v0; corners[1].w = sprite->tex_page;
    corners[2].u = u1; corners[2].v = v1; corners[2].w = sprite->tex_page;
    corners[3].u = u0; corners[3].v = v1; corners[3].w = sprite->tex_page;
    m_Priv.uvws.flags_sprites[i] = sprite->flags;
    // clang-format on
}

static void M_FillObjectUVWs(void)
{
    for (int32_t i = 0; i < Output_GetObjectTextureCount(); i++) {
        M_FillObjectUVW(i);
    }
}

static void M_FillSpriteUVWs(void)
{
    for (int32_t i = 0; i < Output_GetSpriteTextureCount(); i++) {
        M_FillSpriteUVW(i);
    }
}

static void M_UpdateObjectAnimatedUVWs(VECTOR *const source)
{
    for (int32_t i = 0; i < source->count; i++) {
        const OUTPUT_VERTEX_RANGE *const range = Vector_Get(source, i);
        for (int32_t j = 0; j < range->vertex_count; j++) {
            M_FillObjectUVW(range->vertex_start + j);
            M_FillAtlasObjectSize(range->vertex_start + j);
        }
    }
}

static void M_UpdateSpriteAnimatedUVWs(VECTOR *const source)
{
    for (int32_t i = 0; i < source->count; i++) {
        const OUTPUT_VERTEX_RANGE *const range = Vector_Get(source, i);
        for (int32_t j = 0; j < range->vertex_count; j++) {
            M_FillSpriteUVW(range->vertex_start + j);
            M_FillAtlasSpriteSize(range->vertex_start + j);
        }
    }
}

static void M_PrepareUVWs(void)
{
    m_Priv.uvws.count_objects = Output_GetObjectTextureCount();
    m_Priv.uvws.count_sprites = Output_GetSpriteTextureCount();
    m_Priv.uvws.count = m_Priv.uvws.count_objects + m_Priv.uvws.count_sprites;
    m_Priv.uvws.data = Memory_Alloc(m_Priv.uvws.count * sizeof(M_UVW_PACK));
    m_Priv.uvws.data_objects = m_Priv.uvws.data;
    m_Priv.uvws.data_sprites = m_Priv.uvws.data + m_Priv.uvws.count_objects;
    m_Priv.uvws.animated = Memory_Alloc(m_Priv.uvws.count * sizeof(bool));
    m_Priv.uvws.animated_objects = m_Priv.uvws.animated;
    m_Priv.uvws.animated_sprites =
        m_Priv.uvws.animated + m_Priv.uvws.count_objects;
    m_Priv.uvws.flags = Memory_Alloc(m_Priv.uvws.count * sizeof(uint16_t));
    m_Priv.uvws.flags_objects = m_Priv.uvws.flags;
    m_Priv.uvws.flags_sprites = m_Priv.uvws.flags + m_Priv.uvws.count_objects;
    m_Priv.uvws.has_transparency_objects =
        Memory_Alloc(m_Priv.uvws.count_objects * sizeof(bool));
    M_FillObjectUVWs();
    M_FillSpriteUVWs();
}

static bool M_ObjectTextureHasTransparency(const int32_t texture_idx)
{
    if (texture_idx < 0 || texture_idx >= Output_GetObjectTextureCount()
        || m_TexturePages32 == nullptr) {
        return true;
    }

    const OBJECT_TEXTURE *const texture = Output_GetObjectTexture(texture_idx);
    if (texture == nullptr || texture->uv_count <= 0) {
        return true;
    }
    if (texture->tex_page >= m_TexturePageCount) {
        return true;
    }

    int32_t min_u = INT32_MAX;
    int32_t min_v = INT32_MAX;
    int32_t max_u = INT32_MIN;
    int32_t max_v = INT32_MIN;
    for (int32_t i = 0; i < texture->uv_count; i++) {
        CLAMPG(min_u, texture->uv[i].u);
        CLAMPG(min_v, texture->uv[i].v);
        CLAMPL(max_u, texture->uv[i].u);
        CLAMPL(max_v, texture->uv[i].v);
    }

    const int32_t x0 = (min_u * (TEXTURE_PAGE_WIDTH - 1)) / 65535;
    const int32_t y0 = (min_v * (TEXTURE_PAGE_HEIGHT - 1)) / 65535;
    const int32_t x1 =
        (max_u * (TEXTURE_PAGE_WIDTH - 1) + 65534) / 65535; // ceil
    const int32_t y1 =
        (max_v * (TEXTURE_PAGE_HEIGHT - 1) + 65534) / 65535; // ceil

    int32_t px0 = x0;
    int32_t py0 = y0;
    int32_t px1 = x1;
    int32_t py1 = y1;
    CLAMP(px0, 0, TEXTURE_PAGE_WIDTH - 1);
    CLAMP(py0, 0, TEXTURE_PAGE_HEIGHT - 1);
    CLAMP(px1, 0, TEXTURE_PAGE_WIDTH - 1);
    CLAMP(py1, 0, TEXTURE_PAGE_HEIGHT - 1);

    const RGBA_8888 *const page = Output_GetTexturePage32(texture->tex_page);
    if (page == nullptr) {
        return true;
    }

    for (int32_t y = py0; y <= py1; y++) {
        const int32_t row = y * TEXTURE_PAGE_WIDTH;
        for (int32_t x = px0; x <= px1; x++) {
            if (page[row + x].a < 255) {
                return true;
            }
        }
    }
    return false;
}

static void M_PrepareObjectTransparencyFlags(void)
{
    for (int32_t i = 0; i < Output_GetObjectTextureCount(); i++) {
        m_Priv.uvws.has_transparency_objects[i] =
            M_ObjectTextureHasTransparency(i);
    }
}

static void M_PrepareEnvMap(void)
{
    glGenTextures(1, &m_Priv.tex_env_map);
    glBindTexture(GL_TEXTURE_2D, m_Priv.tex_env_map);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    TRX_GL_CheckError();

    if (Output_IsHeadless()) {
        const int32_t pattern_size = 256;
        RGB_888 *test_pattern =
            Memory_Alloc(pattern_size * pattern_size * sizeof(RGB_888));
        RGB_888 *pixel = test_pattern;
        for (int32_t i = 0; i < pattern_size; i++) {
            for (int32_t j = 0; j < pattern_size; j++) {
                pixel->r = i % 256;
                pixel->g = j % 256;
                pixel->b = ((i / 32) % 2 == (j / 32) % 2) ? 255 : 0;
                pixel++;
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_Priv.tex_env_map);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGB, pattern_size, pattern_size, 0, GL_RGB,
            GL_UNSIGNED_BYTE, test_pattern);
        TRX_GL_CheckError();
        Memory_FreePointer(&test_pattern);
    }
}

static void M_PrepareAtlasSizes(void)
{
    const int32_t count_objects = Output_GetObjectTextureCount();
    const int32_t count_sprites = Output_GetSpriteTextureCount();
    const int32_t count = count_objects + count_sprites;
    m_Priv.atlas_sizes.data = Memory_Realloc(
        m_Priv.atlas_sizes.data, count * sizeof(OUTPUT_TEXTURE_SIZE));
    m_Priv.atlas_sizes.data_objects = m_Priv.atlas_sizes.data;
    m_Priv.atlas_sizes.data_sprites = m_Priv.atlas_sizes.data + count_objects;
    for (int32_t i = 0; i < count_objects; i++) {
        M_FillAtlasObjectSize(i);
    }
    for (int32_t i = 0; i < count_sprites; i++) {
        M_FillAtlasSpriteSize(i);
    }
}

static void M_UploadAtlas(void)
{
    glGenTextures(1, &m_Priv.tex_atlas);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_Priv.tex_atlas);
    glTexStorage3D(
        GL_TEXTURE_2D_ARRAY,
        1, // number of mipmaps
        GL_RGBA8, TEXTURE_PAGE_WIDTH, TEXTURE_PAGE_HEIGHT,
        Output_GetTexturePageCount());
    TRX_GL_CheckError();

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    TRX_GL_CheckError();

    for (int32_t i = 0; i < Output_GetTexturePageCount(); i++) {
        const RGBA_8888 *const input_ptr = Output_GetTexturePage32(i);

        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0, // mipmap level
            0, // x offset
            0, // y offset
            i, // z offset
            TEXTURE_PAGE_WIDTH, TEXTURE_PAGE_HEIGHT,
            1, // depth
            GL_RGBA, GL_UNSIGNED_BYTE, input_ptr);
    }
    TRX_GL_CheckError();

    M_PrepareAtlasSizes();

    TRX_GL_CheckError();
}

static void M_FreeLevelData(void)
{
    // destroy per-page locks
    if (m_TexturePageLocks != nullptr) {
        for (int32_t i = 0; i < m_TexturePageCount; i++) {
            SDL_DestroyMutex(m_TexturePageLocks[i]);
        }
        m_TexturePageLocks = nullptr;
    }

    if (m_Priv.tex_atlas != 0) {
        glDeleteTextures(1, &m_Priv.tex_atlas);
        m_Priv.tex_atlas = 0;
    }
    Memory_FreePointer(&m_Priv.uvws.data);
    Memory_FreePointer(&m_Priv.uvws.animated);
    Memory_FreePointer(&m_Priv.uvws.flags);
    Memory_FreePointer(&m_Priv.uvws.has_transparency_objects);
    Memory_FreePointer(&m_Priv.atlas_sizes.data);

    memset(&m_Priv.uvws, 0, sizeof(m_Priv.uvws));
    memset(&m_Priv.atlas_sizes, 0, sizeof(m_Priv.atlas_sizes));
}

void Output_Textures_Init(void)
{
    M_PrepareEnvMap();
    m_AnimationRanges.objects = Vector_Create(sizeof(OUTPUT_VERTEX_RANGE));
    m_AnimationRanges.sprites = Vector_Create(sizeof(OUTPUT_VERTEX_RANGE));
}

void Output_Textures_Shutdown(void)
{
    if (m_AnimationRanges.objects != nullptr) {
        Vector_Free(m_AnimationRanges.objects);
        m_AnimationRanges.objects = nullptr;
    }
    if (m_AnimationRanges.sprites != nullptr) {
        Vector_Free(m_AnimationRanges.sprites);
        m_AnimationRanges.sprites = nullptr;
    }
    M_FreeLevelData();

    if (m_Priv.tex_env_map != 0) {
        glDeleteTextures(1, &m_Priv.tex_env_map);
        m_Priv.tex_env_map = 0;
    }

    // These are GameBuf-backed and become invalid once GameBuf_Shutdown runs.
    m_TexturePageCount = 0;
    m_TexturePages8 = nullptr;
    m_TexturePages32 = nullptr;
    m_TexturePageLocks = nullptr;
    m_PaletteSize = 0;
    m_Palette8 = nullptr;
    m_Palette16 = nullptr;
    m_ObjectTextureCount = 0;
    m_SpriteTextureCount = 0;
    m_ObjectTextures = nullptr;
    m_SpriteTextures = nullptr;
    m_AnimTextureRanges = nullptr;
}

void Output_Textures_ObserveLevelLoad(void)
{
    M_FreeLevelData();
    M_PrepareUVWs();
    if (!M_TryLoadTransparencyCache()) {
        M_PrepareObjectTransparencyFlags();
        M_WriteTransparencyCache();
    }
    M_PrepareAnimationRanges();
    M_UploadAtlas();
}

void Output_Textures_UpdateEnvironmentMap(void)
{
    if (Output_IsHeadless()) {
        return;
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    TRX_GL_CheckError();

    const GLint vp_x = viewport[0];
    const GLint vp_y = viewport[1];
    const GLint vp_w = viewport[2];
    const GLint vp_h = viewport[3];

    const int32_t side = MIN(vp_w, vp_h);
    const int32_t x = vp_x + (vp_w - side) / 2;
    const int32_t y = vp_y + (vp_h - side) / 2;
    const int32_t w = side;
    const int32_t h = side;

    glBindTexture(GL_TEXTURE_2D, m_Priv.tex_env_map);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, w, h, 0);
    TRX_GL_CheckError();
}

void Output_Textures_CycleAnimations(void)
{
    if (m_Priv.uvws.count != 0) {
        M_UpdateSpriteAnimatedUVWs(m_AnimationRanges.sprites);
        M_UpdateObjectAnimatedUVWs(m_AnimationRanges.objects);
    }
}

GLuint Output_Textures_GetAtlasTexture(void)
{
    return m_Priv.tex_atlas;
}

GLuint Output_Textures_GetEnvMapTexture(void)
{
    return m_Priv.tex_env_map;
}

int32_t Output_Textures_GetObjectUVWIndex(int32_t texture_idx, int32_t corner)
{
    return texture_idx * 4 + corner;
}

int32_t Output_Textures_GetSpriteUVWIndex(int32_t texture_idx, int32_t corner)
{
    return (m_Priv.uvws.count_objects + texture_idx) * 4 + corner;
}

OUTPUT_UVW Output_Textures_GetUVW(const int32_t uvw_idx)
{
    ASSERT(uvw_idx >= 0 && uvw_idx / 4 < m_Priv.uvws.count);
    return m_Priv.uvws.data[uvw_idx / 4].corners[uvw_idx % 4];
}

OUTPUT_TEXTURE_SIZE Output_Textures_GetAtlasSize(const int32_t uvw_idx)
{
    ASSERT(uvw_idx >= 0 && uvw_idx < m_Priv.uvws.count);
    return m_Priv.atlas_sizes.data[uvw_idx];
}

bool Output_Textures_IsObjectTextureAnimated(const int32_t texture_idx)
{
    return m_Priv.uvws.animated_objects[texture_idx];
}

bool Output_Textures_IsSpriteTextureAnimated(const int32_t texture_idx)
{
    return m_Priv.uvws.animated_sprites[texture_idx];
}

void Output_Textures_SetSpriteTextureFlags(
    const int32_t texture_idx, const uint16_t flags)
{
    m_Priv.uvws.flags_sprites[texture_idx] = flags;
}

uint16_t Output_Textures_GetSpriteTextureFlags(const int32_t texture_idx)
{
    return VERT_BILLBOARD | m_Priv.uvws.flags_sprites[texture_idx];
}

SCENE_PASS Output_Textures_GetObjectTextureScenePass(const int32_t texture_idx)
{
    switch (Output_GetObjectTexture(texture_idx)->draw_type) {
    case DRAW_OPAQUE:
    case DRAW_REFLECTIVE_OPAQUE:
        return SCENE_PASS_OPAQUE;
    case DRAW_BLEND:
        if (!m_Priv.uvws.animated_objects[texture_idx]
            && !m_Priv.uvws.has_transparency_objects[texture_idx]) {
            return SCENE_PASS_OPAQUE;
        }
        return SCENE_PASS_TRANSPARENT;
    case DRAW_BLEND_ADD:
    case DRAW_REFLECTIVE_BLEND_ADD:
        return SCENE_PASS_BLEND_ADD;
    case DRAW_BLEND_SUB:
        return SCENE_PASS_BLEND_SUB;
    }
    return SCENE_PASS_OPAQUE;
}

void Output_Textures_ApplyRenderSettings(void)
{
    // re-adjust UVs when the bilinear filter is toggled.
    if (m_Priv.uvws.count != 0) {
        M_FillObjectUVWs();
    }
}
void Output_InitialiseTexturePages(const int32_t num_pages, const bool use_8bit)
{
    m_TexturePageCount = num_pages;
    if (num_pages == 0) {
        m_TexturePages32 = nullptr;
        m_TexturePages8 = nullptr;
        return;
    }

    const int32_t page_size = num_pages * TEXTURE_PAGE_SIZE;
    m_TexturePages32 =
        GameBuf_Alloc(sizeof(RGBA_8888) * page_size, GBUF_TEXTURE_PAGES);
    m_TexturePages8 = use_8bit
        ? GameBuf_Alloc(sizeof(uint8_t) * page_size, GBUF_TEXTURE_PAGES)
        : nullptr;

    m_TexturePageLocks =
        GameBuf_Alloc(sizeof(SDL_mutex *) * num_pages, GBUF_TEXTURE_PAGES);
    for (int32_t i = 0; i < num_pages; i++) {
        m_TexturePageLocks[i] = SDL_CreateMutex();
        ASSERT(m_TexturePageLocks[i] != nullptr);
    }
}

void Output_InitialisePalettes(
    const int32_t palette_size, const RGB_888 *const palette_8,
    const RGB_888 *const palette_16)
{
    ASSERT(palette_size != 0);
    ASSERT(palette_8 != nullptr);
    m_PaletteSize = palette_size;

    m_Palette8 = GameBuf_Alloc(sizeof(RGB_888) * palette_size, GBUF_PALETTES);
    memcpy(m_Palette8, palette_8, sizeof(RGB_888) * palette_size);

    if (palette_16 != nullptr) {
        m_Palette16 =
            GameBuf_Alloc(sizeof(RGB_888) * palette_size, GBUF_PALETTES);
        memcpy(m_Palette16, palette_16, sizeof(RGB_888) * palette_size);
    } else {
        m_Palette16 = nullptr;
    }
}

void Output_InitialiseObjectTextures(const int32_t num_textures)
{
    m_ObjectTextureCount = num_textures;
    m_ObjectTextures = num_textures == 0
        ? nullptr
        : GameBuf_Alloc(
              sizeof(OBJECT_TEXTURE) * num_textures, GBUF_OBJECT_TEXTURES);
}

void Output_InitialiseSpriteTextures(const int32_t num_textures)
{
    m_SpriteTextureCount = num_textures;
    m_SpriteTextures = num_textures == 0
        ? nullptr
        : GameBuf_Alloc(
              sizeof(SPRITE_TEXTURE) * num_textures, GBUF_SPRITE_TEXTURES);
}

void Output_InitialiseAnimatedTextures(const int32_t num_ranges)
{
    m_AnimTextureRanges = num_ranges == 0
        ? nullptr
        : GameBuf_Alloc(
              sizeof(ANIMATED_TEXTURE_RANGE) * num_ranges,
              GBUF_ANIMATED_TEXTURE_RANGES);
}

int32_t Output_GetTexturePageCount(void)
{
    return m_TexturePageCount;
}

uint8_t *Output_GetTexturePage8(const int32_t page_idx)
{
    if (m_TexturePages8 == nullptr) {
        return nullptr;
    }
    return &m_TexturePages8[page_idx * TEXTURE_PAGE_SIZE];
}

RGBA_8888 *Output_GetTexturePage32(const int32_t page_idx)
{
    if (m_TexturePages32 == nullptr) {
        return nullptr;
    }
    return &m_TexturePages32[page_idx * TEXTURE_PAGE_SIZE];
}

void Output_LockTexturePage32(const int32_t page_idx)
{
    ASSERT(page_idx >= 0 && page_idx < m_TexturePageCount);
    SDL_LockMutex(m_TexturePageLocks[page_idx]);
}

void Output_UnlockTexturePage32(const int32_t page_idx)
{
    ASSERT(page_idx >= 0 && page_idx < m_TexturePageCount);
    SDL_UnlockMutex(m_TexturePageLocks[page_idx]);
}

int32_t Output_GetPaletteSize(void)
{
    return m_PaletteSize;
}

RGB_888 Output_GetPaletteColor8(const uint16_t idx)
{
    if (m_Palette8 == nullptr) {
        return COLOR_RGB_888_BLACK;
    }
    return m_Palette8[idx];
}

RGB_888 Output_GetPaletteColor16(const uint16_t idx)
{
    if (m_Palette16 == nullptr) {
        return COLOR_RGB_888_BLACK;
    }
    return m_Palette16[idx];
}

LIGHT_MAP *Output_GetLightMap(const uint8_t idx)
{
    return &m_LightMap[idx];
}

SHADE_MAP *Output_GetShadeMap(const uint8_t idx)
{
    return &m_ShadeMap[idx];
}

int32_t Output_GetObjectTextureCount(void)
{
    return m_ObjectTextureCount;
}

int32_t Output_GetSpriteTextureCount(void)
{
    return m_SpriteTextureCount;
}

OBJECT_TEXTURE *Output_GetObjectTexture(const int32_t texture_idx)
{
    if (m_ObjectTextures == nullptr) {
        return nullptr;
    }
    return &m_ObjectTextures[texture_idx];
}

SPRITE_TEXTURE *Output_GetSpriteTexture(const int32_t texture_idx)
{
    if (m_SpriteTextures == nullptr) {
        return nullptr;
    }
    return &m_SpriteTextures[texture_idx];
}

ANIMATED_TEXTURE_RANGE *Output_GetAnimatedTextureRange(const int32_t range_idx)
{
    if (m_AnimTextureRanges == nullptr) {
        return nullptr;
    }
    return &m_AnimTextureRanges[range_idx];
}

RGBA_8888 Output_RGB2RGBA(const RGB_888 color)
{
    return (RGBA_8888) { .r = color.r, .g = color.g, .b = color.b, .a = 255 };
}

RGBA_F Output_RGB2RGBA_F(const RGB_F color)
{
    return (RGBA_F) { .r = color.r, .g = color.g, .b = color.b, .a = 1.0f };
}

int16_t Output_FindColor8(const RGB_888 color)
{
    if (m_Palette8 == nullptr) {
        return -1;
    }

    int32_t best_idx = 0;
    int32_t best_diff = INT32_MAX;
    for (int32_t i = 0; i < m_PaletteSize; i++) {
        const int32_t dr = color.r - m_Palette8[i].r;
        const int32_t dg = color.g - m_Palette8[i].g;
        const int32_t db = color.b - m_Palette8[i].b;
        const int32_t diff = SQUARE(dr) + SQUARE(dg) + SQUARE(db);
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    return best_idx;
}

void Output_CycleAnimatedTextures(void)
{
    const ANIMATED_TEXTURE_RANGE *range = m_AnimTextureRanges;
    for (; range != nullptr; range = range->next_range) {
        int32_t i = 0;
        const OBJECT_TEXTURE temp = m_ObjectTextures[range->textures[i]];
        for (; i < range->num_textures - 1; i++) {
            m_ObjectTextures[range->textures[i]] =
                m_ObjectTextures[range->textures[i + 1]];
        }
        m_ObjectTextures[range->textures[i]] = temp;
    }

    const int32_t static_2d_count = Object_GetStaticObjects2DCount();
    for (int32_t i = 0; i < static_2d_count; i++) {
        const STATIC_OBJECT_2D *const obj = Object_Get2DStatic(i);
        if (obj == nullptr || !obj->loaded || obj->frame_count == 1) {
            continue;
        }

        const int16_t frame_count = obj->frame_count;
        const SPRITE_TEXTURE temp = m_SpriteTextures[obj->texture_idx];
        for (int32_t j = 0; j < frame_count - 1; j++) {
            m_SpriteTextures[obj->texture_idx + j] =
                m_SpriteTextures[obj->texture_idx + j + 1];
        }
        m_SpriteTextures[obj->texture_idx + frame_count - 1] = temp;
    }
}
