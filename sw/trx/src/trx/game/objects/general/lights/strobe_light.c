#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/math.h>
#include <trx/game/collision/los.h>
#include <trx/game/game_flow.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>

typedef struct {
    int32_t life;
    bool alarm_active;
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_OPTIONAL(JSON_READ(io, "life", &p->life));
    JSON_OPTIONAL(JSON_READ(io, "alarm_active", &p->alarm_active));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "life", p->life);
    JSONW_WRITE(io, "alarm_active", p->alarm_active);
}

void M_TriggerAlertLight(
    const XYZ_32 pos, const RGB_888 color, const int16_t angle,
    const int16_t room_num)
{
    GAME_VECTOR src = { .pos = pos, .room_num = room_num };
    Room_GetSector(pos, &src.room_num);

    const int32_t dist = 8 * WALL_L;
    GAME_VECTOR dst = {
        .pos = {
            .x = pos.x + ((dist * Math_Sin(angle)) >> W2V_SHIFT),
            .y = pos.y,
            .z = pos.z + ((dist * Math_Cos(angle)) >> W2V_SHIFT),
        },
    };

    if (!LOS_Check(&src, &dst, false)) {
        Output_AddDynamicLightRGB(dst.pos, 8, color);
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    if (GF_BadGetLevelNum() == 15 && !p->alarm_active) {
        return;
    }

    item->rot.y += 2912;
    const int16_t angle = item->rot.y + 0x5800;

    M_TriggerAlertLight(
        (XYZ_32) { item->pos.x, item->pos.y - WALL_L / 2, item->pos.z },
        (RGB_888) { 255, 64, 0 }, angle, item->room_num);

    Output_AddDynamicLightRGB(
        (XYZ_32) {
            item->pos.x + ((STEP_L * Math_Sin(angle)) >> W2V_SHIFT),
            item->pos.y - STEP_L * 3,
            item->pos.z + ((STEP_L * Math_Cos(angle)) >> W2V_SHIFT),
        },
        6, (RGB_888) { 255, 96, 0 });

    const int32_t time4 = Output_GetTimeInGame() * 4;
    if (!(time4 & 0x7F)) {
        Sound_Effect(SFX_ALARM_1, &item->pos, SPM_NORMAL);
    }

    p->life++;

    if (p->life > 60 * LOGIC_FPS) {
        p->alarm_active = false;
        p->life = 0;
    }
}

static void M_HandleEvent(
    ITEM *const item, const OBJECT_EVENT event, const void *const data)
{
    M_PRIV *const p = item->priv;
    if (event != OBJECT_EVENT_ALERT) {
        return;
    }

    p->alarm_active = true;
    p->life = 0;
}

static void M_Setup(OBJECT *const obj)
{
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->control_func = M_Control;
    obj->event_func = M_HandleEvent;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_STROBE_LIGHT, M_Setup)
