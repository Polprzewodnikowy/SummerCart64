#pragma once

#include <trx/config/types.h>

void Shell_SyncToWindow(void);
void Shell_SyncFromWindow(bool update_viewport);

void Shell_RefreshRendererViewport(void);
void Shell_HandleConfigChange(const CONFIG *old, const CONFIG *new);
