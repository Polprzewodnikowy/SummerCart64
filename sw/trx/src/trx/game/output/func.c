#include <trx/game/output/func.h>

#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/matrix.h>
#include <trx/game/output/const.h>
#include <trx/game/output/state.h>
#include <trx/game/output/vars.h>
#include <trx/gl/context.h>

#include <math.h>

CLIP Output_CheckBoundsClip(const BOUNDS_16 *const bounds)
{
    if (g_MatrixPtr->_23 >= Output_GetFarZ()) {
        return CLIP_NOT_VISIBLE;
    }

    const XYZ_32 vtx[8] = {
        { .x = bounds->min.x, .y = bounds->min.y, .z = bounds->min.z },
        { .x = bounds->max.x, .y = bounds->min.y, .z = bounds->min.z },
        { .x = bounds->max.x, .y = bounds->max.y, .z = bounds->min.z },
        { .x = bounds->min.x, .y = bounds->max.y, .z = bounds->min.z },
        { .x = bounds->min.x, .y = bounds->min.y, .z = bounds->max.z },
        { .x = bounds->max.x, .y = bounds->min.y, .z = bounds->max.z },
        { .x = bounds->max.x, .y = bounds->max.y, .z = bounds->max.z },
        { .x = bounds->min.x, .y = bounds->max.y, .z = bounds->max.z },
    };

    int32_t num_z = 0;
    int32_t x_min = INT32_MAX;
    int32_t y_min = INT32_MAX;
    int32_t x_max = INT32_MIN;
    int32_t y_max = INT32_MIN;

    for (int32_t i = 0; i < 8; i++) {
        // clang-format off
        const int32_t zv = (
            g_MatrixPtr->_20 * vtx[i].x +
            g_MatrixPtr->_21 * vtx[i].y +
            g_MatrixPtr->_22 * vtx[i].z +
            g_MatrixPtr->_23);
        // clang-format on

        if (zv <= Output_GetNearZ() || zv >= Output_GetFarZ()) {
            continue;
        }

        num_z++;
        const int32_t zp = zv / g_PhdPersp;
        // clang-format off
        const int32_t xv = (
            g_MatrixPtr->_00 * vtx[i].x +
            g_MatrixPtr->_01 * vtx[i].y +
            g_MatrixPtr->_02 * vtx[i].z +
            g_MatrixPtr->_03) / zp;
        const int32_t yv = (
            g_MatrixPtr->_10 * vtx[i].x +
            g_MatrixPtr->_11 * vtx[i].y +
            g_MatrixPtr->_12 * vtx[i].z +
            g_MatrixPtr->_13) / zp;
        // clang-format on

        CLAMPG(x_min, xv);
        CLAMPL(x_max, xv);
        CLAMPG(y_min, yv);
        CLAMPL(y_max, yv);
    }

    if (num_z == 0) {
        return CLIP_NOT_VISIBLE; // out of screen
    }

    const VIEWPORT_RECT vp = Viewport_GetRect(VIEWPORT_GAME);
    x_min += vp.w / 2;
    x_max += vp.w / 2;
    y_min += vp.h / 2;
    y_max += vp.h / 2;

    // clang-format off
    if (x_min > g_PhdRight
        || y_min > g_PhdBottom
        || x_max < g_PhdLeft
        || y_max < g_PhdTop) {
        return CLIP_NOT_VISIBLE; // out of screen
    }
    // clang-format on

    // clang-format off
    if (num_z < 8 ||
        x_min < 0 ||
        y_min < 0 ||
        x_max >= vp.w ||
        y_max >= vp.h) {
        return CLIP_PARTIALLY_VISIBLE;
    }
    // clang-format on

    return CLIP_FULLY_VISIBLE;
}

void Output_MakeScreenshot(const char *const path)
{
    LOG_INFO("Taking screenshot");
    TRX_GL_Context_ScheduleScreenshot(path);
}

void Output_ApplyFOV(void)
{
    int32_t fov = Viewport_GetEffectiveFOV();
    const int32_t sw = Viewport_GetWidth(VIEWPORT_GAME);
    const int32_t sh = Viewport_GetHeight(VIEWPORT_GAME);
    const float aspect = sw / (float)sh;

    int32_t fov_width;
    switch (Viewport_GetFOVMode()) {
    case FOV_MODE_VERTICAL: {
        const float fov_rad_h = fov * M_PI / (180 * DEG_1);
        const float fov_rad_v = 2 * atan(aspect * tan(fov_rad_h / 2));
        fov = round((fov_rad_v / M_PI) * (180 * DEG_1));
        fov_width = sw;
        break;
    }
    case FOV_MODE_HORIZONTAL:
        fov_width = sw;
        break;
    case FOV_MODE_PC:
        fov_width = sw * ((4.0f / 3.0f) / aspect);
        break;
    case FOV_MODE_PS1:
        fov_width = sw * ((4.0f / 3.0f) / aspect) * 240 / 200;
        break;
    default:
        ASSERT_FAIL();
    }

    const int16_t c = Math_Cos(fov / 2);
    const int16_t s = Math_Sin(fov / 2);
    g_PhdPersp = fov_width / 2;
    if (s != 0) {
        g_PhdPersp *= c;
        g_PhdPersp /= s;
    }
}
