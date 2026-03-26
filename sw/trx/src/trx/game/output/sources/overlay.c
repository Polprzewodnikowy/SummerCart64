#include <trx/game/output/sources/overlay.h>

#include <trx/av/image.h>
#include <trx/config.h>
#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/fader.h>
#include <trx/game/game.h>
#include <trx/game/interpolation.h>
#include <trx/game/objects.h>
#include <trx/game/output/const.h>
#include <trx/game/output/overlay.h>
#include <trx/game/output/quad.h>
#include <trx/game/output/scene_compositor.h>
#include <trx/game/output/scene_source.h>
#include <trx/game/output/state.h>
#include <trx/game/output/textures.h>
#include <trx/game/viewport.h>
#include <trx/gl/texture.h>
#include <trx/gl/utils.h>

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define M_SCHEDULE_OP(queue_id, draw_func, inst)                               \
    M_ScheduleOpHelper(                                                        \
        &m_Priv, queue_id, (M_DRAW_OP_FUNC)draw_func, sizeof(inst),            \
        (const M_DRAW_OP *)&inst);

#define M_IMAGE_CACHE_CAPACITY 5
#define M_RELATIVE_ERROR(a, b) ABS((a) - (b)) / (b)

struct M_DRAW_OP;
typedef void (*M_DRAW_OP_FUNC)(const struct M_DRAW_OP *);

typedef struct M_DRAW_OP {
    M_DRAW_OP_FUNC draw;
} M_DRAW_OP;

typedef struct {
    M_DRAW_OP base;
    float opacity;
} M_DRAW_OP_BLACK_RECTANGLE;

typedef struct {
    M_DRAW_OP base;
    GLuint texture_id;
    int32_t width;
    int32_t height;
    float opacity;
    float desaturation;
    bool flip_y;
    bool use_fit;
} M_DRAW_OP_IMAGE;

typedef struct {
    M_DRAW_OP base;
    GLuint texture_id;
    int32_t width;
    int32_t height;
    float opacity;
} M_DRAW_OP_SNAPSHOT;

typedef struct {
    M_DRAW_OP base;
    bool wave;
    float opacity;
} M_DRAW_OP_PATTERN;

typedef struct {
    char *path;
    int32_t width;
    int32_t height;
} M_IMAGE_CANDIDATE;

typedef struct {
    bool in_use;
    uint64_t last_used_token;
    char *file_name;

    char *scan_path;
    VECTOR *candidates;

    char *loaded_path;
    float loaded_for_screen_ratio;
    TRX_GL_TEXTURE texture;
    int32_t texture_width;
    int32_t texture_height;
} M_IMAGE_CACHE_ENTRY;

typedef struct {
    bool has_content;
    float captured_brightness;
    TRX_GL_TEXTURE texture;
    int32_t width;
    int32_t height;
} M_SNAPSHOT_STATE;

typedef struct {
    SCENE_SOURCE source;
    MEMORY_ARENA_ALLOCATOR alloc;
    VECTOR *ops[2];
    OUTPUT_QUAD *renderer;
    TRX_GL_TEXTURE solid_black_texture;
    struct {
        OUTPUT_QUAD *renderer;
        uint64_t next_use_token;
        M_IMAGE_CACHE_ENTRY entries[M_IMAGE_CACHE_CAPACITY];
    } image;
    struct {
        OUTPUT_QUAD *renderer;
        M_SNAPSHOT_STATE state;
        bool transition_active;
        FADER transition_fader;
    } snapshot;
    struct {
        OUTPUT_QUAD *renderer;
        bool uploaded;
        int32_t texture_idx;
        int32_t tex_page;
        OUTPUT_QUAD_SURFACE_DESC desc;
        OUTPUT_TEXTURE_SIZE atlas_size;
    } pattern;
} M_PRIV;

static M_PRIV m_Priv = {
    .alloc = {
        .default_chunk_size = 1024 * 4,
    },
};

