#include <trx/game/ui/text.h>

#include <trx/config.h>
#include <trx/core/enum_map.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/input/common.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/scaler.h>
#include <trx/version.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <uthash.h>

#define M_LETTER_SPACING 0.5f
#define M_WORD_SPACING 6.0f
#define M_DIM_COLOR 12
#define M_MAX_COLOR 13

typedef enum {
    M_FONT_DEFAULT = 0,
    M_FONT_SMALL = 1,
    M_FONT_COUNT,
} M_FONT;

typedef enum {
    // A text character.
    GLYPH_TEXT,
    // An icon.
    GLYPH_ICON,
    // Spacing between words.
    GLYPH_SPACE,
    // Line break.
    GLYPH_NEW_LINE,
    // Marker used in the examine item dialog and others to force a new page.
    GLYPH_NEW_PAGE,
    // Icon for collectible secrets, taking the sprite from O_SECRET
    GLYPH_SECRET,
    // Icon requesting translators to verify AI-translated text.
    GLYPH_REVIEW_MARKER,
    // Marker that toggles the visibility of the following text.
    GLYPH_VISIBILITY_MARKER,
    // Marker that toggles the dimming of the following text.
    GLYPH_DIM_MARKER,
    // Marker that changes the color of the following text.
    GLYPH_COLOR_MARKER,
    // Marker that changes the font of the following text.
    // - mesh_idx = 0: default font (O_ALPHABET).
    // - mesh_idx = 1: default font (O_ALPHABET_SMALL).
    GLYPH_FONT_MARKER,
    // Glyph that dynamically expands a key role to its current key icon.
    GLYPH_INPUT,
} M_GLYPH_ROLE;

typedef struct {
    const char *text;
    M_GLYPH_ROLE role;
    int32_t width[M_FONT_COUNT];
    union {
        int32_t mesh_idx;
        INPUT_ROLE input_role; // for role == GLYPH_INPUT
    };
} M_GLYPH_INFO;

typedef struct {
    M_GLYPH_INFO *glyph;
    UT_hash_handle hh;
} M_GLYPH_MAP_ENTRY;

typedef struct {
    char *text;
    const M_GLYPH_INFO **glyphs;
    size_t glyph_count;
    UT_hash_handle hh;
} M_TEXT_MAP_ENTRY;

static M_GLYPH_INFO m_Glyphs[] = {
#define X_GLYPH_DEFINE(text_, role_, mesh_idx_)                                \
    { .text = text_, .role = role_, .mesh_idx = mesh_idx_ },
#include <trx/game/ui/text.def>
    { .text = nullptr }, // guard
};

static M_GLYPH_MAP_ENTRY *m_GlyphMap = nullptr;
static M_TEXT_MAP_ENTRY *m_TextMap = nullptr;

OBJECT_ID m_FontObjects[M_FONT_COUNT] = {
    [M_FONT_DEFAULT] = O_ALPHABET,
    [M_FONT_SMALL] = O_ALPHABET_SMALL,
};

static RGB_888 m_ColorLight[M_MAX_COLOR] = {
    // clang-format off
    [0]  = { 0xFF, 0xFF, 0xFF },
    [1]  = { 0xB0, 0xB0, 0x00 },
    [2]  = { 0xA0, 0xA0, 0xA0 },
    [3]  = { 0xFF, 0x60, 0x60 },
    [4]  = { 0x80, 0x80, 0xFF },
    [5]  = { 0xC0, 0x80, 0x40 },
    [6]  = { 0xB6, 0xD1, 0x64 },
    [7]  = { 0xC0, 0xFF, 0xC0 },
    [8]  = { 0xFF, 0xFF, 0xFF },
    [9]  = { 0xFF, 0x00, 0xFF },
    [10] = { 0xFF, 0x00, 0xFF },
    [11] = { 0xFF, 0x00, 0xFF },
    [12] = { 0x80, 0x80, 0x80 },
    // clang-format on
};

static RGB_888 m_ColorDark[M_MAX_COLOR] = {
    // clang-format off
    [0]  = { 0x80, 0x80, 0x80 },
    [1]  = { 0x50, 0x50, 0x00 },
    [2]  = { 0x18, 0x18, 0x18 },
    [3]  = { 0x18, 0x00, 0x00 },
    [4]  = { 0x00, 0x00, 0x18 },
    [5]  = { 0x40, 0x10, 0x00 },
    [6]  = { 0xB6, 0x20, 0x13 },
    [7]  = { 0xC0, 0xFF, 0xC0 },
    [8]  = { 0xFF, 0xFF, 0xFF },
    [9]  = { 0x3F, 0x00, 0x3F },
    [10] = { 0x3F, 0x00, 0x3F },
    [11] = { 0x3F, 0x00, 0x3F },
    [12] = { 0x80, 0x80, 0x80 },
    // clang-format on
};

