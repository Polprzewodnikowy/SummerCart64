#pragma once

#include <trx/game/ui/common.h>
#include <trx/game/ui/elements/requester.h>

typedef enum {
    UI_STATS_DIALOG_MODE_LEVEL,
    UI_STATS_DIALOG_MODE_FINAL,
    UI_STATS_DIALOG_MODE_ASSAULT_COURSE,
} UI_STATS_DIALOG_MODE;

typedef enum {
    UI_STATS_DIALOG_STYLE_BARE,
    UI_STATS_DIALOG_STYLE_BORDERED,
} UI_STATS_DIALOG_STYLE;

typedef struct {
    UI_STATS_DIALOG_MODE mode;
    UI_STATS_DIALOG_STYLE style;
    int32_t level_num;
} UI_STATS_DIALOG_ARGS;

typedef struct UI_STATS_DIALOG_STATE UI_STATS_DIALOG_STATE;

UI_STATS_DIALOG_STATE *UI_StatsDialog_Init(UI_STATS_DIALOG_ARGS args);
void UI_StatsDialog_Free(UI_STATS_DIALOG_STATE *s);
int32_t UI_StatsDialog_Control(UI_STATS_DIALOG_STATE *s);
void UI_StatsDialog(UI_STATS_DIALOG_STATE *s);
