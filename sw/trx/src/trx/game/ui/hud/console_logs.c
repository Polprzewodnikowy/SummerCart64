#include <trx/game/ui/hud/console_logs.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/clock.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/stack.h>
#include <trx/game/ui/events.h>

#include <string.h>

#define M_LOG_SCALE 0.8f
#define M_MAX_LOG_LINES 20
#define M_DELAY_PER_CHAR 0.2

static void M_ScrollLogs(UI_CONSOLE_LOGS *s);
static void M_UpdateLogCount(UI_CONSOLE_LOGS *s);
static void M_HandleLog(const EVENT *event, void *user_data);
static void M_HandleClear(const EVENT *event, void *user_data);

static void M_ScrollLogs(UI_CONSOLE_LOGS *const s)
{
    int32_t i = s->max_lines - 1;
    while (i >= 0 && s->logs[i].text == nullptr) {
        i--;
    }

    bool need_layout = false;
    while (i >= 0 && s->logs[i].text != nullptr
           && Clock_GetRealTime() >= s->logs[i].expire_at) {
        s->logs[i].expire_at = 0.0;
        Memory_FreePointer(&s->logs[i].text);
        need_layout = true;
        i--;
    }

    if (need_layout) {
        M_UpdateLogCount(s);
    }
}

static void M_UpdateLogCount(UI_CONSOLE_LOGS *const s)
{
    s->vis_lines = 0;
    for (int32_t i = s->max_lines - 1; i >= 0; i--) {
        if (s->logs[i].expire_at != 0.0) {
            s->vis_lines = i + 1;
            break;
        }
    }
}

static void M_HandleLog(const EVENT *const event, void *const user_data)
{
    const char *text = event->data;
    UI_CONSOLE_LOGS *const s = user_data;
    Memory_FreePointer(&s->logs[s->max_lines - 1].text);
    for (int32_t i = s->max_lines - 1; i > 0; i--) {
        s->logs[i] = s->logs[i - 1];
    }

    s->logs[0].expire_at =
        Clock_GetRealTime() + strlen(text) * M_DELAY_PER_CHAR;
    s->logs[0].text = UI_Text_WordWrap(text, M_LOG_SCALE, UI_GetCanvasWidth());
    M_UpdateLogCount(s);
}

static void M_HandleClear(const EVENT *const event, void *const user_data)
{
    UI_CONSOLE_LOGS *const s = user_data;
    for (size_t i = 0; i < s->max_lines; i++) {
        s->logs[i].expire_at = 0.0;
    }
    M_ScrollLogs(s);
}

void UI_ConsoleLogs_Init(UI_CONSOLE_LOGS *const s)
{
    if (s->max_lines <= 0) {
        s->max_lines = M_MAX_LOG_LINES;
    }
    s->logs = Memory_Alloc(s->max_lines * sizeof(UI_CONSOLE_LOG_LINE));
    s->vis_lines = 0;
    s->listeners[0] = UI_Subscribe("console_log", nullptr, M_HandleLog, s);
    s->listeners[1] = UI_Subscribe("console_clear", nullptr, M_HandleClear, s);
}

void UI_ConsoleLogs_Free(UI_CONSOLE_LOGS *const s)
{
    if (s->logs != nullptr) {
        for (int32_t i = 0; i < M_MAX_LOG_LINES; i++) {
            Memory_FreePointer(&s->logs[i].text);
        }
        Memory_FreePointer(&s->logs);
    }
    UI_Unsubscribe(s->listeners[0]);
    UI_Unsubscribe(s->listeners[1]);
}

void UI_ConsoleLogs(UI_CONSOLE_LOGS *const s)
{
    ASSERT(s != nullptr);
    M_ScrollLogs(s);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = {
            .h = UI_STACK_H_ALIGN_LEFT,
            .v = UI_STACK_V_ALIGN_CENTER,
        },
    });
    for (int32_t i = s->vis_lines - 1; i >= 0; i--) {
        UI_LabelEx(
            s->logs[i].text, (UI_LABEL_SETTINGS) { .scale = M_LOG_SCALE });
    }
    UI_EndStack();
}