static bool M_CreateTextureRGB8(
    TRX_GL_TEXTURE *const texture, const int32_t width, const int32_t height,
    const void *const data)
{
    if (texture == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    if (!texture->initialized) {
        TRX_GL_Texture_Init(texture, GL_TEXTURE_2D);
    }
    TRX_GL_Texture_Bind(texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // RGB8 rows are tightly packed (3 bytes per pixel). With the default
    // unpack alignment of 4, OpenGL will assume padding at the end of each row
    // when width * 3 is not a multiple of 4, which looks like an incorrect
    // source stride.
    GLint prev_unpack_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    TRX_GL_CheckError();
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE,
        data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
    TRX_GL_CheckError();
    return true;
}

static void M_CloseTexture(TRX_GL_TEXTURE *const texture)
{
    if (texture != nullptr && texture->initialized) {
        TRX_GL_Texture_Close(texture);
        texture->initialized = false;
    }
}

static float M_GetScreenAspectRatio(void)
{
    const int32_t w = Viewport_GetWidth(VIEWPORT_GAME);
    const int32_t h = Viewport_GetHeight(VIEWPORT_GAME);
    if (w <= 0 || h <= 0) {
        return 1.0f;
    }
    return w / (float)h;
}

static bool M_PrepareViewportCopy(
    const VIEWPORT_SPACE viewport, const int32_t desired_width,
    const int32_t desired_height, VIEWPORT_RECT *const rect,
    int32_t *const copy_width, int32_t *const copy_height)
{
    if (rect == nullptr || copy_width == nullptr || copy_height == nullptr) {
        return false;
    }

    const VIEWPORT_RECT viewport_rect = Viewport_GetRect(viewport);
    if (viewport_rect.width <= 0 || viewport_rect.height <= 0) {
        return false;
    }

    *rect = viewport_rect;
    *copy_width = desired_width;
    *copy_height = desired_height;
    CLAMPG(*copy_width, viewport_rect.width);
    CLAMPG(*copy_height, viewport_rect.height);
    return *copy_width > 0 && *copy_height > 0;
}

static void M_CopyPresentedFrameToTexture(
    TRX_GL_TEXTURE *const texture, const int32_t width, const int32_t height)
{
    if (texture == nullptr || !texture->initialized || width <= 0
        || height <= 0) {
        return;
    }

    VIEWPORT_RECT rect;
    int32_t copy_width = 0;
    int32_t copy_height = 0;
    if (!M_PrepareViewportCopy(
            VIEWPORT_TARGET, width, height, &rect, &copy_width, &copy_height)) {
        return;
    }

    GLint prev_read_fbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
    GLint prev_read_buffer = 0;
    glGetIntegerv(GL_READ_BUFFER, &prev_read_buffer);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_FRONT);

    TRX_GL_Texture_Bind(texture);
    glCopyTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, rect.x, rect.y, copy_width, copy_height);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
    glReadBuffer(prev_read_buffer);
    TRX_GL_CheckError();
}

static void M_EnsureSolidBlackTexture(void)
{
    if (m_Priv.solid_black_texture.initialized) {
        return;
    }
    const uint8_t pixel[3] = { 0, 0, 0 };
    M_CreateTextureRGB8(&m_Priv.solid_black_texture, 1, 1, &pixel[0]);
}

static void M_ImageCandidates_Free(M_IMAGE_CACHE_ENTRY *const e)
{
    if (e->candidates == nullptr) {
        Memory_FreePointer(&e->scan_path);
        return;
    }

    for (int32_t i = 0; i < e->candidates->count; i++) {
        M_IMAGE_CANDIDATE *const candidate = Vector_Get(e->candidates, i);
        Memory_FreePointer(&candidate->path);
    }
    Vector_Free(e->candidates);
    e->candidates = nullptr;
    Memory_FreePointer(&e->scan_path);
}

