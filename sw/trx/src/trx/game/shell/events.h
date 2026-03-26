#pragma once

#include <SDL2/SDL_events.h>

bool Shell_ProcessEvent(const SDL_Event *event);
void Shell_ProcessEvents(void);
