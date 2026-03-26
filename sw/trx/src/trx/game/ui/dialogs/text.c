#include <trx/game/ui/dialogs/text.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/sound.h>
#include <trx/game/ui/elements/anchor.h>
#include <trx/game/ui/elements/frame.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/modal.h>
#include <trx/game/ui/elements/pad.h>
#include <trx/game/ui/elements/resize.h>
#include <trx/game/ui/elements/spacer.h>
#include <trx/game/ui/elements/stack.h>

#include <stdio.h>
#include <string.h>

#define M_TITLE_MARGIN 5.0f
#define M_DIALOG_PADDING 8.0f

typedef struct UI_TEXT_DIALOG_STATE {
    char *title; // uppercase working title string
    char *last_raw_title; // last raw title (duplicate for strcmp)
    char *last_raw_text; // last raw text (duplicate for strcmp)

    size_t wrap_width;
    size_t wrap_max_lines;
    bool is_heavy;

    size_t max_vis_lines; // maximum visible lines in pagination
    VECTOR *page_content; // paginated wrapped pages
    int32_t current_page;
} UI_TEXT_DIALOG_STATE;

static void M_UpdateTitle(
    UI_TEXT_DIALOG_STATE *const s, const char *const title_raw)
{
    // Title update on change (strcmp to handle static ring buffers)
    if (title_raw != nullptr
        && (s->last_raw_title == nullptr
            || strcmp(s->last_raw_title, title_raw) != 0)) {
        Memory_FreePointer(&s->title);
        s->title = String_ToUpper(title_raw);
        Memory_FreePointer(&s->last_raw_title);
        s->last_raw_title = Memory_DupStr(title_raw);
    }
}

static void M_UpdateText(
    UI_TEXT_DIALOG_STATE *const s, const char *const text_raw)
{
    // Text update on change (strcmp to in case the pointers does not change)
    if (!s->last_raw_text || strcmp(s->last_raw_text, text_raw) != 0) {
        if (s->page_content != nullptr) {
            for (int32_t i = 0; i < s->page_content->count; i++) {
                char *page = *(char **)Vector_Get(s->page_content, i);
                Memory_Free(page);
            }
            Vector_Free(s->page_content);
        }

        const char *wrapped = UI_Text_WordWrap(text_raw, 1.0f, s->wrap_width);
        s->page_content = String_Paginate(wrapped, s->wrap_max_lines);
        Memory_FreePointer(&wrapped);

        s->max_vis_lines = 0;
        for (int32_t i = 0; i < s->page_content->count; ++i) {
            size_t page_lines = 1;
            const char *c = *(char **)Vector_Get(s->page_content, i);
            while (*c != '\0') {
                page_lines += *c++ == '\n';
            }
            CLAMPL(s->max_vis_lines, page_lines);
        }
        Memory_FreePointer(&s->last_raw_text);
        s->last_raw_text = Memory_DupStr(text_raw);
        s->current_page = 0;
    }
}
static bool M_SelectPage(UI_TEXT_DIALOG_STATE *const s, const int32_t new_page)
{
    if (s->page_content == nullptr) {
        return false;
    }
    if (new_page == s->current_page || new_page < 0
        || new_page >= s->page_content->count) {
        return false;
    }
    s->current_page = new_page;
    return true;
}

UI_TEXT_DIALOG_STATE *UI_TextDialog_Init(
    size_t wrap_width, size_t wrap_max_lines, bool is_heavy)
{
    UI_TEXT_DIALOG_STATE *s = Memory_Alloc(sizeof(*s));
    s->wrap_width = wrap_width;
    s->wrap_max_lines = wrap_max_lines;
    s->is_heavy = is_heavy;
    return s;
}

void UI_TextDialog_Free(UI_TEXT_DIALOG_STATE *const s)
{
    ASSERT(s != nullptr);
    Memory_FreePointer(&s->title);
    Memory_FreePointer(&s->last_raw_title);
    Memory_FreePointer(&s->last_raw_text);
    if (s->page_content != nullptr) {
        for (int32_t i = s->page_content->count - 1; i >= 0; --i) {
            Memory_Free(*(char **)Vector_Get(s->page_content, i));
        }
        Vector_Free(s->page_content);
        s->page_content = nullptr;
    }
    Memory_Free(s);
}

void UI_TextDialog_Control(UI_TEXT_DIALOG_STATE *const s)
{
    ASSERT(s != nullptr);
    const int32_t page_shift =
        g_InputDB.menu_left ? -1 : (g_InputDB.menu_right ? 1 : 0);
    if (M_SelectPage(s, s->current_page + page_shift)) {
        Sound_Effect(SFX_MENU_PASSPORT, nullptr, SPM_ALWAYS);
    }
}

void UI_TextDialogEx(
    UI_TEXT_DIALOG_STATE *const s, const UI_TEXT_DIALOG_SETTINGS settings)
{
    ASSERT(s != nullptr);

    const char *const title_raw = settings.title_raw;
    const char *const text_raw = settings.text_raw;

    if (text_raw == nullptr || String_IsEmpty(text_raw)) {
        return;
    }

    M_UpdateTitle(s, title_raw);
    M_UpdateText(s, text_raw);

    UI_BeginModal(0.5f, 0.5f);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .spacing = { .v = 5.0f },
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
    });

    UI_BeginFrame(
        s->is_heavy ? UI_FRAME_DIALOG_BACKGROUND_HEAVY
                    : UI_FRAME_DIALOG_BACKGROUND);
    UI_BeginPad(M_DIALOG_PADDING, M_DIALOG_PADDING);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        { .h = UI_STACK_H_ALIGN_SPAN },
    });

    UI_BeginAnchor(0.5f, 0.5f);
    UI_Label(s->title != nullptr ? s->title : "");
    UI_EndAnchor();
    UI_Spacer(M_TITLE_MARGIN, M_TITLE_MARGIN);

    for (int32_t i = 0; i < s->page_content->count; ++i) {
        if (i != s->current_page) {
            UI_BeginResize(-1.0f, 0.0f);
        } else if (s->page_content->count == 1) {
            UI_BeginResize(-1.0f, -1.0f);
        } else {
            UI_BeginResize(-1.0f, UI_TEXT_HEIGHT * s->max_vis_lines);
        }
        UI_Label(*(char **)Vector_Get(s->page_content, i));
        UI_EndResize();
    }

    if (s->page_content->count > 1) {
        UI_Spacer(M_TITLE_MARGIN, M_TITLE_MARGIN * 3);

        UI_BeginAnchor(1.0f, 0.5f);
        UI_BeginStack(UI_STACK_HORIZONTAL);
        if (s->current_page > 0) {
            UI_Label("\\{button left} ");
        }
        char page_indicator[100];
        sprintf(
            page_indicator, *GS_PTR("general/misc/pagination_nav"),
            s->current_page + 1, s->page_content->count);
        UI_Label(page_indicator);
        if (s->current_page < s->page_content->count - 1) {
            UI_Label(" \\{button right}");
        }
        UI_EndStack();
        UI_EndAnchor();
    }

    UI_EndStack();
    UI_EndPad();
    UI_EndFrame();

    if (settings.footer_func != nullptr) {
        settings.footer_func(settings.footer_user_data);
    }
    UI_EndStack();

    UI_EndModal();
}

void UI_TextDialog(
    UI_TEXT_DIALOG_STATE *const s, const char *const title_raw,
    const char *const text_raw)
{
    UI_TextDialogEx(
        s,
        (UI_TEXT_DIALOG_SETTINGS) {
            .title_raw = title_raw,
            .text_raw = text_raw,
        });
}