static void M_ImageCandidates_Scan(
    M_IMAGE_CACHE_ENTRY *const e, const char *const base_image_path)
{
    ASSERT(e != nullptr);
    ASSERT(base_image_path != nullptr);

    M_ImageCandidates_Free(e);

    LOG_INFO("Searching for overlay images");
    VECTOR *candidates = nullptr;

    const char *last_slash = strrchr(base_image_path, '/');
    const char *last_backslash = strrchr(base_image_path, '\\');
    const char *last_sep =
        last_slash > last_backslash ? last_slash : last_backslash;

    size_t dir_len = 0;
    char *dir_path = nullptr;
    if (last_sep != nullptr) {
        dir_len = (size_t)(last_sep - base_image_path);
        dir_path = String_Format("%.*s", (int)dir_len, base_image_path);
    } else {
        dir_path = Memory_DupStr(".");
    }

    const char *const file_name =
        last_sep != nullptr ? last_sep + 1 : base_image_path;

    void *const dir_handle = File_OpenDirectory(dir_path);
    if (dir_handle == nullptr) {
        e->scan_path = dir_path;
        return;
    }

    candidates = Vector_Create(sizeof(M_IMAGE_CANDIDATE));
    const char *entry;
    while ((entry = File_ReadDirectory(dir_handle)) != nullptr) {
        // Match the file itself, and assume it's of 16:9 aspect ratio.
        if (String_Equivalent(entry, file_name)) {
            Vector_Add(
                candidates,
                &(M_IMAGE_CANDIDATE) {
                    .path = String_Format("%s/%s", dir_path, file_name),
                    .width = 16,
                    .height = 9,
                });
        }

        // Match directories with pattern: <width>x<height>
        int32_t w = 0;
        int32_t h = 0;
        if (sscanf(entry, "%dx%d", &w, &h) == 2) {
            const char *const candidate_path =
                String_FormatStatic("%s/%s/%s", dir_path, entry, file_name);
            if (File_Exists(candidate_path)) {
                Vector_Add(
                    candidates,
                    &(M_IMAGE_CANDIDATE) {
                        .path = Memory_DupStr(candidate_path),
                        .width = w,
                        .height = h,
                    });
            }
        }
    }
    File_CloseDirectory(dir_handle);

    for (int32_t i = 0; i < candidates->count; i++) {
        const M_IMAGE_CANDIDATE *const candidate = Vector_Get(candidates, i);
        LOG_INFO(
            "%d. %s (%d:%d)", i + 1, candidate->path, candidate->width,
            candidate->height);
    }

    e->scan_path = dir_path;
    e->candidates = candidates;
}

static const M_IMAGE_CANDIDATE *M_ImageCandidates_PickBest(
    const M_IMAGE_CACHE_ENTRY *const e, const float screen_ratio)
{
    if (e->candidates == nullptr) {
        return nullptr;
    }
    int32_t best_idx = -1;
    float best_err = FLT_MAX;
    const M_IMAGE_CANDIDATE *const raw = Vector_GetData(e->candidates);
    for (int32_t i = 0; i < e->candidates->count; i++) {
        const float candidate_ratio = raw[i].width / (float)raw[i].height;
        const float err = M_RELATIVE_ERROR(candidate_ratio, screen_ratio);
        if (err < best_err) {
            best_err = err;
            best_idx = i;
        }
    }
    return best_idx >= 0 ? &raw[best_idx] : nullptr;
}

static bool M_Image_LoadIntoTexture(
    M_IMAGE_CACHE_ENTRY *const e, const char *const path)
{
    ASSERT(e != nullptr);
    ASSERT(path != nullptr);

    IMAGE *const img = Image_CreateFromFile(path);
    if (img == nullptr) {
        return false;
    }

    const int32_t width = img->width;
    const int32_t height = img->height;
    if (width <= 0 || height <= 0 || img->data == nullptr) {
        Image_Free(img);
        return false;
    }

    const bool ok = M_CreateTextureRGB8(&e->texture, width, height, img->data);
    Image_Free(img);
    if (!ok) {
        return false;
    }

    e->texture_width = width;
    e->texture_height = height;
    return true;
}

static void M_ImageCacheEntry_Reset(M_IMAGE_CACHE_ENTRY *const e)
{
    ASSERT(e != nullptr);

    M_CloseTexture(&e->texture);
    Memory_FreePointer(&e->file_name);
    Memory_FreePointer(&e->loaded_path);
    M_ImageCandidates_Free(e);
    *e = (M_IMAGE_CACHE_ENTRY) { 0 };
}

