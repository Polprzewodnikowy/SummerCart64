#pragma once

typedef enum {
    SCENE_PASS_BACKGROUND,
    SCENE_PASS_OPAQUE,
    SCENE_PASS_TRANSPARENT,
    SCENE_PASS_BLEND_SUB,
    SCENE_PASS_BLEND_ADD,
    SCENE_PASS_OVERLAY_PRE_UI,
    SCENE_PASS_UI,
    SCENE_PASS_OVERLAY_POST_UI,
    SCENE_PASS_COUNT,
} SCENE_PASS;

typedef struct SCENE_SOURCE {
    void (*render_begin)(const struct SCENE_SOURCE *);
    void (*render_pass)(const struct SCENE_SOURCE *, SCENE_PASS pass);
    void (*render_end)(const struct SCENE_SOURCE *);
    bool (*is_dirty)(const struct SCENE_SOURCE *, SCENE_PASS pass);
    void (*animate_textures)(const struct SCENE_SOURCE *);
    void *priv;
} SCENE_SOURCE;
