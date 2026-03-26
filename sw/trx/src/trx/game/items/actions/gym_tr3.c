#include <trx/game/game.h>
#include <trx/game/items/actions.h>
#include <trx/game/lara.h>
#include <trx/game/music.h>
#include <trx/game/rooms.h>

static int32_t m_ExerciseNumber = 0;

static void M_PlayExerciseTrack(
    const int32_t expected_num, const MUSIC_TRX_ID track)
{
    if (!Game_IsInGym()) {
        m_ExerciseNumber = 0;
        return;
    }

    if (m_ExerciseNumber == expected_num) {
        Music_Play(track, MPM_ONCE);
        m_ExerciseNumber++;
    }
}

static void M_Exercise01(ITEM *const item)
{
    M_PlayExerciseTrack(0, MX_TR3_GYM_EXERCISE_01);
    Room_SetFlipEffect(-1);
}

static void M_Exercise02(ITEM *const item)
{
    M_PlayExerciseTrack(1, MX_TR3_GYM_EXERCISE_02);
    Room_SetFlipEffect(-1);
}

static void M_Exercise03(ITEM *const item)
{
    M_PlayExerciseTrack(2, MX_TR3_GYM_EXERCISE_03);
    Room_SetFlipEffect(-1);
}

static void M_Exercise04(ITEM *const item)
{
    M_PlayExerciseTrack(3, MX_TR3_GYM_EXERCISE_04);
    Room_SetFlipEffect(-1);
}

static void M_Exercise05(ITEM *const item)
{
    M_PlayExerciseTrack(4, MX_TR3_GYM_EXERCISE_05);
    Room_SetFlipEffect(-1);
}

static void M_Exercise06(ITEM *const item)
{
    M_PlayExerciseTrack(5, MX_TR3_GYM_EXERCISE_06);
    Room_SetFlipEffect(-1);
}

static void M_Exercise07(ITEM *const item)
{
    M_PlayExerciseTrack(6, MX_TR3_GYM_EXERCISE_07);
    Room_SetFlipEffect(-1);
}

static void M_Exercise08(ITEM *const item)
{
    M_PlayExerciseTrack(7, MX_TR3_GYM_EXERCISE_08);
    Room_SetFlipEffect(-1);
}

static void M_Exercise09(ITEM *const item)
{
    M_PlayExerciseTrack(8, MX_TR3_GYM_EXERCISE_09);
    Room_SetFlipEffect(-1);
}

static void M_Exercise10(ITEM *const item)
{
    M_PlayExerciseTrack(9, MX_TR3_GYM_EXERCISE_10);
    Room_SetFlipEffect(-1);
}

static void M_Exercise11(ITEM *const item)
{
    M_PlayExerciseTrack(10, MX_TR3_GYM_EXERCISE_11);
    Room_SetFlipEffect(-1);
}

static void M_Exercise12(ITEM *const item)
{
    M_PlayExerciseTrack(11, MX_TR3_GYM_EXERCISE_12);
    Room_SetFlipEffect(-1);
}

static void M_Exercise13(ITEM *const item)
{
    M_PlayExerciseTrack(12, MX_TR3_GYM_EXERCISE_13);
    Room_SetFlipEffect(-1);
}

static void M_Exercise14(ITEM *const item)
{
    M_PlayExerciseTrack(13, MX_TR3_GYM_EXERCISE_14);
    Room_SetFlipEffect(-1);
}

static void M_Exercise15(ITEM *const item)
{
    M_PlayExerciseTrack(14, MX_TR3_GYM_EXERCISE_15);
    Room_SetFlipEffect(-1);
}

static void M_Exercise16(ITEM *const item)
{
    M_PlayExerciseTrack(15, MX_TR3_GYM_EXERCISE_16);
    Room_SetFlipEffect(-1);
}

static void M_Exercise17(ITEM *const item)
{
    M_PlayExerciseTrack(16, MX_TR3_GYM_EXERCISE_17);
    Room_SetFlipEffect(-1);
}

static void M_Exercise18_SurfaceOnly(ITEM *const item)
{
    if (!Game_IsInGym()) {
        m_ExerciseNumber = 0;
        Room_SetFlipEffect(-1);
        return;
    }

    if (m_ExerciseNumber == 17
        && Lara_GetLaraInfo()->water_status == LWS_SURFACE) {
        Music_Play(MX_TR3_GYM_EXERCISE_18, MPM_ONCE);
        m_ExerciseNumber++;
    }
    Room_SetFlipEffect(-1);
}

static void M_Exercise19_Reset(ITEM *const item)
{
    if (!Game_IsInGym()) {
        m_ExerciseNumber = 0;
        Room_SetFlipEffect(-1);
        return;
    }

    if (m_ExerciseNumber == 18) {
        Music_Play(MX_TR3_GYM_EXERCISE_19, MPM_ONCE);
        m_ExerciseNumber = 0;
    }
    Room_SetFlipEffect(-1);
}

static void M_ResetExercises(ITEM *const item)
{
    m_ExerciseNumber = 0;
    Room_SetFlipEffect(-1);
}

REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_1, M_Exercise01)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_2, M_Exercise02)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_3, M_Exercise03)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_4, M_Exercise04)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_5, M_Exercise05)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_6, M_Exercise06)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_7, M_Exercise07)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_8, M_Exercise08)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_9, M_Exercise09)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_10, M_Exercise10)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_11, M_Exercise11)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_12, M_Exercise12)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_13, M_Exercise13)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_14, M_Exercise14)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_15, M_Exercise15)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_16, M_Exercise16)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_17, M_Exercise17)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_18, M_Exercise18_SurfaceOnly)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_19, M_Exercise19_Reset)
REGISTER_ITEM_ACTION(ITEM_ACTION_GYM_HINT_RESET, M_ResetExercises)
