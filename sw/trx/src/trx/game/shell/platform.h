#pragma once

// Isolated platform-sensitive initialization code
#include <SDL2/SDL_video.h>

void Shell_SetupHiDPI(void);
void Shell_SetupLibAV(void);

void Shell_EnableThemeSupport(SDL_Window *window);
