#include <trx/game/game/state.h>

#include <trx/game/fmv.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>

static bool m_IsPlaying = false;
static const GF_LEVEL *m_CurrentLevel = nullptr;
static GAME_BONUS_FLAG m_BonusFlag = GBF_NONE;
static bool m_IsLevelComplete = false;

void Game_SetIsPlaying(const bool is_playing)
{
    m_IsPlaying = is_playing;
    Random_FreezeDraw(!is_playing);
}

bool Game_IsPlaying(void)
{
    return m_IsPlaying;
}

const GF_LEVEL *Game_GetCurrentLevel(void)
{
    return m_CurrentLevel;
}

void Game_SetCurrentLevel(const GF_LEVEL *const level)
{
    m_CurrentLevel = level;
}

bool Game_IsInGym(void)
{
    const GF_LEVEL *const current_level = GF_GetCurrentLevel();
    return current_level != nullptr && current_level->type == GFL_GYM;
}

bool Game_IsLoaded(void)
{
    if (FMV_IsPlaying()) {
        return false;
    }
    const GF_LEVEL *const current_level = GF_GetCurrentLevel();
    if (current_level == nullptr || current_level->type == GFL_TITLE) {
        return false;
    }
    return true;
}

bool Game_IsPlayable(void)
{
    if (FMV_IsPlaying()) {
        return false;
    }

    const GF_LEVEL *const current_level = GF_GetCurrentLevel();
    if (current_level == nullptr || current_level->type == GFL_TITLE
        || current_level->type == GFL_DEMO
        || current_level->type == GFL_CUTSCENE) {
        return false;
    }

    if (!Object_Get(O_LARA)->loaded || Lara_GetItem() == nullptr
        || !Lara_IsControllable()) {
        return false;
    }

    return true;
}

GAME_BONUS_FLAG Game_GetBonusFlag(void)
{
    return m_BonusFlag;
}

void Game_SetBonusFlag(const GAME_BONUS_FLAG flag)
{
    m_BonusFlag = flag;
}

bool Game_IsBonusFlagSet(const GAME_BONUS_FLAG flag)
{
    return (m_BonusFlag & flag) != 0;
}

void Game_SetIsLevelComplete(const bool is_complete)
{
    m_IsLevelComplete = is_complete;
}

bool Game_IsLevelComplete(void)
{
    return m_IsLevelComplete;
}
