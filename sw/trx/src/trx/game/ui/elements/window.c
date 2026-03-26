#include <trx/game/ui/elements/window.h>

#include <trx/debug.h>
#include <trx/game/ui.h>
#include <trx/version.h>

typedef struct {
    UI_WINDOW_SETTINGS settings;
    bool show_scroll_hints;
} M_CONTEXT;

static M_CONTEXT m_ContextStack[16];
static int32_t m_ContextStackSize = 0;

static bool M_ShouldShowScrollHints(const UI_WINDOW_SETTINGS *const settings)
{
    if (settings->scrollable == nullptr) {
        return false;
    }
    return settings->reserve_scroll_space
        || settings->scrollable->vis_items < settings->scrollable->max_items;
}

static float M_GetOuterPad(void)
{
    return g_TRVersion >= 2 ? 3.0f : 2.0f;
}

static float M_GetBodyPadX(void)
{
    return g_TRVersion >= 2 ? 4.0f : 8.0f;
}

static float M_GetBodyPadY(void)
{
    return 4.0f;
}

static float M_GetTitleSpacing(const UI_WINDOW_SETTINGS *const settings)
{
    if (settings->title_spacing >= 0.0f) {
        return settings->title_spacing;
    }
    return 3.0f;
}

static void M_ScrollHintRow(
    const UI_WINDOW_SETTINGS *const settings, const bool show_arrow,
    const bool up)
{
    UI_BeginResize(-1.0f, 7.0f);
    UI_BeginAnchor(0.5f, up ? 1.5f : -1.0f);
    if (show_arrow) {
        UI_LabelEx(
            up ? "\\{arrow up}" : "\\{arrow down}",
            (UI_LABEL_SETTINGS) { .scale = 0.7f });
    }
    UI_EndAnchor();
    UI_EndResize();
}

static void M_Title(const char *const title)
{
    UI_BeginFrame(UI_FRAME_DIALOG_HEADING);
    UI_BeginPad(10.0f, g_TRVersion >= 2 ? 1.0f : 2.0f);
    UI_BeginAnchor(0.5f, 0.5f);
    UI_Label(title);
    UI_EndAnchor();
    UI_EndPad();
    UI_EndFrame();
}

void UI_BeginWindow(UI_WINDOW_SETTINGS settings)
{
    const bool show_scroll_hints = M_ShouldShowScrollHints(&settings);
    ASSERT(
        m_ContextStackSize
        < (int32_t)(sizeof(m_ContextStack) / sizeof(m_ContextStack[0])));
    m_ContextStack[m_ContextStackSize++] = (M_CONTEXT) {
        .settings = settings,
        .show_scroll_hints = show_scroll_hints,
    };

    UI_BeginFrame(
        settings.heavy ? UI_FRAME_DIALOG_BACKGROUND_HEAVY
                       : UI_FRAME_DIALOG_BACKGROUND);
    UI_BeginPad(M_GetOuterPad(), M_GetOuterPad());
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
        .spacing = { .v = M_GetTitleSpacing(&settings) },
    });

    if (settings.title != nullptr) {
        M_Title(settings.title);
    }

    UI_BeginPadEx(
        M_GetBodyPadX(), M_GetBodyPadX(), M_GetBodyPadY(), M_GetBodyPadY());
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
    });

    if (settings.header_func != nullptr) {
        settings.header_func(settings.user_data);
    }

    if (show_scroll_hints) {
        M_ScrollHintRow(&settings, settings.scrollable->first_item != 0, true);
    }
}

void UI_EndWindow(void)
{
    ASSERT(m_ContextStackSize > 0);
    m_ContextStackSize--;
    const M_CONTEXT *const ctx = &m_ContextStack[m_ContextStackSize];
    const UI_WINDOW_SETTINGS *const settings = &ctx->settings;

    if (ctx->show_scroll_hints) {
        M_ScrollHintRow(
            settings,
            settings->scrollable->first_item + settings->scrollable->vis_items
                < settings->scrollable->max_items,
            false);
    }

    if (settings->footer_func != nullptr) {
        settings->footer_func(settings->user_data);
    }

    UI_EndStack();
    UI_EndPad();
    UI_EndStack();
    UI_EndPad();
    UI_EndFrame();
}
