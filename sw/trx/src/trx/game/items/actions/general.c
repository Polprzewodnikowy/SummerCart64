#include <trx/game/game.h>
#include <trx/game/gym.h>
#include <trx/game/items.h>
#include <trx/game/rooms.h>

static void M_FinishLevel(ITEM *const item)
{
    Game_SetIsLevelComplete(true);
}

static void M_FlipMap(ITEM *const item)
{
    Room_FlipMap();
}

static void M_AssaultStart(ITEM *const item)
{
    Gym_TrackManager_Start(GYM_TRACK_ASSAULT);
    Room_SetFlipEffect(-1);
}

static void M_AssaultStop(ITEM *const item)
{
    Gym_TrackManager_Stop(GYM_TRACK_ASSAULT);
    Room_SetFlipEffect(-1);
}

static void M_AssaultReset(ITEM *const item)
{
    Gym_TrackManager_Reset(GYM_TRACK_ASSAULT);
    Room_SetFlipEffect(-1);
}

static void M_AssaultFinished(ITEM *const item)
{
    Gym_TrackManager_Finish(GYM_TRACK_ASSAULT);
    Room_SetFlipEffect(-1);
}

static void M_AssaultPenalty8(ITEM *const item)
{
    Gym_TrackManager_AddPenaltySeconds(GYM_TRACK_ASSAULT, 8);
    Room_SetFlipEffect(-1);
}

static void M_AssaultPenalty30(ITEM *const item)
{
    Gym_TrackManager_AddPenaltySeconds(GYM_TRACK_ASSAULT, 30);
    Room_SetFlipEffect(-1);
}

static void M_RacetrackStart(ITEM *const item)
{
    Gym_TrackManager_Start(GYM_TRACK_QUAD);
    Room_SetFlipEffect(-1);
}

static void M_RacetrackReset(ITEM *const item)
{
    Gym_TrackManager_Stop(GYM_TRACK_QUAD);
    Room_SetFlipEffect(-1);
}

static void M_RacetrackFinished(ITEM *const item)
{
    Gym_TrackManager_Finish(GYM_TRACK_QUAD);
    Room_SetFlipEffect(-1);
}

REGISTER_ITEM_ACTION(ITEM_ACTION_FINISH_LEVEL, M_FinishLevel)
REGISTER_ITEM_ACTION(ITEM_ACTION_FLIP_MAP, M_FlipMap)
REGISTER_ITEM_ACTION(ITEM_ACTION_ASSAULT_RESET, M_AssaultReset)
REGISTER_ITEM_ACTION(ITEM_ACTION_ASSAULT_STOP, M_AssaultStop)
REGISTER_ITEM_ACTION(ITEM_ACTION_ASSAULT_START, M_AssaultStart)
REGISTER_ITEM_ACTION(ITEM_ACTION_ASSAULT_FINISHED, M_AssaultFinished)
REGISTER_ITEM_ACTION(ITEM_ACTION_ASSAULT_PENALTY_8, M_AssaultPenalty8)
REGISTER_ITEM_ACTION(ITEM_ACTION_ASSAULT_PENALTY_30, M_AssaultPenalty30)
REGISTER_ITEM_ACTION(ITEM_ACTION_RACETRACK_START, M_RacetrackStart)
REGISTER_ITEM_ACTION(ITEM_ACTION_RACETRACK_RESET, M_RacetrackReset)
REGISTER_ITEM_ACTION(ITEM_ACTION_RACETRACK_FINISHED, M_RacetrackFinished)
