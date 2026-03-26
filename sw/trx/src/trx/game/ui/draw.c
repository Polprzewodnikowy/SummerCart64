#include <trx/game/ui/draw.h>

#include <trx/core/memory.h>
#include <trx/core/vector.h>
#include <trx/game/objects.h>
#include <trx/game/output/common.h>
#include <trx/game/output/sources/ui.h>
#include <trx/game/output/state.h>
#include <trx/game/ui/scaler.h>
#include <trx/game/ui/settings.h>
#include <trx/version.h>

#include <string.h>

#define M_WHITE ((RGBA_F) { 1.0f, 1.0f, 1.0f, 1.0f })
#define M_OUTLINE_THICKNESS 0.75f
#define M_SCHEDULE_OP(draw_func, inst)                                         \
    M_ScheduleOpHelper(                                                        \
        (M_DRAW_OP_FUNC)draw_func, sizeof(inst), (const M_DRAW_OP *)&inst);

struct M_DRAW_OP;
typedef void (*M_DRAW_OP_FUNC)(const struct M_DRAW_OP *);

typedef struct M_DRAW_OP {
    M_DRAW_OP_FUNC draw;
} M_DRAW_OP;

typedef struct {
    M_DRAW_OP base;
    UI_STYLE ui_style;
    int32_t x0, x1, y, z;
} M_DRAW_OP_HORZ_LINE;

typedef struct {
    M_DRAW_OP base;
    UI_STYLE ui_style;
    int32_t x0, y0, x1, y1, z;
    TEXT_STYLE text_style;
} M_DRAW_OP_TEXT_RECT;

typedef struct {
    M_DRAW_OP base;
    int32_t sx, sy, z, scale_h, scale_v, sprite_idx;
    const RGBA_F colors[4];
} M_DRAW_OP_SPRITE;

typedef struct {
    M_DRAW_OP base;
    int32_t x0, y0, x1, y1, z;
    RGBA_8888 tl, tr, bl, br;
} M_DRAW_OP_QUAD;

typedef struct {
    MEMORY_ARENA_ALLOCATOR alloc;
    VECTOR *ops;
} M_PRIV;

static M_PRIV m_Priv = {
    .alloc = {
        .default_chunk_size = 1024 * 4,
    },
};

static void M_DrawScreenQuad(
    const int32_t x0, const int32_t y0, const int32_t x1, const int32_t y1,
    const int32_t z, const RGBA_8888 tl, const RGBA_8888 tr, const RGBA_8888 bl,
    const RGBA_8888 br)
{
    OutputSource_UI_StageQuad((OUTPUT_UI_QUAD) {
        .x0 = x0,
        .y0 = y0,
        .x1 = x1,
        .y1 = y1,
        .tl = tl,
        .tr = tr,
        .bl = bl,
        .br = br,
        .z = Output_GetNearZ_UI() + z,
    });
}

static void M_DrawScreenSprite(
    const int32_t sx, const int32_t sy, const int32_t sz, const int32_t scale_h,
    const int32_t scale_v, const int32_t sprite_idx, const RGBA_F colors[4])
{
    Output_DrawScreenSprite(sx, sy, sz, scale_h, scale_v, sprite_idx, colors);
}

static void M_DrawScreenGradientBox(
    const int32_t x0, const int32_t y0, const int32_t x1, const int32_t y1,
    const int32_t z, const RGBA_8888 tl, const RGBA_8888 tr, const RGBA_8888 bl,
    const RGBA_8888 br, const float thickness)
{
    const float e = UI_Scaler_Calc(thickness, UI_SCALER_TARGET_TEXT);
    M_DrawScreenQuad(x0 - e, y0 - e, x1 + e, y0 + e, z, tl, tr, tl, tr);
    M_DrawScreenQuad(x0 - e, y1 - e, x1 + e, y1 + e, z, bl, br, bl, br);
    M_DrawScreenQuad(x0 - e, y0 - e, x0 + e, y1 + e, z, tl, tl, bl, bl);
    M_DrawScreenQuad(x1 - e, y0 - e, x1 + e, y1 + e, z, tr, tr, br, br);
}

