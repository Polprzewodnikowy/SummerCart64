#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/game_buf.h>
#include <trx/game/game_flow/util.h>
#include <trx/game/interpolation.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/output/textures.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

#include <stdint.h>

#define M_SHOAL_COUNT 8
#define M_FISH_PER_SHOAL 24

#define M_LEVEL_RANGES(id, ...)                                                \
    { id, sizeof((XYZ_16[])__VA_ARGS__) / sizeof(XYZ_16), __VA_ARGS__ }

typedef struct {
    int16_t angle;
    uint8_t speed;
    bool on;
    int16_t angle_time;
    int16_t speed_time;
    XYZ_16 range;
} M_LEADER;

typedef struct {
    XYZ_16 pos;
    uint16_t angle; // 0..4095
    int16_t dest_y;
    int8_t ang_add;
    uint8_t speed;
    uint8_t acc;
    uint8_t swim;
    struct {
        struct {
            XYZ_16 pos;
            uint16_t angle; // 0..4095
            uint8_t swim;
        } prev, result;
    } interp;
} M_FISH;

typedef struct {
    int32_t leader_num;
    M_FISH fish[M_FISH_PER_SHOAL + 1];
    M_LEADER leader;
    int32_t piranha_hit_wait;
    int16_t carcass_item_num;
} M_PRIV;

typedef struct {
    int32_t level_id;
    int32_t range_count;
    XYZ_16 ranges[M_SHOAL_COUNT];
} M_FISH_LEVEL_CONFIG;

static const M_FISH_LEVEL_CONFIG m_FishLevelConfigs[] = {
    M_LEVEL_RANGES(
        1,
        {
            { .x = 8, .z = 20, .y = 3 },
        }),
    M_LEVEL_RANGES(
        2,
        {
            { .x = 4, .z = 4, .y = 2 },
            { .x = 4, .z = 16, .y = 2 },
            { .x = 4, .z = 28, .y = 3 },
        }),
    M_LEVEL_RANGES(
        3,
        {
            { .x = 4, .z = 12, .y = 1 },
            { .x = 0, .z = 12, .y = 2 },
            { .x = 8, .z = 4, .y = 2 },
            { .x = 4, .z = 8, .y = 1 },
            { .x = 4, .z = 16, .y = 2 },
            { .x = 4, .z = 24, .y = 1 },
            { .x = 12, .z = 4, .y = 1 },
            { .x = 16, .z = 4, .y = 1 },
        }),
    M_LEVEL_RANGES(
        0,
        {
            { .x = 4, .z = 4, .y = 1 },
            { .x = 16, .z = 8, .y = 2 },
            { .x = 24, .z = 8, .y = 2 },
            { .x = 8, .z = 16, .y = 2 },
            { .x = 8, .z = 12, .y = 1 },
            { .x = 20, .z = 8, .y = 2 },
            { .x = 16, .z = 8, .y = 1 },
        }),
    M_LEVEL_RANGES(
        5,
        {
            { .x = 12, .z = 12, .y = 6 },
            { .x = 12, .z = 20, .y = 6 },
            { .x = 20, .z = 4, .y = 8 },
        }),
    M_LEVEL_RANGES(
        6,
        {
            { .x = 20, .z = 4, .y = 6 },
        }),
    M_LEVEL_RANGES(
        7,
        {
            { .x = 16, .z = 16, .y = 8 },
            { .x = 4, .z = 8, .y = 5 },
        }),
};

static bool M_IsValidShoalNum(const int32_t shoal_num)
{
    return shoal_num >= 0 && shoal_num < M_SHOAL_COUNT;
}

static uint16_t M_GetFishAngle12(
    const int32_t x1, const int32_t z1, const int32_t x2, const int32_t z2)
{
    const int32_t dx = x2 - x1;
    const int32_t dz = z2 - z1;
    const int32_t fish_angle16 = Math_Atan(dx, dz) - DEG_90;
    return (fish_angle16 >> 4) & 0xFFF; // 0..4095
}

static int32_t M_GetAngle12Diff(const int32_t a, const int32_t b)
{
    int32_t diff = a - b;
    if (diff > 2048) {
        diff -= 4096;
    } else if (diff < -2048) {
        diff += 4096;
    }
    return diff;
}

