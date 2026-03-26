#include <trx/game/ui/elements/fps_counter.h>

#include <trx/core/memory.h>
#include <trx/game/clock.h>
#include <trx/game/ui/elements/label.h>

#include <stdio.h>

typedef struct UI_FPS_COUNTER_STATE {
    int32_t drawn_frames;
    int32_t fps_counter;
    CLOCK_TIMER timer;
} UI_FPS_COUNTER_STATE;

UI_FPS_COUNTER_STATE *UI_FPSCounter_Init(void)
{
    UI_FPS_COUNTER_STATE *const s = Memory_Alloc(sizeof(UI_FPS_COUNTER_STATE));
    s->timer.type = CLOCK_TIMER_REAL;
    return s;
}

void UI_FPSCounter_Free(UI_FPS_COUNTER_STATE *const s)
{
    if (s != nullptr) {
        Memory_Free(s);
    }
}

void UI_FPSCounter(UI_FPS_COUNTER_STATE *const s)
{
    UI_LabelFmt("%d FPS", s->fps_counter);
    s->drawn_frames++;
    if (ClockTimer_CheckElapsedAndTake(&s->timer, 1.0)) {
        s->fps_counter = s->drawn_frames;
        s->drawn_frames = 0;
    }
}