static void M_DrawScreenCentreGradientBox(
    const int32_t x0, const int32_t y0, const int32_t x1, const int32_t y1,
    const int32_t z, const RGBA_8888 edge, const RGBA_8888 center_h,
    const RGBA_8888 center_v, const float thickness)
{
    const float e = UI_Scaler_Calc(thickness, UI_SCALER_TARGET_TEXT);
    const int32_t xm = (x0 + x1) / 2;
    const int32_t ym = (y0 + y1) / 2;
    const RGBA_8888 ch = center_h;
    const RGBA_8888 cv = center_v;
    const RGBA_8888 ce = edge;

    // clang-format off
    M_DrawScreenQuad(x0 - e, y0 - e, xm,     y0 + e, z, ce, ch, ce, ch);
    M_DrawScreenQuad(xm,     y0 - e, x1 + e, y0 + e, z, ch, ce, ch, ce);
    M_DrawScreenQuad(x0 - e, y1 - e, xm,     y1 + e, z, ce, ch, ce, ch);
    M_DrawScreenQuad(xm,     y1 - e, x1 + e, y1 + e, z, ch, ce, ch, ce);
    M_DrawScreenQuad(x0 - e, y0,     x0 + e, ym,     z, ce, ce, cv, cv);
    M_DrawScreenQuad(x0 - e, ym,     x0 + e, y1,     z, cv, cv, ce, ce);
    M_DrawScreenQuad(x1 - e, y0,     x1 + e, ym,     z, ce, ce, cv, cv);
    M_DrawScreenQuad(x1 - e, ym,     x1 + e, y1,     z, cv, cv, ce, ce);
    // clang-format on
}

static void M_DrawOp_HorizontalLine(const M_DRAW_OP_HORZ_LINE *const op)
{
    if (g_TRVersion == 1 && op->ui_style == UI_STYLE_PC) {
        const float e =
            UI_Scaler_Calc(M_OUTLINE_THICKNESS, UI_SCALER_TARGET_TEXT);
        const UI_MENU_COLORS_PC *const c = UI_Settings_GetMenuColorsPC();
        M_DrawScreenQuad(
            op->x0, op->y - e, op->x1, op->y, op->z, c->outline_light,
            c->outline_light, c->outline_light, c->outline_light);
        M_DrawScreenQuad(
            op->x0, op->y, op->x1, op->y + e, op->z, c->outline_dark,
            c->outline_dark, c->outline_dark, c->outline_dark);
    } else if (g_TRVersion == 2 && op->ui_style == UI_STYLE_PC) {
        const int32_t mesh_idx = Object_Get(O_TEXT_BOX)->mesh_idx;
        M_DrawScreenSprite(
            op->x0, op->y, op->z, (op->x1 - op->x0) * PHD_ONE / 8, PHD_ONE,
            mesh_idx + 4, (RGBA_F[4]) { M_WHITE, M_WHITE, M_WHITE, M_WHITE });
    } else if (g_TRVersion == 3 && op->ui_style == UI_STYLE_PC) {
        const float e1 = UI_Scaler_Calc(1.0f, UI_SCALER_TARGET_TEXT);
        const float e2 = e1 / 3.0f;
        const UI_MENU_COLORS_PC *const c = UI_Settings_GetMenuColorsPC();
        M_DrawScreenQuad(
            op->x0, op->y - e1, op->x1, op->y + e1, op->z, c->outline_dark,
            c->outline_dark, c->outline_dark, c->outline_dark);
        M_DrawScreenQuad(
            op->x0, op->y - e2, op->x1, op->y + e2, op->z, c->outline_light,
            c->outline_light, c->outline_light, c->outline_light);
    } else {
        const float e =
            UI_Scaler_Calc(M_OUTLINE_THICKNESS, UI_SCALER_TARGET_TEXT);
        const UI_MENU_COLORS_PS1 *const c = UI_Settings_GetMenuColorsPS1();
        M_DrawScreenQuad(
            op->x0, op->y - e, op->x1, op->y + e, op->z, c->outline_bl,
            c->outline_br, c->outline_bl, c->outline_br);
    }
}

