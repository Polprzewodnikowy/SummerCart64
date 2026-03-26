#include <trx/game/ui/elements/frame.h>

#include <trx/config.h>
#include <trx/game/output.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/helpers.h>

typedef struct {
    UI_STYLE ui_style;
    TEXT_STYLE text_style;
    int32_t outline_z;
    int32_t background_z;
} M_DATA;

static void M_Draw(const UI_NODE *node)
{
    const M_DATA *const data = node->data;
    if (data->background_z >= 0) {
        UI_ScheduleDrawTextBackground(
            data->ui_style, UI_ScaleX(node->x), UI_ScaleY(node->y),
            data->background_z, UI_ScaleX(node->w), UI_ScaleY(node->h),
            data->text_style);
    }
    if (data->outline_z >= 0) {
        UI_ScheduleDrawTextOutline(
            data->ui_style, UI_ScaleX(node->x), UI_ScaleY(node->y),
            data->outline_z, UI_ScaleX(node->w), UI_ScaleY(node->h),
            data->text_style);
    }
    UI_DrawWrapper(node);
}

void UI_BeginFrame(UI_FRAME_STYLE style)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = UI_MeasureWrapper,
            .layout = UI_LayoutWrapper,
            .draw = M_Draw,
        },
        sizeof(M_DATA));
    M_DATA *const data = node->data;

    data->ui_style = g_Config.ui.menu_style;

    switch (style) {
    case UI_FRAME_DIALOG_BACKGROUND:
        data->outline_z = 160;
        data->background_z = 160;
        data->text_style = TS_BACKGROUND;
        break;
    case UI_FRAME_DIALOG_BACKGROUND_HEAVY:
        data->outline_z = 160;
        data->background_z = 160;
        data->text_style = TS_BACKGROUND_HEAVY;
        break;
    case UI_FRAME_DIALOG_HEADING:
        data->outline_z = 80;
        data->background_z = 80;
        data->text_style = TS_HEADING;
        break;
    case UI_FRAME_SELECTED_OPTION:
        data->outline_z = 80;
        data->background_z = 80;
        data->text_style = TS_REQUESTED;
        break;
    case UI_FRAME_OUTLINE_ONLY:
        data->outline_z = 80;
        data->background_z = -1;
        data->text_style = TS_REQUESTED;
        break;
    }

    UI_AddChild(node);
    UI_PushCurrent(node);
}

void UI_EndFrame(void)
{
    UI_PopCurrent();
}