static RGBA_F m_TextColor[M_MAX_COLOR][4] = {};

static float M_ScaleScreen(const float value)
{
    return UI_Scaler_Calc(value, UI_SCALER_TARGET_TEXT);
}

static float M_ScaleNeutral(const float value)
{
    return value * g_Config.ui.text_scale;
}

static RGBA_F M_ToRGBA_F(const RGB_888 color)
{
    return (RGBA_F) {
        .r = color.r / 255.0f,
        .g = color.g / 255.0f,
        .b = color.b / 255.0f,
        .a = 1.0f,
    };
}

static int32_t M_HasGlyph(const M_FONT font, const M_GLYPH_INFO *const glyph)
{
    return glyph->width[font] > 0;
}

static int32_t M_GetGlyphWidth(
    const M_FONT font, const M_GLYPH_INFO *const glyph)
{
    // Non-breaking space
    if (strcmp(glyph->text, " ") == 0) {
        return M_WORD_SPACING;
    }

    if (glyph->role == GLYPH_SECRET) {
        return 16;
    }

    if (glyph->mesh_idx != -1
        && (glyph->role == GLYPH_TEXT || glyph->role == GLYPH_ICON
            || glyph->role == GLYPH_REVIEW_MARKER)) {
        const OBJECT *const object = Object_Get(m_FontObjects[font]);
        if (!object->loaded) {
            return -1;
        }
        if (glyph->mesh_idx >= ABS(object->mesh_count)) {
            return -1;
        }
        const SPRITE_TEXTURE *const sprite =
            Output_GetSpriteTexture(object->mesh_idx + glyph->mesh_idx);
        if (sprite == nullptr) {
            return -1;
        }
        if (sprite->x1 - sprite->x0 == 0 && sprite->width / 255 == 1) {
            // Just a placeholder glyph necessary for indexing of other glyphs
            return -1;
        }
        return sprite->width / 255;
    }

    return 0;
}

static const M_GLYPH_INFO **M_Decompose(
    const char *const content, size_t *const out_glyph_count)
{
    // Count number of characters
    size_t glyph_count = 0;
    const char *content_ptr = content;
    while (*content_ptr != '\0') {
        const size_t glyph_size = String_GetCharByteSize(content_ptr);
        content_ptr += glyph_size;
        glyph_count++;
    }

    // Assign glyphs using hash table
    const M_GLYPH_INFO **glyphs =
        Memory_Alloc((glyph_count + 1) * sizeof(M_GLYPH_INFO *));
    content_ptr = content;
    const M_GLYPH_INFO **glyph_ptr = glyphs;
    while (*content_ptr != '\0') {
        const size_t glyph_size = String_GetCharByteSize(content_ptr);
        const char *const key_buf =
            String_FormatStatic("%.*s", (int)glyph_size, content_ptr);
        M_GLYPH_MAP_ENTRY *entry;
        HASH_FIND_STR(m_GlyphMap, key_buf, entry);

        if (entry != nullptr) {
            *glyph_ptr++ = entry->glyph;
        } else {
            LOG_WARNING("Unknown glyph: %s", key_buf);
            glyph_count--;
        }

        content_ptr += glyph_size;
    }

    if (out_glyph_count != nullptr) {
        *out_glyph_count = glyph_count;
    }

    // guard
    *glyph_ptr++ = nullptr;
    return glyphs;
}

static const M_GLYPH_INFO **M_DecomposeWithCache(
    const char *const content, size_t *const out_glyph_count)
{
    M_TEXT_MAP_ENTRY *entry;
    HASH_FIND_STR(m_TextMap, content, entry);
    if (entry == nullptr) {
        entry = Memory_Alloc(sizeof(M_TEXT_MAP_ENTRY));
        entry->text = Memory_DupStr(content);
        entry->glyphs = M_Decompose(content, &entry->glyph_count);
        HASH_ADD_STR(m_TextMap, text, entry);
    }
    if (out_glyph_count != nullptr) {
        *out_glyph_count = entry->glyph_count;
    }
    return entry->glyphs;
}

