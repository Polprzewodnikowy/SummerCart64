#include <trx/game/viewport.h>

#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/game/const.h>
#include <trx/game/output/vars.h>
#include <trx/game/shell.h>

#define L_DEFAULT_VIEWPORT                                                     \
    { .width = SHELL_HEADLESS_WIDTH, .height = SHELL_HEADLESS_HEIGHT }
static VIEWPORT_RECT m_Rects[VIEWPORT_NUMBER_OF] = {
    [VIEWPORT_WINDOW] = L_DEFAULT_VIEWPORT,
    [VIEWPORT_TARGET] = L_DEFAULT_VIEWPORT,
    [VIEWPORT_GAME] = L_DEFAULT_VIEWPORT,
    [VIEWPORT_UI] = L_DEFAULT_VIEWPORT,
};
#undef L_DEFAULT_VIEWPORT

static int16_t m_CurrentFOV = 65;
static FOV_MODE m_CurrentFOVMode = FOV_MODE_GAME;

void Viewport_Init(int32_t x, int32_t y, int32_t width, int32_t height)
{
    const VIEWPORT_RECT *const target = &m_Rects[VIEWPORT_TARGET];
    VIEWPORT_RECT *const game = &m_Rects[VIEWPORT_GAME];
    VIEWPORT_RECT *const ui = &m_Rects[VIEWPORT_UI];

    if (x < 0 || y < 0 || width < 0 || height < 0) {
        struct {
            int32_t w, h;
        } ar = { .w = 1, .h = 1 };
        switch (g_Config.rendering.aspect_mode) {
        case ASPECT_MODE_4_3:
            ar.w = 4;
            ar.h = 3;
            break;
        case ASPECT_MODE_16_9:
            ar.w = 16;
            ar.h = 9;
            break;
        case ASPECT_MODE_16_10:
            ar.w = 16;
            ar.h = 10;
            break;
        case ASPECT_MODE_ANY:
            ar.w = target->width;
            ar.h = target->height;
            break;
        }

        x = 0;
        y = 0;
        width = target->width;
        height = target->height;
        if (g_Config.rendering.aspect_mode != ASPECT_MODE_ANY) {
            width = height * ar.w / ar.h;
        }
    }

    ui->x = x;
    ui->y = y;
    ui->width = width;
    ui->height = height;

    game->x = x;
    game->y = y;
    game->width = width / g_Config.rendering.upscaling_factor;
    game->height = height / g_Config.rendering.upscaling_factor;

    g_PhdLeft = Viewport_GetMinX(VIEWPORT_GAME);
    g_PhdTop = Viewport_GetMinY(VIEWPORT_GAME);
    g_PhdRight = Viewport_GetMaxX(VIEWPORT_GAME);
    g_PhdBottom = Viewport_GetMaxY(VIEWPORT_GAME);
}

int16_t Viewport_GetSystemFOV(void)
{
    return m_CurrentFOV;
}

int16_t Viewport_GetUserFOV(void)
{
    return g_Config.visuals.fov * DEG_1;
}

int16_t Viewport_GetEffectiveFOV(void)
{
    return Viewport_GetSystemFOV() != -1 ? Viewport_GetSystemFOV()
                                         : Viewport_GetUserFOV();
}

int32_t Viewport_GetMinX(const VIEWPORT_SPACE space)
{
    return m_Rects[space].x;
}

int32_t Viewport_GetMinY(const VIEWPORT_SPACE space)
{
    return m_Rects[space].y;
}

int32_t Viewport_GetMaxX(const VIEWPORT_SPACE space)
{
    return m_Rects[space].x + m_Rects[space].width;
}

int32_t Viewport_GetMaxY(const VIEWPORT_SPACE space)
{
    return m_Rects[space].y + m_Rects[space].height;
}

int32_t Viewport_GetCenterX(const VIEWPORT_SPACE space)
{
    return (m_Rects[space].x + m_Rects[space].width) / 2;
}

int32_t Viewport_GetCenterY(const VIEWPORT_SPACE space)
{
    return (m_Rects[space].y + m_Rects[space].height) / 2;
}

int32_t Viewport_GetWidth(const VIEWPORT_SPACE space)
{
    return m_Rects[space].width;
}

int32_t Viewport_GetHeight(const VIEWPORT_SPACE space)
{
    return m_Rects[space].height;
}

VIEWPORT_RECT Viewport_GetRect(const VIEWPORT_SPACE space)
{
    return m_Rects[space];
}

void Viewport_Reset(void)
{
    const SHELL_SIZE size = Shell_GetCurrentSize();
    VIEWPORT_RECT *const window = &m_Rects[VIEWPORT_WINDOW];
    VIEWPORT_RECT *const target = &m_Rects[VIEWPORT_TARGET];

    window->x = 0;
    window->y = 0;
    window->width = size.w;
    window->height = size.h;

    int32_t border_x = window->width * g_Config.rendering.borders;
    const int32_t border_y = window->height * g_Config.rendering.borders;
    if (g_Config.rendering.aspect_mode == ASPECT_MODE_ANY) {
        border_x = border_y;
    }
    const int32_t max_w = window->width - border_x;
    const int32_t max_h = window->height - border_y;

    double aspect_ratio = 0.0;
    switch (g_Config.rendering.aspect_mode) {
    case ASPECT_MODE_4_3:
        aspect_ratio = 4.0 / 3.0;
        break;
    case ASPECT_MODE_16_9:
        aspect_ratio = 16.0 / 9.0;
        break;
    case ASPECT_MODE_16_10:
        aspect_ratio = 16.0 / 10.0;
        break;
    case ASPECT_MODE_ANY:
    default:
        aspect_ratio = (double)max_w / (double)max_h; // just match window
        break;
    }

    // Fit the aspect ratio rectangle within max_w x max_h
    target->width = max_w;
    target->height = max_w / aspect_ratio;
    if (target->height > max_h) {
        // too tall, clamp
        target->height = max_h;
        target->width = max_h * aspect_ratio;
    }
    target->x = (window->width - target->width) / 2;
    target->y = (window->height - target->height) / 2;
    Viewport_Init(-1, -1, -1, -1);
    Viewport_Debug();
}

FOV_MODE Viewport_GetFOVMode(void)
{
    return m_CurrentFOVMode;
}

void Viewport_AlterFOV(const int16_t fov, const FOV_MODE fov_mode)
{
    m_CurrentFOV = fov;
    m_CurrentFOVMode = fov_mode;
}

void Viewport_Debug(void)
{
    const VIEWPORT_RECT *r;
    r = &m_Rects[VIEWPORT_WINDOW];
    LOG_TRACE("Window viewport: %dx%d+%d,%d", r->width, r->height, r->x, r->y);
    r = &m_Rects[VIEWPORT_TARGET];
    LOG_TRACE("Target viewport: %dx%d+%d,%d", r->width, r->height, r->x, r->y);
    r = &m_Rects[VIEWPORT_GAME];
    LOG_TRACE("Game viewport: %dx%d+%d,%d", r->width, r->height, r->x, r->y);
    r = &m_Rects[VIEWPORT_UI];
    LOG_TRACE("UI viewport: %dx%d+%d,%d", r->width, r->height, r->x, r->y);
}
