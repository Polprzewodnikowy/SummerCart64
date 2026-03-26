#pragma once

typedef enum {
    CLOCK_TIMER_SIM,
    CLOCK_TIMER_REAL,
} CLOCK_TIMER_TYPE;

typedef struct {
    double ref;
    CLOCK_TIMER_TYPE type;
} CLOCK_TIMER;

void ClockTimer_Sync(CLOCK_TIMER *timer);
double ClockTimer_PeekElapsed(const CLOCK_TIMER *timer);
double ClockTimer_TakeElapsed(CLOCK_TIMER *timer);
bool ClockTimer_CheckElapsed(const CLOCK_TIMER *timer, double sec);
bool ClockTimer_CheckElapsedAndTake(CLOCK_TIMER *timer, double sec);