// Replace input placeholder glyph with the actual keyboard glyph for the
// current binding
static const M_GLYPH_INFO *M_GetResolvedGlyph(const M_GLYPH_INFO *glyph)
{
    if (glyph->role != GLYPH_INPUT) {
        return glyph;
    }
    const char *const key_name = Input_GetKeyName(
        g_Config.input.backend, g_Config.input.layout[g_Config.input.backend],
        glyph->input_role);
    // NOTE: this aliasing approach assumes that Input_GetKeyName returns
    // text that resolves to a single glyph.
    M_GLYPH_MAP_ENTRY *entry = nullptr;
    if (key_name != nullptr) {
        HASH_FIND_STR(m_GlyphMap, key_name, entry);
    }
    if (entry == nullptr) {
        HASH_FIND_STR(m_GlyphMap, "?", entry);
    }
    return entry != nullptr ? entry->glyph : nullptr;
}

static int32_t M_DetectBulletIndent(
    const M_GLYPH_INFO **glyphs, const size_t glyph_count, const size_t idx)
{
    size_t scan = idx;
    int32_t leading_spaces = 0;
    while (scan < glyph_count && glyphs[scan]->role == GLYPH_SPACE) {
        leading_spaces++;
        scan++;
    }
    if (scan + 1 < glyph_count && glyphs[scan]->role == GLYPH_TEXT
        && glyphs[scan]->text[0] == '-' && glyphs[scan]->text[1] == '\0'
        && glyphs[scan + 1]->role == GLYPH_SPACE) {
        return leading_spaces + 2;
    }
    return 0;
}

static void M_EmitIndent(
    char *const dst, size_t *const out_len, const int32_t indent,
    const float space_width, float *const cur_width)
{
    for (int32_t s = 0; s < indent; s++) {
        if (dst != nullptr) {
            dst[*out_len] = ' ';
        }
        (*out_len)++;
    }
    *cur_width += indent * space_width;
}

static void M_EmitNewline(
    char *const dst, size_t *const out_len, const int32_t indent,
    const float space_width, float *const cur_width)
{
    if (dst != nullptr) {
        dst[*out_len] = '\n';
    }
    (*out_len)++;
    *cur_width = 0.0f;
    if (indent > 0) {
        M_EmitIndent(dst, out_len, indent, space_width, cur_width);
    }
}

