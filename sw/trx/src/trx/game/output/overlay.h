#pragma once

void Output_Overlay_DrawGame(void);
void Output_Overlay_DrawGameMono(float desaturation);
void Output_Overlay_DrawGameMonoCool(float desaturation);
void Output_Overlay_DrawGameMonoWarm(float desaturation);
void Output_Overlay_DrawPattern(bool wave);
void Output_Overlay_DrawPatternOpacity(bool wave, float opacity);
void Output_Overlay_DrawBlackRectangle(float opacity, bool post_ui);
bool Output_Overlay_LoadImage(const char *file_name);
void Output_Overlay_DrawImage(const char *file_name);
void Output_Overlay_DrawImageMono(const char *file_name, float intensity);
void Output_Overlay_CaptureSnapshot(void);
void Output_Overlay_DrawSnapshot(float opacity);
void Output_Overlay_BeginTransitionFadeOut(float duration, float start);