static M_IMAGE_CACHE_ENTRY *M_ImageCache_GetEntry(
    M_PRIV *const p, const char *const file_name)
{
    ASSERT(p != nullptr);
    ASSERT(file_name != nullptr);

    for (int32_t i = 0; i < M_IMAGE_CACHE_CAPACITY; i++) {
        M_IMAGE_CACHE_ENTRY *const e = &p->image.entries[i];
        if (e->in_use && e->file_name != nullptr
            && String_Equivalent(e->file_name, file_name)) {
            return e;
        }
    }

    for (int32_t i = 0; i < M_IMAGE_CACHE_CAPACITY; i++) {
        M_IMAGE_CACHE_ENTRY *const e = &p->image.entries[i];
        if (!e->in_use) {
            e->in_use = true;
            e->file_name = Memory_DupStr(file_name);
            return e;
        }
    }

    int32_t evict_idx = 0;
    uint64_t best_token = p->image.entries[0].last_used_token;
    for (int32_t i = 1; i < M_IMAGE_CACHE_CAPACITY; i++) {
        if (p->image.entries[i].last_used_token < best_token) {
            best_token = p->image.entries[i].last_used_token;
            evict_idx = i;
        }
    }

    M_ImageCacheEntry_Reset(&p->image.entries[evict_idx]);
    p->image.entries[evict_idx].in_use = true;
    p->image.entries[evict_idx].file_name = Memory_DupStr(file_name);
    return &p->image.entries[evict_idx];
}

static bool M_ImageCache_Load(M_IMAGE_CACHE_ENTRY *const e)
{
    ASSERT(e != nullptr);
    ASSERT(e->file_name != nullptr);

    if (e->candidates == nullptr && e->scan_path == nullptr) {
        M_ImageCandidates_Scan(e, e->file_name);
    }

    const float screen_ratio = M_GetScreenAspectRatio();
    const bool should_reselect =
        e->loaded_path == nullptr || e->loaded_for_screen_ratio != screen_ratio;

    if (e->texture.initialized && !should_reselect) {
        return true;
    }

    const M_IMAGE_CANDIDATE *const best =
        M_ImageCandidates_PickBest(e, screen_ratio);
    if (best != nullptr) {
        const bool already_loaded = e->loaded_path != nullptr
            && String_Equivalent(best->path, e->loaded_path);
        if (!already_loaded || !e->texture.initialized) {
            if (M_Image_LoadIntoTexture(e, best->path)) {
                char *prev = e->loaded_path;
                e->loaded_path = Memory_DupStr(best->path);
                Memory_FreePointer(&prev);
                e->loaded_for_screen_ratio = screen_ratio;
                return true;
            }
        } else {
            e->loaded_for_screen_ratio = screen_ratio;
            return true;
        }
    }

    const bool already_loaded = e->loaded_path != nullptr
        && String_Equivalent(e->file_name, e->loaded_path);
    if (!already_loaded || !e->texture.initialized) {
        if (M_Image_LoadIntoTexture(e, e->file_name)) {
            char *prev = e->loaded_path;
            e->loaded_path = Memory_DupStr(e->file_name);
            Memory_FreePointer(&prev);
            e->loaded_for_screen_ratio = screen_ratio;
            return true;
        }
    }

    M_CloseTexture(&e->texture);
    e->texture_width = 0;
    e->texture_height = 0;
    Memory_FreePointer(&e->loaded_path);
    e->loaded_for_screen_ratio = 0.0f;
    return false;
}

static inline void *M_ArenaAlloc(M_PRIV *const p, const size_t sz)
{
    void *const mem = Memory_ArenaAlloc(&p->alloc, sz);
    memset(mem, 0, sz);
    return mem;
}

static inline void M_ScheduleOp(
    M_PRIV *const p, const int32_t queue_id, M_DRAW_OP *const op)
{
    Vector_Add(p->ops[queue_id], &op);
}

static void M_ScheduleOpHelper(
    M_PRIV *const p, const int32_t queue_id, const M_DRAW_OP_FUNC draw_func,
    const size_t size, const M_DRAW_OP *const op_src)
{
    M_DRAW_OP *const op = M_ArenaAlloc(&m_Priv, size);
    memcpy(op, op_src, size);
    op->draw = draw_func;
    M_ScheduleOp(p, queue_id, op);
}

static void M_DrawOp_BlackRectangle(const M_DRAW_OP_BLACK_RECTANGLE *const op)
{
    const M_PRIV *const p = &m_Priv;
    if (op->opacity <= 0.0f) {
        return;
    }
    M_EnsureSolidBlackTexture();
    Output_Quad_SetExternalTexture(
        p->renderer, p->solid_black_texture.id, 1, 1, false);
    Output_Quad_SetEffect(p->renderer, OUTPUT_QUAD_EFFECT_NONE);
    Output_Quad_SetRepeat(p->renderer, 1, 1);
    Output_Quad_SetTextureSize(p->renderer, nullptr);
    Output_Quad_SetOpacity(p->renderer, op->opacity);
    Output_Quad_RenderWithBlend(p->renderer);
}