static bool M_FishNearItem(
    const XYZ_32 *const pos, const int32_t dist, const ITEM *const item)
{
    const int32_t dx = pos->x - item->pos.x;
    const int32_t dy = ABS(pos->y - item->pos.y);
    const int32_t dz = pos->z - item->pos.z;

    // clang-format off
    if (dx < -dist || dx > dist ||
        dz < -dist || dz > dist ||
        dy < -3072 || dy > 3072 ||
        SQUARE(dz) + SQUARE(dx) > SQUARE(dist)
        || dy > dist) {
        return false;
    }
    // clang-format on

    return true;
}

static void M_SetupShoal(M_PRIV *const p, const int32_t shoal_num)
{
    if (p == nullptr || !M_IsValidShoalNum(shoal_num)) {
        return;
    }

    M_LEADER *const leader = &p->leader;

    if (g_TRVersion < 3) {
        goto fallback;
    }

    const int32_t lvl = GF_BadGetLevelNum();
    for (size_t i = 0; i < ARRAY_SIZE(m_FishLevelConfigs); i++) {
        const M_FISH_LEVEL_CONFIG *cfg = &m_FishLevelConfigs[i];
        if (cfg->level_id == lvl && shoal_num < cfg->range_count) {
            const XYZ_16 *const r = &cfg->ranges[shoal_num];
            leader->range.x = (r->x + 2) << 8;
            leader->range.y = r->y << 8;
            leader->range.z = (r->z + 2) << 8;
            return;
        }
    }

fallback:
    leader->range.x = 256;
    leader->range.y = 256;
    leader->range.z = 256;
}

static void M_SetupFish(M_PRIV *const p, const ITEM *const item)
{
    if (p == nullptr || item == nullptr || !M_IsValidShoalNum(p->leader_num)) {
        return;
    }

    M_LEADER *const leader = &p->leader;
    M_FISH *fish = &p->fish[0];

    const int16_t x = leader->range.x;
    const int16_t y = leader->range.y;
    const int16_t z = leader->range.z;

    fish->pos.x = 0;
    fish->pos.y = 0;
    fish->pos.z = 0;
    fish->angle = 0;
    fish->speed = ((Random_GetControl() & 0x3F) + 8);
    fish->swim = (Random_GetControl() & 0x3F);
    fish->interp.prev.pos = fish->pos;
    fish->interp.prev.angle = fish->angle;
    fish->interp.prev.swim = fish->swim;
    fish->interp.result.pos = fish->pos;
    fish->interp.result.angle = fish->angle;
    fish->interp.result.swim = fish->swim;

    for (int32_t i = 0; i < M_FISH_PER_SHOAL; i++) {
        fish = &p->fish[i + 1];
        fish->pos.x = Random_GetControl() % (x << 1) - x;
        fish->pos.y = Random_GetControl() % y;
        fish->pos.z = Random_GetControl() % (z << 1) - z;
        fish->dest_y = Random_GetControl() % y;
        fish->angle = Random_GetControl() & 0xFFF;
        fish->speed = (Random_GetControl() & 0x1F) + 32;
        fish->swim = Random_GetControl() & 0x3F;
        fish->interp.prev.pos = fish->pos;
        fish->interp.prev.angle = fish->angle;
        fish->interp.prev.swim = fish->swim;
        fish->interp.result.pos = fish->pos;
        fish->interp.result.angle = fish->angle;
        fish->interp.result.swim = fish->swim;
    }

    leader->on = true;
    leader->angle = 0;
    leader->speed = (Random_GetControl() & 0x7F) + 32;
    leader->angle_time = 0;
    leader->speed_time = 0;
    p->piranha_hit_wait = 0;
}

static void M_FindCarcass(const ITEM *const shoal_item)
{
    M_PRIV *const p = shoal_item->priv;
    p->carcass_item_num = Item_FindTypeInRoom(shoal_item->room_num, O_CARCASS);
}

