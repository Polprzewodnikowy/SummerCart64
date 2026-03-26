#include <trx/game/ui/elements/offset.h>

#include <trx/game/ui/elements/pad.h>

void UI_BeginOffset(const float x, const float y)
{
    UI_BeginPadEx(x, -x, y, -y);
}

void UI_EndOffset(void)
{
    UI_EndPad();
}