static void M_DrawOp_TextBackground(const M_DRAW_OP_TEXT_RECT *const op)
{
    switch (op->ui_style) {
    case UI_STYLE_PC: {
        const UI_MENU_COLORS_PC *const c = UI_Settings_GetMenuColorsPC();
        const RGBA_8888 *const ramp = op->text_style == TS_BACKGROUND_HEAVY
            ? c->background_heavy
            : c->background;
        M_DrawScreenQuad(
            op->x0, op->y0, op->x1, op->y1, op->z, ramp[0], ramp[0], ramp[1],
            ramp[1]);
        break;
    }

    case UI_STYLE_PS1: {
        const UI_MENU_COLORS_PS1 *const c = UI_Settings_GetMenuColorsPS1();
        const int32_t xm = (op->x0 + op->x1) / 2;
        const int32_t ym = (op->y0 + op->y1) / 2;
        RGBA_8888 edge, center;
        switch (op->text_style) {
        case TS_BACKGROUND_HEAVY:
            edge = c->background_heavy_edge;
            center = c->background_heavy_center;
            break;
        case TS_HEADING:
            edge = c->heading_edge;
            center = c->heading_center;
            break;
        case TS_REQUESTED:
            edge = c->requested_edge;
            center = c->requested_center;
            break;
        default:
            edge = c->background_edge;
            center = c->background_center;
            break;
        }

        // clang-format off
#define L_DRAW(x0, y0, x1, y1, tl, tr, bl, br) \
    M_DrawScreenQuad(x0, y0, x1, y1, op->z, tl, tr, bl, br);
        L_DRAW(xm,     op->y0, op->x0, ym,     edge,   edge,   center, edge  );
        L_DRAW(op->x1, op->y0, xm,     ym,     edge,   edge,   edge,   center);
        L_DRAW(xm,     ym,     op->x0, op->y1, center, edge,   edge,   edge  );
        L_DRAW(op->x1, ym,     xm,     op->y1, edge,   center, edge,   edge  );
#undef L_DRAW
        // clang-format on
        break;
    }
    }
}

