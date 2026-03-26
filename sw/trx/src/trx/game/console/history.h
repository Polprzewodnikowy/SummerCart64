#pragma once

#include <stdint.h>

int32_t Console_History_GetLength(void);
void Console_History_Clear(void);
void Console_History_Append(const char *prompt);
const char *Console_History_Get(int32_t idx);
