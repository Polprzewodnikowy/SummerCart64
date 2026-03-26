#include <trx/game/ui/elements/label.h>

#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/ui/helpers.h>
#include <trx/game/ui/text.h>

#include <stdarg.h>
#include <string.h>

typedef struct {
    UI_LABEL_SETTINGS settings;
    char *text;
} M_DATA;

static UI_LABEL_SETTINGS m_DefaultSettings = { .scale = 1.0f };

static void M_Measure(UI_NODE *const node)
{
    M_DATA *const data = node->data;
    float w = 0.0f, h = 0.0f;
    UI_Text_Measure(data->text, &w, &h, data->settings);
    node->measure_w = w;
    node->measure_h = h;
}

static void M_Draw(const UI_NODE *const node)
{
    M_DATA *const data = node->data;
    UI_Text_Draw(data->text, node->x, node->y, data->settings);
    UI_DrawWrapper(node);
}

void UI_Label(const char *const text)
{
    UI_LabelEx(text, m_DefaultSettings);
}

void UI_LabelFmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const char *const text = String_FormatStaticV(fmt, args);
    va_end(args);
    UI_Label(text);
}

void UI_LabelEx(const char *text, const UI_LABEL_SETTINGS settings)
{
    if (text == nullptr) {
        text = "(null)"; // quality of life for UI development
    }
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutBasic,
            .draw = M_Draw,
        },
        sizeof(M_DATA) + (text != nullptr ? strlen(text) + 1 : 1));
    M_DATA *const data = node->data;
    data->settings = settings;
    data->text = (char *)node->data + sizeof(M_DATA);
    strcpy(data->text, text != nullptr ? text : "");
    UI_AddChild(node);
}

void UI_Label_Measure(
    const char *const text, float *const out_w, float *const out_h)
{
    UI_Label_MeasureEx(text, out_w, out_h, m_DefaultSettings);
}

float UI_Label_MeasureW(const char *const text)
{
    float result;
    UI_Label_Measure(text, &result, nullptr);
    return result;
}

void UI_Label_MeasureEx(
    const char *const text, float *const out_w, float *const out_h,
    const UI_LABEL_SETTINGS settings)
{
    float w = 0.0f, h = 0.0f;
    UI_Text_Measure(text, &w, &h, settings);
    if (out_w != nullptr) {
        *out_w = w;
    }
    if (out_h != nullptr) {
        *out_h = h;
    }
}