static void M_DrawOp_TextOutline(const M_DRAW_OP_TEXT_RECT *const op)
{
    int32_t x0 = op->x0;
    int32_t x1 = op->x1;
    int32_t y0 = op->y0;
    int32_t y1 = op->y1;

    switch (op->ui_style) {
    case UI_STYLE_PC:
        if (g_TRVersion == 1) {
            const UI_MENU_COLORS_PC *const c = UI_Settings_GetMenuColorsPC();
            const float thickness =
                UI_Scaler_Calc(M_OUTLINE_THICKNESS, UI_SCALER_TARGET_TEXT);
            Output_DrawScreenFrame(
                x0, y0, x1 - x0, y1 - y0, c->outline_dark, c->outline_light,
                thickness);
        } else if (g_TRVersion == 2) {
            const int32_t mesh_idx = Object_Get(O_TEXT_BOX)->mesh_idx;

            const int32_t offset = 4;
            x0 += offset;
            y0 += offset;
            x1 -= offset;
            y1 -= offset;
            const int32_t scale_h = PHD_ONE;
            const int32_t scale_v = PHD_ONE;
            const int32_t w = (x1 - x0) * PHD_ONE / 8;
            const int32_t h = (y1 - y0) * PHD_ONE / 8;

            const RGBA_F neutral[4] = { M_WHITE, M_WHITE, M_WHITE, M_WHITE };

            // Corners
            M_DrawScreenSprite(
                x0, y0, op->z, scale_h, scale_v, mesh_idx + 0, neutral);
            M_DrawScreenSprite(
                x1, y0, op->z, scale_h, scale_v, mesh_idx + 1, neutral);
            M_DrawScreenSprite(
                x1, y1, op->z, scale_h, scale_v, mesh_idx + 2, neutral);
            M_DrawScreenSprite(
                x0, y1, op->z, scale_h, scale_v, mesh_idx + 3, neutral);

            // Lines
            M_DrawScreenSprite(
                x0, y0, op->z, w, scale_v, mesh_idx + 4, neutral);
            M_DrawScreenSprite(
                x1, y0, op->z, scale_h, h, mesh_idx + 5, neutral);
            M_DrawScreenSprite(
                x0, y1, op->z, w, scale_v, mesh_idx + 6, neutral);
            M_DrawScreenSprite(
                x0, y0, op->z, scale_h, h, mesh_idx + 7, neutral);
        } else if (g_TRVersion == 3) {
            const UI_MENU_COLORS_PC *const c = UI_Settings_GetMenuColorsPC();
            const float thickness = UI_Scaler_Calc(1.0f, UI_SCALER_TARGET_TEXT);
            Output_DrawScreenFrame(
                x0, y0, x1 - x0, y1 - y0, c->outline_dark, c->outline_dark,
                thickness);
            Output_DrawScreenFrame(
                x0, y0, x1 - x0, y1 - y0, c->outline_light, c->outline_light,
                thickness / 3.0f);
        }

        break;

    case UI_STYLE_PS1: {
        const UI_MENU_COLORS_PS1 *const c = UI_Settings_GetMenuColorsPS1();
        switch (op->text_style) {
        case TS_BACKGROUND:
        case TS_BACKGROUND_HEAVY:
            M_DrawScreenGradientBox(
                x0, y0, x1, y1, op->z, c->outline_tl, c->outline_tr,
                c->outline_bl, c->outline_br, M_OUTLINE_THICKNESS);
            break;
        case TS_HEADING:
            M_DrawScreenGradientBox(
                x0, y0, x1, y1, op->z, c->heading_outline, c->heading_outline,
                c->heading_outline, c->heading_outline, M_OUTLINE_THICKNESS);
            break;
        case TS_REQUESTED:
            M_DrawScreenCentreGradientBox(
                x0, y0, x1, y1, op->z, c->requested_outline_edge,
                c->requested_outline_ch, c->requested_outline_cv,
                M_OUTLINE_THICKNESS);
            break;
        }
        break;
    }
    }
}

static void M_DrawOp_Sprite(const M_DRAW_OP_SPRITE *const op)
{
    Output_DrawScreenSprite(
        op->sx, op->sy, op->z, op->scale_h, op->scale_v, op->sprite_idx,
        op->colors);
}

static void M_DrawOp_Quad(const M_DRAW_OP_QUAD *const op)
{
    M_DrawScreenQuad(
        op->x0, op->y0, op->x1, op->y1, op->z, op->tl, op->tr, op->bl, op->br);
}

// Allocate a new deferred draw operation in the arena.
static inline void *M_ArenaAlloc(const size_t sz)
{
    void *p = Memory_ArenaAlloc(&m_Priv.alloc, sz);
    memset(p, 0, sz);
    return p;
}

static inline void M_ScheduleOp(M_DRAW_OP *const op)
{
    M_PRIV *const p = &m_Priv;
    Vector_Add(p->ops, &op);
}

static void M_ScheduleOpHelper(
    const M_DRAW_OP_FUNC draw_func, const size_t size,
    const M_DRAW_OP *const op_src)
{
    M_DRAW_OP *const op = M_ArenaAlloc(size);
    memcpy(op, op_src, size);
    op->draw = draw_func;
    M_ScheduleOp(op);
}

void UI_ScheduleDrawTextBackground(
    const UI_STYLE ui_style, const int32_t sx, const int32_t sy,
    const int32_t z, const int32_t w, const int32_t h,
    const TEXT_STYLE text_style)
{
    M_SCHEDULE_OP(
        M_DrawOp_TextBackground,
        ((M_DRAW_OP_TEXT_RECT) {
            .ui_style = ui_style,
            .x0 = sx,
            .y0 = sy,
            .x1 = sx + w,
            .y1 = sy + h,
            .z = z,
            .text_style = text_style,
        }));
}

