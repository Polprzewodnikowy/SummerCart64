#include <trx/game/ui/elements/prompt.h>

#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/game/const.h>
#include <trx/game/input.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/flash.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/events.h>
#include <trx/game/ui/helpers.h>
#include <trx/game/ui/text.h>

#include <string.h>

typedef struct {
    UI_PROMPT_STATE *state;
} M_DATA;

static void M_Layout(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    UI_LayoutBasic(node, x, y, w, h);
    const M_DATA *const data = node->data;
    const UI_PROMPT_STATE *const s = data->state;
    UI_NODE *const prompt = node->first_child;
    UI_NODE *const caret = prompt->next_sibling;
    prompt->ops.layout(prompt, x, y, w, h);

    const char old = s->current_text[s->caret_pos];
    s->current_text[s->caret_pos] = '\0';
    float caret_pos;
    UI_Label_Measure(s->current_text, &caret_pos, nullptr);
    s->current_text[s->caret_pos] = old;

    caret->ops.layout(caret, x + caret_pos, y, w, h);
}

static int32_t M_GetPrevCaretPos(
    const char *const text, const int32_t caret_pos)
{
    if (caret_pos <= 0) {
        return 0;
    }

    const char *const caret_ptr = text + caret_pos;
    const char *p = text;
    const char *prev = text;

    while (p < caret_ptr) {
        prev = p;
        p += String_GetCharByteSize(p);
    }

    return (int32_t)(prev - text);
}

static int32_t M_GetNextCaretPos(
    const char *const text, const int32_t caret_pos)
{
    const size_t text_len = strlen(text);
    if ((size_t)caret_pos >= text_len) {
        return (int32_t)text_len;
    }

    int32_t next_pos =
        caret_pos + (int32_t)String_GetCharByteSize(text + caret_pos);
    if ((size_t)next_pos > text_len) {
        next_pos = (int32_t)text_len;
    }
    return next_pos;
}

static void M_MoveCaretLeft(UI_PROMPT_STATE *const s)
{
    s->caret_pos = M_GetPrevCaretPos(s->current_text, s->caret_pos);
}

static void M_MoveCaretRight(UI_PROMPT_STATE *const s)
{
    s->caret_pos = M_GetNextCaretPos(s->current_text, s->caret_pos);
}

static void M_MoveCaretStart(UI_PROMPT_STATE *const s)
{
    s->caret_pos = 0;
}

static void M_MoveCaretEnd(UI_PROMPT_STATE *const s)
{
    s->caret_pos = strlen(s->current_text);
}

static void M_DeleteCharBack(UI_PROMPT_STATE *const s)
{
    if (s->caret_pos <= 0) {
        return;
    }

    const int32_t delete_start =
        M_GetPrevCaretPos(s->current_text, s->caret_pos);
    if (delete_start >= s->caret_pos || delete_start < 0) {
        return;
    }

    memmove(
        s->current_text + delete_start, s->current_text + s->caret_pos,
        strlen(s->current_text) + 1 - (size_t)s->caret_pos);
    s->caret_pos = delete_start;
}

static void M_Clear(UI_PROMPT_STATE *const s)
{
    strcpy(s->current_text, "");
    s->caret_pos = 0;
}

static void M_Cancel(UI_PROMPT_STATE *const s)
{
    UI_FireEvent((EVENT) {
        .name = "cancel",
        .sender = s,
        .data = s->current_text,
    });
    M_Clear(s);
}

static void M_Confirm(UI_PROMPT_STATE *const s)
{
    if (String_IsEmpty(s->current_text)) {
        M_Cancel(s);
        return;
    }
    UI_FireEvent((EVENT) {
        .name = "confirm",
        .sender = s,
        .data = s->current_text,
    });
    M_Clear(s);
}

