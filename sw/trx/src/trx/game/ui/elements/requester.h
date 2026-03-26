#pragma once

// A window to select a single option from a list of predefined choices.

#include <trx/game/ui/common.h>
#include <trx/game/ui/scrollable.h>

#include <stdint.h>

#define UI_REQUESTER_CANCEL -2
#define UI_REQUESTER_NO_CHOICE -1

typedef struct {
    bool is_selectable;
    UI_SCROLLABLE scroll;
    float row_pad;
    float row_spacing;
    bool show_arrows;
    bool reserve_space;
} UI_REQUESTER_STATE;

// state functions
void UI_Requester_Init(
    UI_REQUESTER_STATE *s, int32_t vis_rows, int32_t max_rows,
    bool is_selectable);
void UI_Requester_Free(UI_REQUESTER_STATE *s);
int32_t UI_Requester_Control(UI_REQUESTER_STATE *s);
void UI_Requester_SetMaxRows(UI_REQUESTER_STATE *s, size_t max_rows);
void UI_Requester_SetVisibleRows(UI_REQUESTER_STATE *s, size_t visible_rows);
void UI_Requester_SelectRow(UI_REQUESTER_STATE *s, int32_t i);
int32_t UI_Requester_GetFirstRow(const UI_REQUESTER_STATE *s);
int32_t UI_Requester_GetLastRow(const UI_REQUESTER_STATE *s);
int32_t UI_Requester_GetCurrentRow(const UI_REQUESTER_STATE *s);
bool UI_Requester_IsRowVisible(const UI_REQUESTER_STATE *s, int32_t i);
bool UI_Requester_IsRowSelected(const UI_REQUESTER_STATE *s, int32_t i);

// draw functions
void UI_BeginRequester(const UI_REQUESTER_STATE *s, const char *title);
void UI_EndRequester(const UI_REQUESTER_STATE *s);

void UI_BeginRequesterRow(const UI_REQUESTER_STATE *s, int32_t i);
void UI_EndRequesterRow(const UI_REQUESTER_STATE *s, int32_t i);