void UI_ScheduleDrawTextOutline(
    const UI_STYLE ui_style, const int32_t sx, const int32_t sy,
    const int32_t z, const int32_t w, const int32_t h,
    const TEXT_STYLE text_style)
{
    M_SCHEDULE_OP(
        M_DrawOp_TextOutline,
        ((M_DRAW_OP_TEXT_RECT) {
            .ui_style = ui_style,
            .x0 = sx,
            .y0 = sy,
            .x1 = sx + w,
            .y1 = sy + h,
            .z = z,
            .text_style = text_style,
        }));
}

void UI_ScheduleDrawScreenSprite(
    const int32_t sx, const int32_t sy, const int32_t z, const int32_t scale_h,
    const int32_t scale_v, const int32_t sprite_idx, const RGBA_F colors_[4])
{
    M_SCHEDULE_OP(
        M_DrawOp_Sprite,
        ((M_DRAW_OP_SPRITE) {
            .sx = sx,
            .sy = sy,
            .z = z,
            .scale_h = scale_h,
            .scale_v = scale_v,
            .sprite_idx = sprite_idx,
            .colors = {
                [0] = colors_[0],
                [1] = colors_[1],
                [2] = colors_[2],
                [3] = colors_[3],
            },
        }));
}

void UI_ScheduleDrawScreenFlatQuad(
    const int32_t sx, const int32_t sy, const int32_t z, const int32_t w,
    const int32_t h, const RGBA_8888 color)
{
    M_SCHEDULE_OP(
        M_DrawOp_Quad,
        ((M_DRAW_OP_QUAD) {
            .x0 = sx,
            .y0 = sy,
            .x1 = sx + w,
            .y1 = sy + h,
            .z = z,
            .tl = color,
            .tr = color,
            .bl = color,
            .br = color,
        }));
}

void UI_ScheduleDrawScreenGradientQuad(
    const int32_t sx, const int32_t sy, const int32_t z, const int32_t w,
    const int32_t h, const RGBA_8888 tl, const RGBA_8888 tr, const RGBA_8888 bl,
    const RGBA_8888 br)
{
    M_SCHEDULE_OP(
        M_DrawOp_Quad,
        ((M_DRAW_OP_QUAD) {
            .x0 = sx,
            .y0 = sy,
            .x1 = sx + w,
            .y1 = sy + h,
            .z = z,
            .tl = tl,
            .tr = tr,
            .bl = bl,
            .br = br,
        }));
}

void UI_ScheduleDrawHorizontalLine(
    const UI_STYLE ui_style, const int32_t x0, const int32_t x1,
    const int32_t y, const int32_t z)
{
    M_SCHEDULE_OP(
        M_DrawOp_HorizontalLine,
        ((M_DRAW_OP_HORZ_LINE) {
            .ui_style = ui_style,
            .x0 = x0,
            .x1 = x1,
            .y = y,
            .z = z,
        }));
}

void UI_InitDraw(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->ops == nullptr) {
        p->ops = Vector_Create(sizeof(M_DRAW_OP *));
    }
}

void UI_ShutdownDraw(void)
{
    M_PRIV *const p = &m_Priv;
    Memory_ArenaFree(&p->alloc);
    if (p->ops != nullptr) {
        Vector_Free(p->ops);
        p->ops = nullptr;
    }
}

void UI_ClearDraw(void)
{
    M_PRIV *const p = &m_Priv;
    Vector_Clear(p->ops);
    Memory_ArenaReset(&p->alloc);
}

void UI_Draw(void)
{
    M_PRIV *const p = &m_Priv;
    for (int32_t i = 0; i < m_Priv.ops->count; i++) {
        const M_DRAW_OP *const op = *(M_DRAW_OP **)Vector_Get(m_Priv.ops, i);
        op->draw(op);
    }
}