static void M_DrawOp_Image(const M_DRAW_OP_IMAGE *const op)
{
    const M_PRIV *const p = &m_Priv;
    if (op->texture_id == 0 || op->width <= 0 || op->height <= 0) {
        return;
    }

    Output_Quad_SetExternalTexture(
        p->image.renderer, op->texture_id, op->width, op->height, op->flip_y);
    Output_Quad_SetEffect(p->image.renderer, OUTPUT_QUAD_EFFECT_NONE);
    Output_Quad_SetRepeat(p->image.renderer, 1, 1);
    Output_Quad_SetTextureSize(p->image.renderer, nullptr);
    Output_SetDesaturation(op->desaturation);
    if (op->use_fit) {
        Output_Quad_SetFit(
            p->image.renderer, OUTPUT_QUAD_FIT_SMART, (float)op->width,
            (float)op->height);
    } else {
        Output_Quad_ClearFit(p->image.renderer);
    }
    Output_Quad_SetOpacity(p->image.renderer, op->opacity);
    if (op->opacity >= 1.0f) {
        Output_Quad_Render(p->image.renderer);
    } else {
        Output_Quad_RenderWithBlend(p->image.renderer);
    }
    Output_SetDesaturation(0.0f);
}

static void M_DrawOp_Snapshot(const M_DRAW_OP_SNAPSHOT *const op)
{
    const M_PRIV *const p = &m_Priv;
    if (op->opacity <= 0.0f) {
        return;
    }
    if (op->texture_id == 0 || op->width <= 0 || op->height <= 0) {
        return;
    }

    Output_Quad_SetExternalTexture(
        p->snapshot.renderer, op->texture_id, op->width, op->height, true);
    Output_Quad_SetEffect(p->snapshot.renderer, OUTPUT_QUAD_EFFECT_NONE);
    Output_Quad_SetRepeat(p->snapshot.renderer, 1, 1);
    Output_Quad_SetTextureSize(p->snapshot.renderer, nullptr);
    Output_Quad_ClearFit(p->snapshot.renderer);
    Output_Quad_SetOpacity(p->snapshot.renderer, op->opacity);
    Output_Quad_RenderWithBlend(p->snapshot.renderer);
}

static bool M_EnsurePatternUploaded(M_PRIV *const p)
{
    if (p->pattern.renderer == nullptr) {
        return false;
    }

    const OBJECT *const obj = Object_Get(O_INV_BACKGROUND);
    if (obj == nullptr || !obj->loaded) {
        return false;
    }

    const OBJECT_MESH *const mesh = Object_GetMesh(obj->mesh_idx);
    if (mesh == nullptr || mesh->tex_face4s.count < 1) {
        return false;
    }

    const int32_t texture_idx = mesh->tex_face4s.data[0].texture_idx;
    const OBJECT_TEXTURE *const texture = Output_GetObjectTexture(texture_idx);
    if (texture == nullptr) {
        return false;
    }

    const RGBA_8888 *const page = Output_GetTexturePage32(texture->tex_page);
    if (page == nullptr) {
        return false;
    }

    const OUTPUT_QUAD_SURFACE_DESC desc = {
        .width = TEXTURE_PAGE_WIDTH,
        .height = TEXTURE_PAGE_HEIGHT,
        .bit_count = 32,
        .tex_format = GL_RGBA,
        .tex_type = GL_UNSIGNED_INT_8_8_8_8_REV,
        .uv = {
            {
                texture->uv[0].u / 256.0f / TEXTURE_PAGE_WIDTH,
                texture->uv[0].v / 256.0f / TEXTURE_PAGE_HEIGHT
            },
            {
                texture->uv[1].u / 256.0f / TEXTURE_PAGE_WIDTH,
                texture->uv[1].v / 256.0f / TEXTURE_PAGE_HEIGHT
            },
            {
                texture->uv[2].u / 256.0f / TEXTURE_PAGE_WIDTH,
                texture->uv[2].v / 256.0f / TEXTURE_PAGE_HEIGHT
            },
            {
                texture->uv[3].u / 256.0f / TEXTURE_PAGE_WIDTH,
                texture->uv[3].v / 256.0f / TEXTURE_PAGE_HEIGHT
            },
        },
        .pitch = TEXTURE_PAGE_WIDTH * 2,
    };

    if (!p->pattern.uploaded || p->pattern.texture_idx != texture_idx
        || p->pattern.tex_page != texture->tex_page
        || memcmp(&p->pattern.desc, &desc, sizeof(desc)) != 0) {
        OUTPUT_QUAD_SURFACE_DESC tmp_desc = desc;
        Output_Quad_Upload(p->pattern.renderer, &tmp_desc, (uint8_t *)page);
        p->pattern.uploaded = true;
        p->pattern.texture_idx = texture_idx;
        p->pattern.tex_page = texture->tex_page;
        p->pattern.desc = desc;
        p->pattern.atlas_size = Output_Textures_GetAtlasSize(texture_idx);
    }

    return true;
}

