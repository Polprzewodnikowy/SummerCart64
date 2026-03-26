#include <trx/game/clock/timer.h>

#include <trx/game/clock/common.h>

static double M_GetTime(const CLOCK_TIMER *const timer)
{
    return timer->type == CLOCK_TIMER_REAL ? Clock_GetRealTime()
                                           : Clock_GetSimTime();
}

void ClockTimer_Sync(CLOCK_TIMER *const timer)
{
    timer->ref = M_GetTime(timer);
}

double ClockTimer_PeekElapsed(const CLOCK_TIMER *const timer)
{
    return M_GetTime(timer) - timer->ref;
}

double ClockTimer_TakeElapsed(CLOCK_TIMER *const timer)
{
    const double prev_time_sec = timer->ref;
    const double current_time_sec = M_GetTime(timer);
    timer->ref = current_time_sec;
    return current_time_sec - prev_time_sec;
}

bool ClockTimer_CheckElapsed(const CLOCK_TIMER *const timer, double sec)
{
    return (M_GetTime(timer) - timer->ref) >= sec;
}

bool ClockTimer_CheckElapsedAndTake(CLOCK_TIMER *const timer, double sec)
{
    const double current_time_sec = M_GetTime(timer);
    if ((current_time_sec - timer->ref) >= sec) {
        timer->ref = current_time_sec;
        return true;
    }
    return false;
}
