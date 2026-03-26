#pragma once

#include <trx/game/ui/common.h>

#include <stdint.h>

// Make the child widget invisible in the specified interval.

typedef struct {
    int32_t rate;
    int32_t count;
} UI_FLASH_STATE;

// state functions
void UI_Flash_Init(UI_FLASH_STATE *s, int32_t rate);
void UI_Flash_Free(UI_FLASH_STATE *s);
void UI_Flash_Control(UI_FLASH_STATE *s);

// draw functions
void UI_BeginFlash(const UI_FLASH_STATE *s);
void UI_EndFlash(void);
