// Base passport dialog functions.
// Does not implement a function on its own, and is used mostly for placement
// and sizing of the larger dialogs such as load/save game.

#pragma once

#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/requester.h>

// state functions
void UI_BasePassportDialog_Init(UI_REQUESTER_STATE *req, size_t max_rows);
void UI_BasePassportDialog_Control(UI_REQUESTER_STATE *req);

// draw functions
void UI_BeginBasePassportDialog(void);
void UI_EndBasePassportDialog(void);