static void M_DrawOp_Pattern(const M_DRAW_OP_PATTERN *const op)
{
    M_PRIV *const p = &m_Priv;
    if (!M_EnsurePatternUploaded(p)) {
        return;
    }
    if (op->opacity <= 0.0f) {
        return;
    }

    const int32_t repeat_y = 6;
    const float screen_ratio = M_GetScreenAspectRatio();
    const int32_t repeat_x = (int32_t)roundf(repeat_y * screen_ratio);

    Output_Quad_SetRepeat(p->pattern.renderer, repeat_x, repeat_y);
    Output_Quad_SetTextureSize(
        p->pattern.renderer,
        &(OUTPUT_QUAD_TEXTURE_SIZE) {
            .x0 = p->pattern.atlas_size.x0,
            .y0 = p->pattern.atlas_size.y0,
            .x1 = p->pattern.atlas_size.x1,
            .y1 = p->pattern.atlas_size.y1,
        });
    Output_Quad_SetEffect(
        p->pattern.renderer,
        op->wave ? OUTPUT_QUAD_EFFECT_WAVE : OUTPUT_QUAD_EFFECT_VIGNETTE);
    Output_Quad_ClearFit(p->pattern.renderer);
    Output_Quad_SetOpacity(p->pattern.renderer, op->opacity);
    if (op->opacity >= 1.0f) {
        Output_Quad_Render(p->pattern.renderer);
    } else {
        Output_Quad_RenderWithBlend(p->pattern.renderer);
    }
}

static void M_EnsureSnapshotTexture(M_PRIV *const p)
{
    const int32_t w = Viewport_GetWidth(VIEWPORT_TARGET);
    const int32_t h = Viewport_GetHeight(VIEWPORT_TARGET);
    if (w <= 0 || h <= 0) {
        return;
    }

    M_SNAPSHOT_STATE *const s = &p->snapshot.state;
    if (s->texture.initialized && s->width == w && s->height == h) {
        return;
    }

    s->width = w;
    s->height = h;
    M_CreateTextureRGB8(&s->texture, s->width, s->height, nullptr);
    Output_Quad_ClearFit(p->snapshot.renderer);
}

static void M_RunQueue(const VECTOR *const queue)
{
    for (int32_t i = 0; i < queue->count; i++) {
        M_DRAW_OP *const op = *(M_DRAW_OP **)Vector_Get(queue, i);
        op->draw(op);
    }
}

static void M_RenderBegin(const SCENE_SOURCE *const source)
{
    M_PRIV *const p = &m_Priv;
    Vector_Clear(p->ops[0]);
    Vector_Clear(p->ops[1]);
    Memory_ArenaReset(&p->alloc);

    if (p->snapshot.transition_active) {
        const float opacity =
            Fader_GetCurrentValue(&p->snapshot.transition_fader);

        if (opacity <= 0.0f || !p->snapshot.state.has_content
            || !p->snapshot.state.texture.initialized) {
            p->snapshot.transition_active = false;
            p->snapshot.state.has_content = false;
            return;
        }

        M_SCHEDULE_OP(
            false, M_DrawOp_Snapshot,
            ((M_DRAW_OP_SNAPSHOT) {
                .texture_id = p->snapshot.state.texture.id,
                .width = p->snapshot.state.width,
                .height = p->snapshot.state.height,
                .opacity = opacity,
            }));

        if (!Fader_IsActive(&p->snapshot.transition_fader)) {
            p->snapshot.transition_active = false;
            p->snapshot.state.has_content = false;
        }
    }
}

