#pragma once

#include <trx/game/shell/args.h>

void Shell_InitCommonModules(void);
void Shell_ShutdownCommonModules(void);

const SHELL_ARGS *Shell_CommonInit(const SHELL_ARGS *args);
