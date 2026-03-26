#include <trx/game/fader.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/clock.h>
#include <trx/game/shell.h>

static void M_Init(FADER *const fader, FADER_ARGS args)
{
    CLAMP(args.initial, 0.0f, 1.0f);
    CLAMP(args.target, 0.0f, 1.0f);

    if (args.from_current) {
        args.initial = Fader_GetCurrentValue(fader);

        // Reduce duration proportionally to how close the initial value is to
        // the target.
        float ratio = ABS(args.target - args.initial);
        CLAMP(ratio, 0.0f, 1.0f);
        args.duration *= ratio;
        if (ratio < 1.0f) {
            args.debuff = 0.0f;
        }
    }

    fader->args = args;
    ClockTimer_Sync(&fader->timer);
}

void Fader_InitTo(
    FADER *const fader, const float initial, const float target,
    const float duration)
{
    M_Init(
        fader,
        (FADER_ARGS) {
            .from_current = false,
            .initial = initial,
            .target = target,
            .duration = duration,
            .debuff = 0.0f,
        });
}

void Fader_InitToHold(
    FADER *const fader, const float initial, const float target,
    const float duration, const float debuff)
{
    M_Init(
        fader,
        (FADER_ARGS) {
            .from_current = false,
            .initial = initial,
            .target = target,
            .duration = duration,
            .debuff = debuff,
        });
}

void Fader_InitFromCurrent(
    FADER *const fader, const float target, const float duration)
{
    M_Init(
        fader,
        (FADER_ARGS) {
            .from_current = true,
            .initial = 0.0f,
            .target = target,
            .duration = duration,
            .debuff = 0.0f,
        });
}

void Fader_InitFromCurrentHold(
    FADER *const fader, const float target, const float duration,
    const float debuff)
{
    M_Init(
        fader,
        (FADER_ARGS) {
            .from_current = true,
            .initial = 0.0f,
            .target = target,
            .duration = duration,
            .debuff = debuff,
        });
}

float Fader_GetCurrentValue(const FADER *const fader)
{
    if (!g_Config.visuals.enable_fade_effects || fader->args.duration <= 0.0) {
        return fader->args.target;
    }
    const float elapsed_time = ClockTimer_PeekElapsed(&fader->timer);
    const float target_time = fader->args.duration;
    float ratio = elapsed_time / target_time;
    CLAMP(ratio, 0.0, 1.0);
    float value = fader->args.initial
        + (fader->args.target - fader->args.initial) * (float)ratio;
    CLAMP(value, 0.0f, 1.0f);
    return value;
}

bool Fader_IsActive(const FADER *const fader)
{
    if (!g_Config.visuals.enable_fade_effects || fader->args.duration <= 0.0) {
        return false;
    }
    const float elapsed_time = ClockTimer_PeekElapsed(&fader->timer);
    const float target_time = fader->args.duration + fader->args.debuff;
    return elapsed_time < target_time;
}
