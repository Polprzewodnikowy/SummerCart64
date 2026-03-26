#include "sophia_internal.h"

#include <trx/core/math/func.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sparks.h>

static const uint8_t m_Falloffs[2] = { 13, 7 };

static void M_TriggerPlasmaBallFlame(const int16_t effect_num, const XYZ_32 vel)
{
    const EFFECT *const effect = Effect_Get(effect_num);
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = lara_item->pos.x - effect->pos.x;
    const int32_t dz = lara_item->pos.z - effect->pos.z;

    const int32_t max_dist = 16 * WALL_L;
    if (dx < -max_dist || dx > max_dist || dz < -max_dist || dz > max_dist) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 48;
    spark->src_color.g = 255;
    spark->src_color.b = (Random_GetControl() & 0x1F) + 48;
    spark->dst_color.r = 32;
    spark->dst_color.g = (Random_GetControl() & 0x3F) + 192;
    spark->dst_color.b = (Random_GetControl() & 0x3F) + 128;

    spark->fade_to_black = 8;
    spark->col_fade_speed = (Random_GetControl() & 3) + 12;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->life = (Random_GetControl() & 7) + 24;
    spark->s_life = spark->life;
    spark->extras = 0;
    spark->dynamic = -1;
    spark->pos.x = (Random_GetControl() & 0xF) - 8;
    spark->pos.y = 0;
    spark->pos.z = (Random_GetControl() & 0xF) - 8;
    spark->vel.x = vel.x + (Random_GetControl() & 0xFF) - 128;
    spark->vel.y = vel.y;
    spark->vel.z = vel.z + (Random_GetControl() & 0xFF) - 128;
    spark->friction = 5;

    if (Random_GetControl() & 1) {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_FX | SPARK_F_ROTATE
            | SPARK_F_SPRITE | SPARK_F_SCALE;
        spark->rot_angle = Random_GetControl() & 0xFFF;

        if (Random_GetControl() & 1) {
            spark->rot_add = -16 - (Random_GetControl() & 0xF);
        } else {
            spark->rot_add = (Random_GetControl() & 0xF) + 16;
        }
    } else {
        spark->flags =
            SPARK_F_ALT_SPRITE | SPARK_F_FX | SPARK_F_SPRITE | SPARK_F_SCALE;
    }

    spark->effect_num = effect_num;
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->scalar = 1;
    spark->max_y_vel = 0;
    spark->gravity = 0;
    spark->size.width = (Random_GetControl() & 0x1F) + 64;
    spark->size.height = spark->size.width;
    spark->src_size.width = spark->size.width;
    spark->src_size.height = spark->size.height;
    spark->dst_size.width = spark->size.width >> 2;
    spark->dst_size.height = spark->size.height >> 2;
    Sparks_FinishSetup(spark);
}

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    effect->fall_speed++;
    const int32_t old_y = effect->pos.y;

    if (effect->speed > 8) {
        effect->speed -= 2;
    }
    if (effect->rot.x > -15360) {
        effect->rot.x -= 256;
    }

    const int32_t speed =
        (effect->speed * Math_Cos(effect->rot.x)) >> W2V_SHIFT;
    effect->pos = XYZ_32_OffsetYaw(effect->pos, effect->rot.y, speed);
    effect->pos.y += effect->fall_speed
        - ((effect->speed * Math_Sin(effect->rot.x)) >> W2V_SHIFT);

    const int32_t time4 = Output_GetTimeInGame() * 4;
    if ((time4 & 0xF) == 0) {
        M_TriggerPlasmaBallFlame(
            effect_num,
            (XYZ_32) { .x = 0, .y = ABS(old_y - effect->pos.y) << 3, .z = 0 });
    }

    int16_t room_num = effect->room_num;
    const SECTOR *const sector = Room_GetSector(effect->pos, &room_num);
    const int32_t h = Room_GetHeight(sector, effect->pos);
    const int32_t c = Room_GetCeiling(sector, effect->pos);

    if (effect->pos.y >= h || effect->pos.y < c
        || Room_Get(room_num)->flags.underwater) {
        Effect_Kill(effect_num);
        return;
    }

    if (effect->flag2 == 0 && Lara_IsNearItem(&effect->pos, 200)) {
        Lara_TakeDamage(25, true);
        Effect_Kill(effect_num);
        return;
    }

    if (effect->room_num != room_num) {
        Effect_UpdateRoom(effect_num, room_num);
    }

    const int32_t color_base = Random_GetControl();
    const RGB_888 color = {
        .r = color_base & 0x3F,
        .g = 255 - ((color_base >> 4) & 0x1F),
        .b = 192 - ((color_base >> 6) & 0x1F),
    };
    Output_AddDynamicLightRGB(effect->pos, m_Falloffs[effect->flag1], color);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
}

void Sophia_TriggerPlasmaBall(
    const int32_t type, const XYZ_32 pos, const int16_t room_num,
    const int16_t angle)
{
    const int16_t fx_num = Effect_Create(room_num);
    if (fx_num == NO_ITEM) {
        return;
    }

    EFFECT *const effect = Effect_Get(fx_num);
    effect->speed = (Random_GetControl() & 0x1F) + 64;
    effect->pos = pos;
    effect->rot.x = DEG_45;
    effect->rot.y = angle + Random_GetControl() + DEG_90;
    effect->object_id = O_SOPHIA_PLASMA_BALL;
    effect->fall_speed = 0;
    effect->flag1 = 1;
    effect->flag2 = type == 2;
}

REGISTER_OBJECT(O_SOPHIA_PLASMA_BALL, M_Setup)
