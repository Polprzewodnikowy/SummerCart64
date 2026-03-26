#pragma once

#include <trx/game/output/shaders/generic.h>

typedef OUTPUT_SHADER OUTPUT_UI_SHADER;

OUTPUT_UI_SHADER *Output_UIShader_Create(void);
void Output_UIShader_Free(OUTPUT_UI_SHADER *shader);
void Output_UIShader_Bind(const OUTPUT_UI_SHADER *shader);