static size_t M_WordWrap(
    const M_GLYPH_INFO **glyphs, const size_t glyph_count, const float scale_f,
    const float max_width, char *const dst)
{
    size_t out_len = 0;
    float cur_width = 0.0f;
    int32_t bullet_indent = 0;

    const float space_width = M_WORD_SPACING * scale_f;

#define L_CONCAT_CHAR(part)                                                    \
    if (dst != nullptr) {                                                      \
        dst[out_len] = part;                                                   \
    }                                                                          \
    out_len++;
#define L_CONCAT_STR(part)                                                     \
    if (dst != nullptr) {                                                      \
        strcpy(dst + out_len, part);                                           \
    }                                                                          \
    out_len += strlen(part);

    M_FONT current_font = M_FONT_DEFAULT;

    // Iterate glyphs for wrapping
    for (size_t i = 0; i < glyph_count; i++) {
        const M_GLYPH_INFO *const glyph = M_GetResolvedGlyph(glyphs[i]);
        if (glyph == nullptr) {
            continue;
        }

        if (cur_width == 0.0f && bullet_indent == 0) {
            bullet_indent = M_DetectBulletIndent(glyphs, glyph_count, i);
        }

        if (glyph->role == GLYPH_FONT_MARKER) {
            current_font = glyph->mesh_idx;
        } else if (glyph->role == GLYPH_NEW_LINE) {
            L_CONCAT_CHAR('\n')
            cur_width = 0.0f;
            bullet_indent = 0;
        } else if (glyph->role == GLYPH_NEW_PAGE) {
            L_CONCAT_CHAR('\f')
            cur_width = 0.0f;
            bullet_indent = 0;
        } else if (glyph->role == GLYPH_SPACE) {
            const float w = M_WORD_SPACING * scale_f;
            if (cur_width + w > max_width) {
                M_EmitNewline(
                    dst, &out_len, bullet_indent, space_width, &cur_width);
            } else {
                L_CONCAT_CHAR(' ')
                cur_width += w;
            }
        } else if (
            glyph->role == GLYPH_REVIEW_MARKER
            && !g_Config.debug.enable_review_markers) {
            continue;
        } else {
            // Gather next word glyphs
            size_t word_len = 0;
            for (size_t j = i; j < glyph_count; j++) {
                if (glyphs[i + word_len]->role == GLYPH_SPACE
                    || glyphs[i + word_len]->role == GLYPH_NEW_LINE
                    || glyphs[i + word_len]->role == GLYPH_NEW_PAGE) {
                    break;
                }
                word_len++;
            }

            // Compute width (sum widths + spacing)
            float word_width = 0.0f;
            for (size_t j = i; j < i + word_len; j++) {
                word_width += M_LETTER_SPACING;
                word_width += glyphs[j]->width[current_font];
            }
            if (word_width > 0) {
                word_width -= M_LETTER_SPACING;
            }
            word_width *= scale_f;

            // Wrap line if needed
            if (cur_width + word_width > max_width) {
                if (cur_width > 0.0f) {
                    M_EmitNewline(
                        dst, &out_len, bullet_indent, space_width, &cur_width);
                }

                // Break word if longer than line
                if (word_width > max_width) {
                    for (size_t j = i; j < i + word_len; j++) {
                        const M_GLYPH_INFO *const next_glyph = glyphs[j];
                        const float glyph_width =
                            (next_glyph->width[current_font] + M_LETTER_SPACING)
                            * scale_f;
                        if (cur_width + glyph_width > max_width) {
                            M_EmitNewline(
                                dst, &out_len, bullet_indent, space_width,
                                &cur_width);
                        }
                        L_CONCAT_STR(next_glyph->text)
                        cur_width += glyph_width;
                    }
                } else {
                    for (size_t j = i; j < i + word_len; j++) {
                        const M_GLYPH_INFO *const next_glyph = glyphs[j];
                        L_CONCAT_STR(next_glyph->text)
                    }
                    cur_width = word_width;
                }
            } else {
                // Copy word as is
                for (size_t j = i; j < i + word_len; j++) {
                    const M_GLYPH_INFO *const next_glyph = glyphs[j];
                    L_CONCAT_STR(next_glyph->text)
                }
                cur_width += word_width;
            }

            // Skip forward the characters, respecting the default loop
            // accumulator
            i += word_len - 1;
        }
    }

    L_CONCAT_CHAR('\0')

#undef L_CONCAT_CHAR
#undef L_CONCAT_STR
    return out_len;
}