static void M_HandleKeyDown(const EVENT *const event, void *const user_data)
{
    const UI_INPUT key = (UI_INPUT)(uintptr_t)event->data;
    UI_PROMPT_STATE *const s = user_data;

    if (!s->is_focused) {
        return;
    }

    // clang-format off
    switch (key) {
    case UI_KEY_LEFT:   M_MoveCaretLeft(s); break;
    case UI_KEY_RIGHT:  M_MoveCaretRight(s); break;
    case UI_KEY_HOME:   M_MoveCaretStart(s); break;
    case UI_KEY_END:    M_MoveCaretEnd(s); break;
    case UI_KEY_BACK:   M_DeleteCharBack(s); break;
    case UI_KEY_RETURN: M_Confirm(s); break;
    case UI_KEY_ESCAPE: M_Cancel(s); break;
    default:            break;
    }
    // clang-format on
}

static void M_HandleTextEdit(const EVENT *const event, void *const user_data)
{
    UI_PROMPT_STATE *const s = user_data;
    if (!s->is_focused) {
        return;
    }

    char *filtered = UI_Text_FilterGlyphs(event->data);
    if (filtered == nullptr || filtered[0] == '\0') {
        Memory_FreePointer(&filtered);
        return;
    }
    const char *insert_string = filtered;
    const size_t insert_length = strlen(insert_string);

    const size_t available_space =
        s->current_text_capacity - strlen(s->current_text);
    if (insert_length >= available_space) {
        s->current_text_capacity *= 2;
        s->current_text =
            Memory_Realloc(s->current_text, s->current_text_capacity);
    }

    memmove(
        s->current_text + s->caret_pos + insert_length,
        s->current_text + s->caret_pos,
        strlen(s->current_text) + 1 - s->caret_pos);
    memcpy(s->current_text + s->caret_pos, insert_string, insert_length);

    s->caret_pos += insert_length;
    Memory_FreePointer(&filtered);
}

void UI_Prompt_Init(UI_PROMPT_STATE *const s)
{
    s->is_focused = false;
    s->current_text_capacity = 30;
    s->current_text = Memory_Alloc(s->current_text_capacity);
    s->listener1 = UI_Subscribe("key_down", nullptr, M_HandleKeyDown, s);
    s->listener2 = UI_Subscribe("text_edit", nullptr, M_HandleTextEdit, s);
    UI_Flash_Init(&s->flash, LOGIC_FPS * 2 / 3);
}

void UI_Prompt_Free(UI_PROMPT_STATE *const s)
{
    UI_Unsubscribe(s->listener1);
    UI_Unsubscribe(s->listener2);
    UI_Flash_Free(&s->flash);
    Memory_FreePointer(&s->current_text);
}

void UI_Prompt_Control(UI_PROMPT_STATE *const s)
{
    UI_Flash_Control(&s->flash);
}

void UI_Prompt(UI_PROMPT_STATE *const s)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = UI_MeasureWrapper,
            .layout = M_Layout,
            .draw = UI_DrawWrapper,
        },
        sizeof(M_DATA));
    M_DATA *const data = node->data;
    data->state = s;
    UI_AddChild(node);
    UI_PushCurrent(node);
    UI_LabelEx(
        s->current_text != nullptr ? s->current_text : "",
        (UI_LABEL_SETTINGS) { .scale = 1.0f, .z = 16 });
    if (s->is_focused) {
        UI_BeginFlash(&s->flash);
    }
    UI_LabelEx(
        "\\{button left}", (UI_LABEL_SETTINGS) { .scale = 1.0f, .z = 8 });
    if (s->is_focused) {
        UI_EndFlash();
    }
    UI_PopCurrent();
}

void UI_Prompt_SetFocus(UI_PROMPT_STATE *const s, const bool is_focused)
{
    if (s->is_focused == is_focused) {
        return;
    }
    s->is_focused = is_focused;
    s->flash.count = 0;
    if (is_focused) {
        Input_EnterListenMode();
    } else {
        Input_ExitListenMode();
    }
}

void UI_Prompt_Clear(UI_PROMPT_STATE *const s)
{
    M_Clear(s);
}

void UI_Prompt_ChangeText(UI_PROMPT_STATE *const s, const char *const new_text)
{
    Memory_FreePointer(&s->current_text);
    s->current_text = Memory_DupStr(new_text);
    s->current_text_capacity = strlen(new_text) + 1;
    s->caret_pos = strlen(new_text);
}
