#include <trx/game/level/context.h>

#include <string.h>

static LEVEL_CONTEXT m_LoadContext = {};

void Level_Context_Reset(const LEVEL_FORMAT_LOADER *const loader)
{
    memset(&m_LoadContext, 0, sizeof(m_LoadContext));
    m_LoadContext.loader = loader;
}

LEVEL_CONTEXT *Level_Context_Get(void)
{
    return &m_LoadContext;
}

LEVEL_CONTEXT_INFO *Level_Context_GetInfo(void)
{
    return &m_LoadContext.info;
}