static void M_Process(
    const char *const text, float *const out_w, float *const out_h,
    const UI_TEXT_SETTINGS settings, const float base_x, const float base_y,
    float (*const scale_func)(float),
    void (*const draw_func)(
        int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, const RGBA_F[4]))
{
    if (text == nullptr) {
        return;
    }

    const M_GLYPH_INFO **glyphs = M_DecomposeWithCache(text, nullptr);
    ASSERT(glyphs != nullptr);

    const float scale = scale_func(UI_TEXT_BASE_SCALE * settings.scale);

    float x = scale_func(base_x / g_Config.ui.text_scale);
    float y = scale_func(
        base_y / g_Config.ui.text_scale + settings.scale * UI_TEXT_HEIGHT);
    int32_t z = settings.z;

    float max_width = 0.0f;
    const float start_x = x;

    M_FONT current_font = M_FONT_DEFAULT;
    int32_t color_idx = 0;
    int32_t prev_color_idx = color_idx;
    bool visible = true;

    const M_GLYPH_INFO **glyph_ptr = glyphs;
    while (*glyph_ptr != nullptr) {
        const M_GLYPH_INFO *const glyph = M_GetResolvedGlyph(*glyph_ptr);
        if (glyph == nullptr) {
            goto loop_end;
        }

        if (glyph->role == GLYPH_REVIEW_MARKER
            && !g_Config.debug.enable_review_markers) {
            goto loop_end;
        }

        if (glyph->role == GLYPH_VISIBILITY_MARKER) {
            visible = glyph->mesh_idx;
            goto loop_end;
        }

        if (glyph->role == GLYPH_FONT_MARKER) {
            current_font = glyph->mesh_idx;
            goto loop_end;
        }

        if (glyph->role == GLYPH_DIM_MARKER) {
            if (glyph->mesh_idx != 0) {
                prev_color_idx = color_idx;
                color_idx = M_DIM_COLOR;
            } else {
                color_idx = prev_color_idx;
            }
            goto loop_end;
        }

        if (glyph->role == GLYPH_COLOR_MARKER) {
            if (glyph->mesh_idx != -1) {
                prev_color_idx = color_idx;
                color_idx = glyph->mesh_idx;
            } else {
                color_idx = prev_color_idx;
            }
            goto loop_end;
        }

        if (glyph->role == GLYPH_NEW_LINE || glyph->role == GLYPH_NEW_PAGE) {
            y += UI_TEXT_HEIGHT * scale / UI_TEXT_BASE_SCALE;
            x = start_x;
            goto loop_end;
        }

        if (glyph->role == GLYPH_SPACE) {
            if (glyph_ptr[1] == nullptr
                || (glyph_ptr[1]->role != GLYPH_NEW_LINE
                    && glyph_ptr[1]->role != GLYPH_NEW_PAGE)) {
                x += M_WORD_SPACING * scale / UI_TEXT_BASE_SCALE;
            }
            goto loop_end;
        }

        if (glyph->role == GLYPH_SECRET) {
            const int16_t sprite_idx =
                Object_Get(O_SECRET_1 + glyph->mesh_idx)->mesh_idx;
            const SPRITE_TEXTURE *const sprite =
                Output_GetSpriteTexture(sprite_idx);
            const float input_scale_h =
                settings.scale / (sprite->x1 - sprite->x0);
            const float input_scale_v =
                settings.scale / (sprite->y1 - sprite->y0);
            const float input_scale = MIN(input_scale_h, input_scale_v);
            const float output_scale = scale_func(
                UI_TEXT_BASE_SCALE * glyph->width[current_font] * input_scale);
            if (visible && draw_func != nullptr) {
                draw_func(
                    x + scale_func(10), y, z, output_scale, output_scale,
                    sprite_idx, m_TextColor[color_idx]);
            }
            x += glyph->width[current_font] * scale / UI_TEXT_BASE_SCALE;
            goto loop_end;
        }

        M_FONT glyph_font = current_font;
        if (glyph_font == M_FONT_SMALL && !M_HasGlyph(glyph_font, glyph)) {
            glyph_font = M_FONT_DEFAULT;
        }

        float spacing = glyph->width[glyph_font];
        if (glyph_ptr[1] != nullptr && glyph_ptr[1]->role != GLYPH_NEW_LINE
            && glyph_ptr[1]->role != GLYPH_NEW_PAGE) {
            spacing += M_LETTER_SPACING;
        }

        if (glyph->role == GLYPH_TEXT && glyph->mesh_idx < 0) {
            // Non-breaking space or other non-rendered text glyphs.
            x += spacing * scale / UI_TEXT_BASE_SCALE;
            goto loop_end;
        }

        if (visible && draw_func != nullptr) {
            const OBJECT *object = Object_Get(m_FontObjects[glyph_font]);
            draw_func(
                x, y, z, scale, scale, object->mesh_idx + glyph->mesh_idx,
                m_TextColor[color_idx]);
        }

        x += spacing * scale / UI_TEXT_BASE_SCALE;

    loop_end:
        max_width = MAX(max_width, x);
        glyph_ptr++;
    }

    if (out_w != nullptr) {
        *out_w = max_width;
    }

    if (out_h != nullptr) {
        *out_h = y;
    }
}

void UI_InitText(void)
{
    // Convert the linear array coming from the .def macros to a hash lookup
    // table for faster text-to-glyph resolution.
    for (M_GLYPH_INFO *glyph_ptr = m_Glyphs; glyph_ptr->text != nullptr;
         glyph_ptr++) {
        // mark static glyphs as non-input
        M_GLYPH_MAP_ENTRY *const hash_entry = Memory_Alloc(sizeof(*hash_entry));
        hash_entry->glyph = glyph_ptr;
        HASH_ADD_KEYPTR(
            hh, m_GlyphMap, glyph_ptr->text, strlen(glyph_ptr->text),
            hash_entry);
    }

    // Create dynamic glyphs for "{key <role>}" tokens; resolution happens when
    // drawing/wrapping
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        const char *role_str =
            EnumMap_ToString(ENUM_MAP_NAME(INPUT_ROLE), role);
        if (role_str == nullptr || *role_str == '\0') {
            continue;
        }
        M_GLYPH_INFO *input_glyph = Memory_Alloc(sizeof(*input_glyph));
        input_glyph->text = String_Format("\\{input %s}", role_str);
        input_glyph->role = GLYPH_INPUT;
        input_glyph->input_role = role;
        for (M_FONT font = 0; font < M_FONT_COUNT; font++) {
            input_glyph->width[font] = 0;
        }
        M_GLYPH_MAP_ENTRY *entry = Memory_Alloc(sizeof(*entry));
        entry->glyph = input_glyph;
        HASH_ADD_KEYPTR(
            hh, m_GlyphMap, input_glyph->text, strlen(input_glyph->text),
            entry);
    }
}

