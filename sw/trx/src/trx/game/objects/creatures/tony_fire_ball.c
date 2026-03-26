#include "tony_internal.h"

#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

static void M_TriggerFireBallFlame(
    const int16_t effect_num, const int32_t type, const int32_t xv,
    const int32_t yv, const int32_t zv)
{
    const ITEM *const lara_item = Lara_GetItem();

    const EFFECT *const effect = Effect_Get(effect_num);
    const int32_t dx = lara_item->pos.x - effect->pos.x;
    const int32_t dz = lara_item->pos.z - effect->pos.z;
    if (dx < -0x4000 || dx > 0x4000 || dz < -0x4000 || dz > 0x4000) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 255;
    spark->src_color.g = (Random_GetControl() & 0x1F) + 48;
    spark->src_color.b = 48;
    spark->dst_color.r = (Random_GetControl() & 0x3F) + 192;
    spark->dst_color.g = (Random_GetControl() & 0x3F) + 128;
    spark->dst_color.b = 32;

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
    spark->vel.x = (Random_GetControl() & 0xFF) + xv - 128;
    spark->vel.y = (int16_t)yv;
    spark->vel.z = (Random_GetControl() & 0xFF) + zv - 128;
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
    spark->size.width = (Random_GetControl() & 0x1F) + 64;
    spark->src_size.width = spark->size.width;
    spark->dst_size.width = spark->size.width >> 2;
    spark->size.height = spark->size.width;
    spark->src_size.height = spark->size.height;
    spark->dst_size.height = spark->size.height >> 2;

    if (!type || type == 1) {
        spark->gravity = (Random_GetControl() & 0x1F) + 16;
        spark->max_y_vel = (Random_GetControl() & 0xF) + 48;
        spark->scalar = 2;
        spark->vel.y *= -16;
    } else if (type == 4 || type == 5 || type == 6) {
        spark->max_y_vel = 0;
        spark->gravity = 0;
    } else if (type == 3) {
        spark->gravity = -16 - (Random_GetControl() & 0x1F);
        spark->max_y_vel = -64 - (Random_GetControl() & 0x1F);
        spark->scalar = 2;
        spark->vel.y <<= 4;
    } else if (type == 2) {
        spark->max_y_vel = 0;
        spark->gravity = 0;
        spark->scalar = 2;
    }
    Sparks_FinishSetup(spark);
}

void TonyBoss_TriggerFireBall(
    ITEM *const item, const int32_t type, const XYZ_32 *const pos,
    const int16_t room_num, int16_t angle, int32_t speed)
{
    XYZ_32 effect_pos = {};
    int32_t fall_speed;

    switch (type) {
    case 0:
        Collide_GetJointAbsPosition(item, &effect_pos, 10);
        angle = item->rot.y;
        fall_speed = -16;
        speed = 0;
        break;
    case 1:
        Collide_GetJointAbsPosition(item, &effect_pos, 13);
        angle = item->rot.y;
        fall_speed = -16;
        speed = 0;
        break;
    case 2:
        Collide_GetJointAbsPosition(item, &effect_pos, 13);
        speed = 160;
        fall_speed = -32 - (Random_GetControl() & 7);
        break;
    case 3:
        effect_pos = *pos;
        speed = 0;
        fall_speed = (Random_GetControl() & 3) + 4;
        break;
    case 4:
        effect_pos = *pos;
        speed += (Random_GetControl() & 3);
        angle = Random_GetControl() << 1;
        fall_speed = (Random_GetControl() & 3) - 2;
        break;
    case 5:
        effect_pos = *pos;
        speed = (Random_GetControl() & 7) + 48;
        angle += (Random_GetControl() & 0x1FFF) + 0x7000;
        fall_speed = -16 - (Random_GetControl() & 0xF);
        break;
    default:
        effect_pos = *pos;
        speed = (Random_GetControl() & 0x1F) + 32;
        angle = Random_GetControl() << 1;
        fall_speed = -32 - (Random_GetControl() & 0x1F);
        break;
    }

    const int16_t fx_num = Effect_Create(room_num);
    if (fx_num == NO_EFFECT) {
        return;
    }

    EFFECT *const effect = Effect_Get(fx_num);
    effect->pos = effect_pos;
    effect->rot.y = angle;
    effect->object_id = O_TONY_FIRE_BALL;
    effect->speed = speed;
    effect->fall_speed = fall_speed;
    effect->flag1 = type;
    effect->flag2 = (Random_GetControl() & 3) + 1;

    if (type == 5) {
        effect->flag2 <<= 1;
    } else if (type == 2) {
        effect->flag2 = 0;
    }
}