static bool M_IsTargetable(const ITEM *const item)
{
    return false;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    const int32_t leader_num = item->hit_points;
    if (!M_IsValidShoalNum(leader_num)) {
        return;
    }

    M_PRIV *const p = item->priv;
    if (p == nullptr) {
        return;
    }

    if (p->leader_num != leader_num) {
        p->leader_num = leader_num;
        p->leader.on = false;
    }
    M_LEADER *const leader = &p->leader;
    if (!leader->on) {
        M_SetupShoal(p, leader_num);
        M_SetupFish(p, item);
    }

    const ITEM *const lara_item = Lara_GetItem();

    int32_t piranha_attack = 0;
    if (item->object_id == O_PIRAHNAS && lara_item != nullptr) {
        if (p->carcass_item_num == NO_ITEM) {
            M_FindCarcass(item);
        }
        if (p->carcass_item_num != NO_ITEM) {
            piranha_attack = 2;
        } else {
            piranha_attack = lara_item->room_num == item->room_num;
        }
    }

    if (p->piranha_hit_wait != 0) {
        p->piranha_hit_wait--;
    }

    M_FISH *const leader_fish = &p->fish[0];

    const ITEM *enemy = lara_item;
    if (piranha_attack != 0) {
        if (piranha_attack >= 2) {
            enemy = Item_Get(p->carcass_item_num);
        }
        leader_fish->angle = M_GetFishAngle12(
            item->pos.x + leader_fish->pos.x, item->pos.z + leader_fish->pos.z,
            enemy->pos.x, enemy->pos.z);
        leader->angle = leader_fish->angle;
        leader->speed = (Random_GetControl() & 0x3F) - 64;
    }

    int32_t diff = M_GetAngle12Diff(leader_fish->angle, leader->angle);
    if (diff > 128) {
        leader_fish->ang_add -= 4;
        CLAMPL(leader_fish->ang_add, -120);
    } else if (diff < -128) {
        leader_fish->ang_add += 4;
        CLAMPG(leader_fish->ang_add, 120);
    } else {
        leader_fish->ang_add -= leader_fish->ang_add >> 2;
        if (ABS(leader_fish->ang_add) < 4) {
            leader_fish->ang_add = 0;
        }
    }
    leader_fish->angle = (leader_fish->angle + leader_fish->ang_add) & 0xFFF;
    if (diff > 1024) {
        leader_fish->angle =
            (leader_fish->angle + (leader_fish->ang_add >> 2)) & 0xFFF;
    }

    diff = (int32_t)leader_fish->speed - (int32_t)leader->speed;
    if (diff < -4) {
        int32_t new_speed =
            (int32_t)leader_fish->speed + (Random_GetControl() & 3) + 1;
        CLAMPL(new_speed, 0);
        leader_fish->speed = new_speed;
    } else if (diff > 4) {
        int32_t new_speed =
            (int32_t)leader_fish->speed - (Random_GetControl() & 3) - 1;
        CLAMPG(new_speed, 255);
        leader_fish->speed = new_speed;
    }

    leader_fish->swim = (leader_fish->swim + (leader_fish->speed >> 4)) & 0x3F;

    const int32_t angle16 = leader_fish->angle << 4;
    int32_t x = leader_fish->pos.x
        - (leader_fish->speed * Math_Sin(angle16) >> (W2V_SHIFT + 1));
    int32_t z = leader_fish->pos.z
        + (leader_fish->speed * Math_Cos(angle16) >> (W2V_SHIFT + 1));

    if (piranha_attack == 0) {
        if (z < -leader->range.z) {
            z = -leader->range.z;
            if (leader_fish->angle < 2048) {
                leader->angle =
                    leader_fish->angle - (Random_GetControl() & 0x7F) - 128;
            } else {
                leader->angle =
                    leader_fish->angle + (Random_GetControl() & 0x7F) + 128;
            }
            leader->angle_time = (Random_GetControl() & 0xF) + 8;
            leader->speed_time = 0;
        } else if (z > leader->range.z) {
            z = leader->range.z;
            if (leader_fish->angle > 3072) {
                leader->angle =
                    leader_fish->angle - (Random_GetControl() & 0x7F) - 128;
            } else {
                leader->angle =
                    leader_fish->angle + (Random_GetControl() & 0x7F) + 128;
            }
            leader->angle_time = (Random_GetControl() & 0xF) + 8;
            leader->speed_time = 0;
        }

        if (x < -leader->range.x) {
            x = -leader->range.x;
            if (leader_fish->angle < 1024) {
                leader->angle =
                    leader_fish->angle - (Random_GetControl() & 0x7F) - 128;
            } else {
                leader->angle =
                    leader_fish->angle + (Random_GetControl() & 0x7F) + 128;
            }
            leader->angle_time = (Random_GetControl() & 0xF) + 8;
            leader->speed_time = 0;
        } else if (x > leader->range.x) {
            x = leader->range.x;
            if (leader_fish->angle < 3072) {
                leader->angle =
                    leader_fish->angle - (Random_GetControl() & 0x7F) - 128;
            } else {
                leader->angle =
                    leader_fish->angle + (Random_GetControl() & 0x7F) + 128;
            }
            leader->angle_time = (Random_GetControl() & 0xF) + 8;
            leader->speed_time = 0;
        }

        if ((Random_GetControl() & 0xF) == 0) {
            leader->angle_time = 0;
        }

        if (leader->angle_time != 0) {
            leader->angle_time--;
        } else {
            leader->angle_time = (Random_GetControl() & 0xF) + 8;
            int32_t delta = (Random_GetControl() & 0x3F) - 24;
            if ((Random_GetControl() & 3) == 0) {
                delta *= 32;
            }
            leader->angle = (leader->angle + delta) & 0xFFF;
        }

        if (leader->speed_time != 0) {
            leader->speed_time--;
        } else {
            leader->speed_time = (Random_GetControl() & 0x1F) + 32;

            if ((Random_GetControl() & 7) == 0) {
                leader->speed = (Random_GetControl() & 0x7F) + 128;
            } else if ((Random_GetControl() & 3) == 0) {
                leader->speed += (Random_GetControl() & 0x7F) + 32;
            } else if (leader->speed > 140) {
                leader->speed += 208 - (Random_GetControl() & 0x1F);
            } else {
                leader->speed_time = (Random_GetControl() & 3) + 4;
                leader->speed += (Random_GetControl() & 0x1F) - 15;
            }
        }
    }

    leader_fish->pos.x = x;
    leader_fish->pos.z = z;

    for (int32_t i = 0; i < M_FISH_PER_SHOAL; i++) {
        M_FISH *const fish = &p->fish[i + 1];

        if (item->object_id == O_PIRAHNAS) {
            const XYZ_32 fish_pos = {
                .x = item->pos.x + fish->pos.x,
                .y = item->pos.y + fish->pos.y,
                .z = item->pos.z + fish->pos.z,
            };
            if (M_FishNearItem(&fish_pos, 256, enemy)) {
                if (p->piranha_hit_wait == 0) {
                    Spawn_Blood(
                        fish_pos.x, fish_pos.y, fish_pos.z, 0, 0,
                        enemy->room_num);
                    p->piranha_hit_wait = 8;
                }

                if (piranha_attack != 2) {
                    Lara_TakeDamage(4, false);
                }
            }
        }

        const int32_t dx = SQUARE(fish->pos.x - x - 128 * i + 3072);
        const int32_t dz = SQUARE(fish->pos.z - z + 128 * i - 3072);

        const uint16_t desired =
            M_GetFishAngle12(fish->pos.x, fish->pos.z, x, z);
        diff = M_GetAngle12Diff(fish->angle, desired);

        if (diff > 128) {
            fish->ang_add -= 4;
            CLAMPL(fish->ang_add, -(i >> 1) - 92);
        } else if (diff < -128) {
            fish->ang_add += 4;
            CLAMPG(fish->ang_add, (i >> 1) + 92);
        } else {
            fish->ang_add -= fish->ang_add >> 2;
            if (ABS(fish->ang_add) < 4) {
                fish->ang_add = 0;
            }
        }

        fish->angle = (fish->angle + fish->ang_add) & 0xFFF;
        if (diff > 1024) {
            fish->angle = (fish->angle + (fish->ang_add >> 2)) & 0xFFF;
        }

        if (dx + dz < 16384 * SQUARE(i) + SQUARE(WALL_L)) {
            if (fish->speed > 2 * i + 32) {
                fish->speed -= fish->speed >> 5;
            }
        } else {
            if (fish->speed < (i >> 1) + 160) {
                fish->speed += (i >> 1) + (Random_GetControl() & 3) + 1;
            }

            if (fish->speed > (i >> 1) - 4 * i + 160) {
                fish->speed = (i >> 1) - 4 * i - 96;
            }
        }

        if ((Random_GetControl() & 1) != 0) {
            fish->speed -= Random_GetControl() & 1;
        } else {
            fish->speed += Random_GetControl() & 1;
        }

        CLAMP(fish->speed, 32, 200);

        fish->swim =
            (fish->swim + (fish->speed >> 4) + (fish->speed >> 5)) & 0x3F;

        const int32_t fish_angle16 = fish->angle << 4;
        int32_t next_x = fish->pos.x
            - (fish->speed * Math_Sin(fish_angle16) >> (W2V_SHIFT + 1));
        int32_t next_z = fish->pos.z
            + (fish->speed * Math_Cos(fish_angle16) >> (W2V_SHIFT + 1));
        CLAMP(next_x, -32000, 32000);
        CLAMP(next_z, -32000, 32000);
        fish->pos.x = next_x;
        fish->pos.z = next_z;

        if (piranha_attack == 0) {
            if (ABS(fish->pos.y - fish->dest_y) < 16) {
                fish->dest_y = Random_GetControl() % leader->range.y;
            }
        } else if (ABS(fish->pos.y - fish->dest_y) < 16 && enemy != nullptr) {
            fish->dest_y =
                (enemy->pos.y - item->pos.y + (Random_GetControl() & 0xFF));
        }

        fish->pos.y += (fish->dest_y - fish->pos.y) >> 4;
    }
}

