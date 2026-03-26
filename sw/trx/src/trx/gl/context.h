#pragma once

#include <trx/game/viewport.h>
#include <trx/gl/enum.h>
#include <trx/gl/renderer.h>

#include <stdint.h>

bool TRX_GL_Context_Attach(void *window_handle);
void TRX_GL_Context_Detach(void);

void TRX_GL_Context_SetDisplayFilter(TEXTURE_FILTER filter);
bool TRX_GL_Context_GetWireframeMode(void);
void TRX_GL_Context_SetWireframeMode(bool enable);
void TRX_GL_Context_SetLineWidth(int32_t line_width);
void TRX_GL_Context_SetVSync(bool vsync);

void *TRX_GL_Context_GetWindowHandle(void);

void TRX_GL_Context_Clear(void);
void TRX_GL_Context_SwapBuffers(void);
void TRX_GL_Context_SetRendered(void);

void TRX_GL_Context_SwitchToViewport(VIEWPORT_SPACE space);

void TRX_GL_Context_ScheduleScreenshot(const char *path);
const char *TRX_GL_Context_GetScheduledScreenshotPath(void);
void TRX_GL_Context_ClearScheduledScreenshotPath(void);

TRX_GL_CONFIG *TRX_GL_Context_GetConfig(void);