static void M_Control(const int16_t effect_num)
{
    const ITEM *const lara_item = Lara_GetItem();
    uint8_t falloffs[7] = { 16, 0, 14, 9, 7, 7, 7 };

    EFFECT *const effect = Effect_Get(effect_num);
    XYZ_32 old_pos = effect->pos;

    const int32_t time4 = Output_GetTimeInGame() * 4;

    if (!effect->flag1 || effect->flag1 == 1) {
        effect->fall_speed += (effect->fall_speed >> 3) + 1;
        CLAMPL(effect->fall_speed, -4096);
        effect->pos.y += effect->fall_speed;
        if (time4 & 4) {
            M_TriggerFireBallFlame(effect_num, effect->flag1, 0, 0, 0);
        }
    } else if (effect->flag1 == 3) {
        effect->fall_speed += 2;
        effect->pos.y += effect->fall_speed;

        if (time4 & 4) {
            M_TriggerFireBallFlame(effect_num, 3, 0, 0, 0);
        }
    } else {
        if (effect->flag1 != 2) {
            if (effect->speed > 48) {
                effect->speed--;
            }
        }

        effect->fall_speed += effect->flag2;

        if (effect->fall_speed > 512) {
            effect->fall_speed = 512;
        }

        effect->pos.x += effect->speed * Math_Sin(effect->rot.y) >> W2V_SHIFT;
        effect->pos.y += effect->fall_speed >> 1;
        effect->pos.z += effect->speed * Math_Cos(effect->rot.y) >> W2V_SHIFT;
        const int32_t dx = (old_pos.x - effect->pos.x) << 3;
        const int32_t dy = (old_pos.y - effect->pos.y) << 3;
        const int32_t dz = (old_pos.z - effect->pos.z) << 3;

        if (time4 & 4) {
            M_TriggerFireBallFlame(effect_num, effect->flag1, dx, dy, dz);
        }
    }

    int16_t room_num = effect->room_num;
    SECTOR *sector = Room_GetSector(effect->pos, &room_num);
    const int32_t h = Room_GetHeight(sector, effect->pos);
    const int32_t c = Room_GetCeiling(sector, effect->pos);

    if (effect->pos.y >= h || effect->pos.y < c) {
        if (!effect->flag1 || effect->flag1 == 1 || effect->flag1 == 2
            || effect->flag1 == 3) {
            Sparks_TriggerExplosionSparks(old_pos, 3, -2, 0, effect->room_num);

            if (!effect->flag1 || effect->flag1 == 1) {
                for (int32_t i = 0; i < 2; i++) {
                    Sparks_TriggerExplosionSparks(
                        old_pos, 3, -1, 0, effect->room_num);
                }
            }

            XYZ_32 pos = old_pos;

            int32_t count;
            if (effect->flag1 == 2) {
                count = 7;
            } else {
                count = 3;
            }

            int32_t type;
            if (effect->flag1 == 2) {
                type = 5;
            } else if (effect->flag1 == 3) {
                type = 6;
            } else {
                type = 4;
            }

            for (int32_t i = 0; i < count; i++) {
                TonyBoss_TriggerFireBall(
                    nullptr, type, &pos, effect->room_num, effect->rot.y,
                    (i << 2) + 32);
            }

            if (!effect->flag1 || effect->flag1 == 1) {
                room_num = lara_item->room_num;
                sector = Room_GetSector(lara_item->pos, &room_num);
                pos.x = (Random_GetControl() & 0x3FF) + lara_item->pos.x - 512;
                pos.z = (Random_GetControl() & 0x3FF) + lara_item->pos.z - 512;
                pos.y = Room_GetCeiling(sector, lara_item->pos) + 256;
                Sparks_TriggerExplosionSparks(pos, 3, -2, 0, room_num);
                TonyBoss_TriggerFireBall(nullptr, 3, &pos, room_num, 0, 0);
            }
        }

        Effect_Kill(effect_num);
        return;
    }

    if (Room_Get(room_num)->flags.underwater) {
        Effect_Kill(effect_num);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!lara->burn && Lara_IsNearItem(&effect->pos, 200)) {
        Effect_Kill(effect_num);
        Lara_TakeDamage(200, true);
        Lara_CatchFire();
        return;
    }

    if (effect->room_num != room_num) {
        Effect_UpdateRoom(effect_num, lara_item->room_num);
    }

    if (falloffs[effect->flag1]) {
        const uint8_t r = Random_GetControl();
        const RGB_888 color = {
            .r = 255 - ((r >> 4) & 0x1F),
            .g = 192 - ((r >> 6) & 0x1F),
            .b = r & 0x3F,
        };
        Output_AddDynamicLightRGB(effect->pos, falloffs[effect->flag1], color);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
}

REGISTER_OBJECT(O_TONY_FIRE_BALL, M_Setup)