static bool M_Draw(const ITEM *const item)
{
    if (!item->active) {
        return false;
    }

    if (item->hit_points == NO_ITEM) {
        return false;
    }

    const int32_t leader_num = item->hit_points;
    if (!M_IsValidShoalNum(leader_num)) {
        return false;
    }

    M_PRIV *const p = item->priv;
    if (p == nullptr) {
        return false;
    }

    if (p->leader_num != leader_num) {
        return false;
    }

    if (!p->leader.on) {
        return false;
    }

    const OBJECT *const explosion_obj = Object_Get(O_EXPLOSION_1);
    if (explosion_obj == nullptr || !explosion_obj->loaded) {
        return false;
    }

    int32_t sprite_idx = explosion_obj->mesh_idx;
    if (item->object_id == O_PIRAHNAS) {
        sprite_idx += 10;
    } else {
        sprite_idx += 11;
    }

    if (sprite_idx < 0 || sprite_idx >= Output_GetSpriteTextureCount()) {
        return false;
    }

    const XYZ_32 base_pos = item->interp.result.pos;
    const double ratio = Interpolation_GetWorldRate();
    const bool do_interp =
        Interpolation_IsActive() && ratio > 0.0 && ratio < 1.0;

    M_FISH *fish = &p->fish[1];

    for (int32_t i = 0; i < M_FISH_PER_SHOAL; i++, fish++) {
        if (do_interp) {
            fish->interp.result.pos.x = (int16_t)LERP(
                (int32_t)fish->interp.prev.pos.x, (int32_t)fish->pos.x, ratio);
            fish->interp.result.pos.y = (int16_t)LERP(
                (int32_t)fish->interp.prev.pos.y, (int32_t)fish->pos.y, ratio);
            fish->interp.result.pos.z = (int16_t)LERP(
                (int32_t)fish->interp.prev.pos.z, (int32_t)fish->pos.z, ratio);

            fish->interp.result.angle =
                (Math_AngleMean(
                     fish->interp.prev.angle << 4, fish->angle << 4, ratio)
                 >> 4)
                & 0xFFF;

            int32_t swim_diff =
                (int32_t)fish->swim - (int32_t)fish->interp.prev.swim;
            if (swim_diff > 32) {
                swim_diff -= 64;
            } else if (swim_diff < -32) {
                swim_diff += 64;
            }

            int32_t swim_interp = LERP(
                (int32_t)fish->interp.prev.swim,
                (int32_t)fish->interp.prev.swim + swim_diff, ratio);
            swim_interp %= 64;
            if (swim_interp < 0) {
                swim_interp += 64;
            }
            fish->interp.result.swim = swim_interp;
        } else {
            fish->interp.result.pos = fish->pos;
            fish->interp.result.angle = fish->angle;
            fish->interp.result.swim = fish->swim;
        }

        const int32_t x = base_pos.x + fish->interp.result.pos.x;
        const int32_t y = base_pos.y + fish->interp.result.pos.y;
        const int32_t z = base_pos.z + fish->interp.result.pos.z;

        const int32_t swim_ang16 = fish->interp.result.swim << 10;
        const int32_t swim_wibble = Math_Sin(swim_ang16) >> 7;
        const int32_t ang12 =
            (swim_wibble + fish->interp.result.angle - 2048) & 0xFFF;

        const int32_t size = ((128 * Math_Sin(i << 10)) >> W2V_SHIFT) + 192;
        const int32_t ang16 = ang12 << 4;
        const int32_t back_x = x - ((size * Math_Sin(ang16)) >> W2V_SHIFT);
        const int32_t back_z = z + ((size * Math_Cos(ang16)) >> W2V_SHIFT);

        const XYZ_32 tri_world[3] = {
            { .x = x, .y = y, .z = z },
            { .x = back_x, .y = y - size, .z = back_z },
            { .x = back_x, .y = y + size, .z = back_z },
        };

        int32_t shade = ang12;
        if (shade < 1024) {
            shade -= 512;
        } else if (shade < 2048) {
            shade -= 1536;
        } else if (shade < 3072) {
            shade -= 2560;
        } else {
            shade -= 3584;
        }

        if (shade > 512 || shade < 0) {
            shade = 0;
        } else if (shade < 256) {
            shade >>= 2;
        } else {
            shade = (512 - shade) >> 2;
        }

        shade += i;
        if (shade > 128) {
            shade = 128;
        }

        shade += 80;
        CLAMP(shade, 0, 255);

        const RGBA_8888 color = { shade, shade, shade, 255 };
        const RGBA_8888 tri_color[3] = { color, color, color };

        // OG flips the UV mapping depending on the shoal number (tropical
        // fish) or the fish index (piranhas).
        bool use_default_uv = false;
        if (item->object_id == O_PIRAHNAS) {
            use_default_uv = (i & 1) != 0;
        } else {
            use_default_uv = (leader_num & 1) != 0;
        }

        if (use_default_uv) {
            OutputSource_PolyFX_StageSpriteTriWorld(
                sprite_idx, tri_world, tri_color, DRAW_BLEND);
        } else {
            const int32_t sprite_corners[3] = { 2, 3, 1 }; // u2v2, u1v2, u2v1
            OUTPUT_UVW uvw[3];
            OUTPUT_TEXTURE_SIZE texture_size[3];
            for (int32_t j = 0; j < 3; j++) {
                const int32_t uvw_idx = Output_Textures_GetSpriteUVWIndex(
                    sprite_idx, sprite_corners[j]);
                uvw[j] = Output_Textures_GetUVW(uvw_idx);
                texture_size[j] = Output_Textures_GetAtlasSize(uvw_idx / 4);
            }

            OutputSource_PolyFX_StageTriExtUV(
                tri_world, uvw, texture_size, nullptr, tri_color,
                VERT_NO_LIGHTING | VERT_NO_WIBBLE, DRAW_BLEND);
        }
    }

    if (Interpolation_IsActive() && ratio >= 1.0) {
        for (int32_t i = 0; i < M_FISH_PER_SHOAL + 1; i++) {
            p->fish[i].interp.prev.pos = p->fish[i].pos;
            p->fish[i].interp.prev.angle = p->fish[i].angle;
            p->fish[i].interp.prev.swim = p->fish[i].swim;
        }
    }

    return true;
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->enable_shadow = false;
    item->collidable = false;

    if (item->priv == nullptr) {
        item->priv = GameBuf_Alloc(sizeof(M_PRIV), GBUF_ITEM_DATA);
    }

    M_PRIV *const p = item->priv;
    p->leader.on = false;
    p->leader_num = NO_ITEM;
    p->piranha_hit_wait = 0;
    p->carcass_item_num = NO_ITEM;
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->is_targetable_func = M_IsTargetable;
    obj->draw_func = M_Draw;
    obj->hit_points = NO_ITEM;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
}

void Shoal_TriggerActivate(ITEM *const item, const int16_t trigger_timer)
{
    // TO_OBJECT handling stores (timer & 7) in hit_points and clears timer so
    // it does not count down as a trigger delay.
    const int32_t leader_num = trigger_timer & 7;
    item->hit_points = leader_num;
    item->timer = 0;

    if (M_IsValidShoalNum(leader_num) && item->priv != nullptr) {
        M_PRIV *const p = item->priv;
        p->leader_num = leader_num;
        M_SetupShoal(p, leader_num);
    }
}

void Shoal_TriggerDeactivate(const ITEM *const item)
{
    // Anti-trigger turns the leader off to force a re-setup.
    const int32_t leader_num = item->hit_points;
    if (M_IsValidShoalNum(leader_num) && item->priv != nullptr) {
        M_PRIV *const p = item->priv;
        p->leader.on = false;
    }
}

REGISTER_OBJECT(O_TROPICAL_FISH, M_Setup)
REGISTER_OBJECT(O_PIRAHNAS, M_Setup)