static void M_RenderPass(const SCENE_SOURCE *const src, const SCENE_PASS pass)
{
    M_PRIV *const p = &m_Priv;
    if (pass == SCENE_PASS_OVERLAY_PRE_UI) {
        M_RunQueue(p->ops[0]);
    } else if (pass == SCENE_PASS_OVERLAY_POST_UI) {
        M_RunQueue(p->ops[1]);
    }
}

static bool M_IsDirty(const SCENE_SOURCE *const src, const SCENE_PASS pass)
{
    M_PRIV *const p = &m_Priv;
    if (pass == SCENE_PASS_OVERLAY_PRE_UI) {
        return p->ops[0]->count > 0 || p->snapshot.transition_active;
    } else if (pass == SCENE_PASS_OVERLAY_POST_UI) {
        return p->ops[1]->count > 0;
    }
    return false;
}

bool Output_Overlay_LoadImage(const char *const file_name)
{
    if (file_name == nullptr || file_name[0] == '\0') {
        return false;
    }

    M_PRIV *const p = &m_Priv;
    M_IMAGE_CACHE_ENTRY *const e = M_ImageCache_GetEntry(p, file_name);
    if (p->image.next_use_token == 0) {
        p->image.next_use_token = 1;
    }
    e->last_used_token = p->image.next_use_token++;
    return M_ImageCache_Load(e);
}

void Output_Overlay_DrawImage(const char *const file_name)
{
    Output_Overlay_DrawImageMono(file_name, 0.0f);
}

void Output_Overlay_DrawImageMono(
    const char *const file_name, const float intensity)
{
    if (!Output_Overlay_LoadImage(file_name)) {
        return;
    }

    const M_PRIV *const p = &m_Priv;
    for (int32_t i = 0; i < M_IMAGE_CACHE_CAPACITY; i++) {
        const M_IMAGE_CACHE_ENTRY *const e = &p->image.entries[i];
        if (e->in_use && e->file_name != nullptr
            && String_Equivalent(e->file_name, file_name)
            && e->texture.initialized) {
            M_SCHEDULE_OP(
                false, M_DrawOp_Image,
                ((M_DRAW_OP_IMAGE) {
                    .texture_id = e->texture.id,
                    .width = e->texture_width,
                    .height = e->texture_height,
                    .opacity = 1.0f,
                    .desaturation = intensity,
                    .flip_y = false,
                    .use_fit = true,
                }));
            return;
        }
    }
}

void Output_Overlay_CaptureSnapshot(void)
{
    M_PRIV *const p = &m_Priv;
    p->snapshot.transition_active = false;
    p->snapshot.state.has_content = false;

    M_EnsureSnapshotTexture(p);
    if (!p->snapshot.state.texture.initialized) {
        return;
    }

    M_CopyPresentedFrameToTexture(
        &p->snapshot.state.texture, p->snapshot.state.width,
        p->snapshot.state.height);
    p->snapshot.state.has_content = true;

    // Remove the captured brightness so we can reapply the current multiplier.
    p->snapshot.state.captured_brightness = g_Config.visuals.ui_brightness;
    CLAMPL(p->snapshot.state.captured_brightness, 0.001f);
    Output_Quad_SetBrightnessScale(
        p->snapshot.renderer, 1.0f / p->snapshot.state.captured_brightness);
}

void Output_Overlay_DrawSnapshot(const float opacity)
{
    const M_PRIV *const p = &m_Priv;
    if (!p->snapshot.state.has_content
        || !p->snapshot.state.texture.initialized) {
        return;
    }
    if (opacity <= 0.0f) {
        return;
    }

    M_SCHEDULE_OP(
        false, M_DrawOp_Snapshot,
        ((M_DRAW_OP_SNAPSHOT) {
            .texture_id = p->snapshot.state.texture.id,
            .width = p->snapshot.state.width,
            .height = p->snapshot.state.height,
            .opacity = opacity,
        }));
}

void Output_Overlay_DrawPattern(const bool wave)
{
    Output_Overlay_DrawPatternOpacity(wave, 1.0f);
}

