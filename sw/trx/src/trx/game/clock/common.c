#include <trx/game/clock/common.h>

#include <trx/config.h>
#include <trx/game/clock/const.h>
#include <trx/game/clock/timer.h>
#include <trx/game/clock/turbo.h>

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static bool m_Disabled = false;
static Uint64 m_LastCounter = 0;
static Uint64 m_InitCounter = 0;
static Uint64 m_Frequency = 0;
static double m_Accumulator = 0.0;
static struct {
    double real_time_at_last_change;
    double sim_time_at_last_change;
    double sim_speed;
} m_Priv;

// Fixed‐FPS simulation in headless mode
static bool m_HeadlessFixedFPS = false;
static double m_HeadlessFPS_DT = 0.0;
static double m_HeadlessOffset = 0.0;
static double m_HeadlessAnchor = 0.0;

static double M_GetHighPrecisionCounter(void)
{
    if (m_HeadlessFixedFPS) {
        // Return virtual time = anchor + offset
        return m_HeadlessAnchor + m_HeadlessOffset;
    }
    return (SDL_GetPerformanceCounter() - m_InitCounter) / (double)m_Frequency;
}

void Clock_Init(void)
{
    m_Frequency = SDL_GetPerformanceFrequency();
    m_InitCounter = SDL_GetPerformanceCounter();
}

void Clock_DisableWait(void)
{
    m_Disabled = true;
}

void Clock_EnableHeadlessFixedFPS(int32_t fps)
{
    if (fps <= 0) {
        m_HeadlessFixedFPS = false;
        return;
    }

    // Anchor to current real time, reset offset
    m_HeadlessAnchor = M_GetHighPrecisionCounter();
    m_HeadlessOffset = 0.0;
    m_HeadlessFPS_DT = 1.0 / (double)fps;
    m_HeadlessFixedFPS = true;
}

size_t Clock_GetDateTime(char *const buffer, const size_t size)
{
    time_t lt = time(0);
    struct tm *tptr = localtime(&lt);

    return snprintf(
        buffer, size, "%04d%02d%02d_%02d%02d%02d", tptr->tm_year + 1900,
        tptr->tm_mon + 1, tptr->tm_mday, tptr->tm_hour, tptr->tm_min,
        tptr->tm_sec);
}

int32_t Clock_GetCurrentFPS(void)
{
    return g_Config.rendering.fps;
}

int32_t Clock_GetFrameAdvance(void)
{
    return Clock_GetCurrentFPS() == 30 ? 2 : 1;
}

void Clock_SyncTick(void)
{
    m_LastCounter = SDL_GetPerformanceCounter();
    m_Accumulator = 0.0;
}

int32_t Clock_WaitTick(void)
{
    if (m_Disabled && m_HeadlessFixedFPS) {
        // Advance virtual time by one fixed frame
        m_HeadlessOffset += m_HeadlessFPS_DT;
        return 1;
    }
    if (m_Disabled) {
        return 1;
    }
    const Uint64 current_counter = SDL_GetPerformanceCounter();

    // If this is the first call, just initialize and return a frame.
    if (m_LastCounter == 0) {
        m_LastCounter = current_counter;
        return 1;
    }

    const int32_t fps = Clock_GetCurrentFPS();
    const double speed_multiplier = Clock_GetSpeedMultiplier();

    // The duration of one frame in performance counter units
    const double frame_ticks = m_Frequency / (fps * speed_multiplier);

    // Calculate elapsed ticks since last call
    const double elapsed_ticks = (double)(current_counter - m_LastCounter);

    // Add the elapsed ticks to the accumulator
    m_Accumulator += elapsed_ticks;

    // Determine how many frames we can "release" from the accumulator
    int32_t frames = (int32_t)(m_Accumulator / frame_ticks);

    if (frames < 1) {
        // Not enough accumulated time for even one frame

        // Calculate how long we should wait (in ms) to hit the frame boundary
        double needed = frame_ticks - m_Accumulator;
        double delay_ms = (needed / m_Frequency) * 1000.0;

        if (delay_ms > 0) {
            SDL_Delay((Uint32)delay_ms);
        }

        // After waiting, measure again to be accurate
        const Uint64 after_delay_counter = SDL_GetPerformanceCounter();
        const double after_delay_elapsed =
            (double)(after_delay_counter - current_counter);
        m_Accumulator += after_delay_elapsed;

        // Now, we should have at least one frame available
        frames = (int32_t)(m_Accumulator / frame_ticks);
        if (frames < 1) {
            // To avoid a possible floating-point corner case, ensure at least
            // one frame
            frames = 1;
        }
    }

    // Consume the frames from the m_Accumulator
    m_Accumulator -= frames * frame_ticks;

    // Update the last counter to the current performance counter
    m_LastCounter = SDL_GetPerformanceCounter();

    return frames;
}

double Clock_GetRealTime(void)
{
    return M_GetHighPrecisionCounter();
}

double Clock_GetSimTime(void)
{
    const double real_now = M_GetHighPrecisionCounter();
    const double real_delta = real_now - m_Priv.real_time_at_last_change;
    return m_Priv.sim_time_at_last_change + real_delta * m_Priv.sim_speed;
}

void Clock_SetSimSpeed(const double new_speed)
{
    // First, figure out how much sim time has passed so far
    const double prev_sim_time = Clock_GetSimTime();
    // Then re-anchor the reference point
    m_Priv.real_time_at_last_change = M_GetHighPrecisionCounter();
    m_Priv.sim_time_at_last_change = prev_sim_time;
    m_Priv.sim_speed = new_speed;
}
