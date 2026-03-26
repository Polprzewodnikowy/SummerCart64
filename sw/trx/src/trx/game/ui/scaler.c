#include <trx/game/ui/scaler.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/viewport.h>

static float M_DoCalc(
    const float unit, const float base_width, const float base_height,
    const double factor)
{
    const float vp_width = Viewport_GetWidth(VIEWPORT_UI);
    const float vp_height = Viewport_GetHeight(VIEWPORT_UI);
    const float sign = unit < 0 ? -1 : 1;
    const float sx =
        ((double)vp_width * ABS(unit) * factor) / MAX(1, base_width);
    const float sy =
        ((double)vp_height * ABS(unit) * factor) / MAX(1, base_height);
    return MIN(sx, sy) * sign;
}

double UI_Scaler_GetScale(const UI_SCALER_TARGET target)
{
    switch (target) {
    case UI_SCALER_TARGET_BAR:
        return g_Config.ui.bar_scale;
    case UI_SCALER_TARGET_TEXT:
        return g_Config.ui.text_scale;
    case UI_SCALER_TARGET_ASSAULT_DIGITS:
        return g_Config.ui.text_scale;
    default:
        return 1.0;
    }
}

float UI_Scaler_Calc(const float unit, const UI_SCALER_TARGET target)
{
    return M_DoCalc(unit, 640, 480, UI_Scaler_GetScale(target));
}

float UI_Scaler_CalcInverse(const float unit, const UI_SCALER_TARGET target)
{
    return unit * 0x10000 / MAX(1, UI_Scaler_Calc(0x10000, target));
}