void Output_Overlay_DrawPatternOpacity(const bool wave, const float opacity)
{
    M_SCHEDULE_OP(
        false, M_DrawOp_Pattern,
        ((M_DRAW_OP_PATTERN) { .wave = wave, .opacity = opacity }));
}

void Output_Overlay_BeginTransitionFadeOut(
    const float duration, const float start)
{
    M_PRIV *const p = &m_Priv;
    Output_Overlay_CaptureSnapshot();
    if (!p->snapshot.state.has_content) {
        return;
    }

    p->snapshot.transition_active = true;
    Fader_InitTo(&p->snapshot.transition_fader, start, 0.0f, duration);
}

void Output_Overlay_DrawGame(void)
{
    Interpolation_Disable();
    Game_Draw(false);
    Interpolation_Enable();
}

void Output_Overlay_DrawGameMonoCool(const float desaturation)
{
    Output_SetDesaturation(desaturation);
    Output_SetGlobalTint(Color_Mix(
        COLOR_RGB_F_WHITE, ((RGB_F) { 0.666f, 0.666f, 1.0f }), desaturation));
    Output_Overlay_DrawGame();
    Output_SetGlobalTint(COLOR_RGB_F_WHITE);
    Output_SetDesaturation(0.0f);
}

void Output_Overlay_DrawGameMonoWarm(const float desaturation)
{
    Output_SetDesaturation(desaturation);
    Output_SetGlobalTint(Color_Mix(
        COLOR_RGB_F_WHITE, ((RGB_F) { 1.0f, 0.666f, 0.666f }), desaturation));
    Output_Overlay_DrawGame();
    Output_SetGlobalTint(COLOR_RGB_F_WHITE);
    Output_SetDesaturation(0.0f);
}

void Output_Overlay_DrawGameMono(const float desaturation)
{
    Output_SetDesaturation(desaturation);
    Output_Overlay_DrawGame();
    Output_SetDesaturation(0.0f);
}

void Output_Overlay_DrawBlackRectangle(const float opacity, const bool post_ui)
{
    if (opacity > 0.0f) {
        M_SCHEDULE_OP(
            post_ui, M_DrawOp_BlackRectangle,
            ((M_DRAW_OP_BLACK_RECTANGLE) { .opacity = opacity }));
    }
}

void OutputSource_Overlay_Init(void)
{
    M_PRIV *const p = &m_Priv;
    p->renderer = Output_Quad_Create();
    p->image.renderer = Output_Quad_Create();
    p->snapshot.renderer = Output_Quad_Create();
    p->pattern.renderer = Output_Quad_Create();
    p->pattern.uploaded = false;
    M_EnsureSolidBlackTexture();

    p->source.render_begin = M_RenderBegin;
    p->source.render_pass = M_RenderPass;
    p->source.is_dirty = M_IsDirty;
    p->ops[0] = Vector_Create(sizeof(M_DRAW_OP *));
    p->ops[1] = Vector_Create(sizeof(M_DRAW_OP *));
    SceneCompositor_AddSource(&p->source);
}

void OutputSource_Overlay_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->renderer != nullptr) {
        Output_Quad_Destroy(p->renderer);
        p->renderer = nullptr;
    }
    if (p->image.renderer != nullptr) {
        Output_Quad_Destroy(p->image.renderer);
        p->image.renderer = nullptr;
    }
    if (p->snapshot.renderer != nullptr) {
        Output_Quad_Destroy(p->snapshot.renderer);
        p->snapshot.renderer = nullptr;
    }
    if (p->pattern.renderer != nullptr) {
        Output_Quad_Destroy(p->pattern.renderer);
        p->pattern.renderer = nullptr;
    }
    for (int32_t i = 0; i < M_IMAGE_CACHE_CAPACITY; i++) {
        if (p->image.entries[i].in_use) {
            M_ImageCacheEntry_Reset(&p->image.entries[i]);
        }
    }
    p->pattern.uploaded = false;
    M_CloseTexture(&p->snapshot.state.texture);
    p->snapshot.state.has_content = false;
    M_CloseTexture(&p->solid_black_texture);

    if (p->ops[0] != nullptr) {
        Vector_Free(p->ops[0]);
        p->ops[0] = nullptr;
    }
    if (p->ops[1] != nullptr) {
        Vector_Free(p->ops[1]);
        p->ops[1] = nullptr;
    }
    Memory_ArenaFree(&p->alloc);
}