void UI_LoadText(void)
{
    for (int32_t i = 0; i < M_MAX_COLOR; i++) {
        m_TextColor[i][0] = M_ToRGBA_F(m_ColorLight[i]);
        m_TextColor[i][1] = M_ToRGBA_F(m_ColorLight[i]);
        if (g_TRVersion == 3) {
            m_TextColor[i][2] = M_ToRGBA_F(m_ColorDark[i]);
            m_TextColor[i][3] = M_ToRGBA_F(m_ColorDark[i]);
        } else {
            m_TextColor[i][2] = M_ToRGBA_F(m_ColorLight[i]);
            m_TextColor[i][3] = M_ToRGBA_F(m_ColorLight[i]);
        }
    }

    for (M_FONT font = 0; font < M_FONT_COUNT; font++) {
        for (M_GLYPH_INFO *glyph_ptr = m_Glyphs; glyph_ptr->text != nullptr;
             glyph_ptr++) {
            glyph_ptr->width[font] = M_GetGlyphWidth(font, glyph_ptr);
        }
    }
}

void UI_ShutdownText(void)
{
    {
        M_GLYPH_MAP_ENTRY *current, *tmp;
        HASH_ITER(hh, m_GlyphMap, current, tmp)
        {
            if (current->glyph->role == GLYPH_INPUT) {
                Memory_FreePointer(&current->glyph->text);
                Memory_FreePointer(&current->glyph);
            }
            HASH_DEL(m_GlyphMap, current);
            Memory_Free(current);
        }
    }

    {
        M_TEXT_MAP_ENTRY *current, *tmp;
        HASH_ITER(hh, m_TextMap, current, tmp)
        {
            Memory_FreePointer(&current->text);
            Memory_FreePointer(&current->glyphs);
            HASH_DEL(m_TextMap, current);
            Memory_FreePointer(&current);
        }
    }
}

void UI_Text_Measure(
    const char *const text, float *const out_w, float *const out_h,
    const UI_TEXT_SETTINGS settings)
{
    M_Process(
        text, out_w, out_h, settings, 0.0f, 0.0f, M_ScaleNeutral, nullptr);
}

void UI_Text_Draw(
    const char *const text, const float base_x, const float base_y,
    const UI_TEXT_SETTINGS settings)
{
    M_Process(
        text, nullptr, nullptr, settings, base_x,
        base_y - g_Config.ui.text_scale, M_ScaleScreen,
        UI_ScheduleDrawScreenSprite);
}

char *UI_Text_WordWrap(
    const char *text, const float scale, const float max_width)
{
    if (text == nullptr || max_width <= 0) {
        return nullptr;
    }

    size_t glyph_count = 0;
    const M_GLYPH_INFO **glyphs = M_DecomposeWithCache(text, &glyph_count);

    const float scale_f = scale * g_Config.ui.text_scale;
    size_t len = M_WordWrap(glyphs, glyph_count, scale_f, max_width, nullptr);
    char *const wrapped_text = Memory_Alloc(len);
    M_WordWrap(glyphs, glyph_count, scale_f, max_width, wrapped_text);
    return wrapped_text;
}

char *UI_Text_FilterGlyphs(const char *const text)
{
    if (text == nullptr) {
        return nullptr;
    }
    const size_t in_len = strlen(text);
    char *out = Memory_Alloc(in_len + 1);
    size_t out_len = 0;
    const char *p = text;
    while (*p != '\0') {
        const size_t sz = String_GetCharByteSize(p);
        const char *const key_buf = String_FormatStatic("%.*s", (int32_t)sz, p);
        M_GLYPH_MAP_ENTRY *entry = nullptr;
        HASH_FIND_STR(m_GlyphMap, key_buf, entry);
        if (entry != nullptr) {
            memcpy(out + out_len, p, sz);
            out_len += sz;
        }
        p += sz;
    }
    out[out_len] = '\0';
    return out;
}
