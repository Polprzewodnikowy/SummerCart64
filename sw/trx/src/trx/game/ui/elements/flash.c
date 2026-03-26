#include <trx/game/ui/elements/flash.h>

#include <trx/game/ui/elements/hide.h>
#include <trx/game/ui/helpers.h>

void UI_Flash_Init(UI_FLASH_STATE *const s, const int32_t rate)
{
    s->count = 0;
    s->rate = rate;
}

void UI_Flash_Free(UI_FLASH_STATE *const s)
{
}

void UI_Flash_Control(UI_FLASH_STATE *const s)
{
    s->count -= 2;
    if (s->count < -s->rate) {
        s->count = s->rate;
    }
}

void UI_BeginFlash(const UI_FLASH_STATE *const s)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = UI_MeasureWrapper,
            .layout = UI_LayoutWrapper,
            .draw = UI_DrawWrapper,
        },
        0);
    UI_AddChild(node);
    UI_PushCurrent(node);
    UI_BeginHide(s->count >= 0);
}

void UI_EndFlash(void)
{
    UI_EndHide();
    UI_PopCurrent();
}
