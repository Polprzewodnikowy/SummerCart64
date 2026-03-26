#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/math.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/game/interpolation.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/output/textures.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>

#define M_MAX_BATS 32
#define M_BAT_SPRITE_OFFSET 12

typedef struct {
    XYZ_32 pos;
    XYZ_32 prev_pos;
    int16_t angle;
    int16_t prev_angle;
    int16_t speed;
    uint8_t wing_y_off;
    uint8_t prev_wing_y_off;
    bool active;
    uint8_t life;
} M_BAT;

typedef struct {
    bool bats_triggered;
    bool bats_alive;
    M_BAT bats[M_MAX_BATS];

    struct {
        bool prepared;
        int32_t sprite_idx;
        OUTPUT_UVW tri_uvw[3][3];
        OUTPUT_TEXTURE_SIZE tri_tex_size[3][3];
    } draw;
} M_PRIV;

static const XYZ_16 m_BatMesh[5] = {
    { -192, 0, -48 },  { -192, 0, 48 },  { 96, 0, 0 },
    { -144, 0, -192 }, { -144, 0, 192 },
};

static const uint8_t m_BatTriangles[3][3] = {
    { 0, 1, 2 },
    { 3, 0, 2 },
    { 1, 4, 2 },
};

// TR3 UV mapping differs per triangle in the original bat GT3 setup.
static const uint8_t m_BatTriangleSpriteCorners[3][3] = {
    { 0, 2, 3 },
    { 1, 0, 2 },
    { 0, 1, 2 },
};

static int32_t M_GetWingYOffset(const int32_t corner, const uint8_t wing_y_off)
{
    if (corner < 3) {
        const int16_t angle = (((wing_y_off - 32) & 0x3F) << 10);
        return (Math_Sin(angle) >> 10) - 512;
    }

    const int16_t angle = wing_y_off << 10;
    return (Math_Sin(angle) >> 6) - 512;
}

static void M_RememberBat(M_BAT *const bat)
{
    bat->prev_pos = bat->pos;
    bat->prev_angle = bat->angle;
    bat->prev_wing_y_off = bat->wing_y_off;
}

static uint8_t M_GetInterpolatedWingYOffset(
    const M_BAT *const bat, const double ratio)
{
    int32_t wing_diff =
        (int32_t)bat->wing_y_off - (int32_t)bat->prev_wing_y_off;
    if (wing_diff > 32) {
        wing_diff -= 64;
    } else if (wing_diff < -32) {
        wing_diff += 64;
    }

    int32_t wing_interp = LERP(
        (int32_t)bat->prev_wing_y_off,
        (int32_t)bat->prev_wing_y_off + wing_diff, ratio);
    wing_interp %= 64;
    if (wing_interp < 0) {
        wing_interp += 64;
    }
    return (uint8_t)wing_interp;
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "bats_triggered", &p->bats_triggered));
    JSON_SHOULD(JSON_READ(io, "bats_alive", &p->bats_alive));

    for (int32_t i = 0; i < M_MAX_BATS; i++) {
        p->bats[i] = (M_BAT) {};
    }

    if (p->bats_alive && JSON_SHOULD(JSON_PUSH(io, "bats"))) {
        for (int32_t i = 0; i < M_MAX_BATS; i++) {
            const char *const key = String_FormatStatic("bat_%d", i);
            if (JSON_SHOULD(JSON_PUSH(io, key))) {
                M_BAT *const bat = &p->bats[i];
                JSON_SHOULD(JSON_READ(io, "pos", &bat->pos));
                JSON_SHOULD(JSON_READ(io, "angle", &bat->angle));
                JSON_SHOULD(JSON_READ(io, "speed", &bat->speed));
                JSON_SHOULD(JSON_READ(io, "wing_y_off", &bat->wing_y_off));
                JSON_SHOULD(JSON_READ(io, "active", &bat->active));
                JSON_SHOULD(JSON_READ(io, "life", &bat->life));
                JSON_SHOULD(JSON_POP(io));
            }
        }
        JSON_SHOULD(JSON_POP(io));
    }

    p->draw.prepared = false;
    p->draw.sprite_idx = -1;
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "bats_triggered", p->bats_triggered);
    JSONW_WRITE(io, "bats_alive", p->bats_alive);

    if (!p->bats_alive) {
        return;
    }

    JSONW_PUSH_OBJECT(io);
    for (int32_t i = 0; i < M_MAX_BATS; i++) {
        const M_BAT *const bat = &p->bats[i];
        const char *const key = String_FormatStatic("bat_%d", i);
        JSONW_PUSH_OBJECT(io);
        JSONW_WRITE(io, "pos", bat->pos);
        JSONW_WRITE(io, "angle", bat->angle);
        JSONW_WRITE(io, "speed", bat->speed);
        JSONW_WRITE(io, "wing_y_off", bat->wing_y_off);
        JSONW_WRITE(io, "active", bat->active);
        JSONW_WRITE(io, "life", bat->life);
        JSONW_POP_AND_SET(io, key);
    }
    JSONW_POP_AND_SET(io, "bats");
}

static void M_PrepareDrawData(M_PRIV *const p)
{
    if (p->draw.prepared) {
        return;
    }

    p->draw.sprite_idx = -1;

    const OBJECT *const explosion = Object_Get(O_EXPLOSION_1);
    if (explosion == nullptr || !explosion->loaded) {
        return;
    }

    const int32_t sprite_idx = explosion->mesh_idx + M_BAT_SPRITE_OFFSET;
    if (sprite_idx < 0 || sprite_idx >= Output_GetSpriteTextureCount()) {
        return;
    }

    p->draw.sprite_idx = sprite_idx;

    for (size_t i = 0; i < ARRAY_SIZE(m_BatTriangleSpriteCorners); i++) {
        for (size_t j = 0; j < ARRAY_SIZE(m_BatTriangleSpriteCorners[0]); j++) {
            const int32_t uvw_idx = Output_Textures_GetSpriteUVWIndex(
                sprite_idx, m_BatTriangleSpriteCorners[i][j]);
            p->draw.tri_uvw[i][j] = Output_Textures_GetUVW(uvw_idx);
            p->draw.tri_tex_size[i][j] =
                Output_Textures_GetAtlasSize(uvw_idx / 4);
        }
    }

    p->draw.prepared = true;
}

static bool M_Draw(const ITEM *const item)
{
    M_PRIV *const p = item->priv;
    if (!p->bats_alive) {
        return false;
    }

    if (!p->draw.prepared) {
        M_PrepareDrawData(p);
    }

    if (p->draw.sprite_idx < 0
        || p->draw.sprite_idx >= Output_GetSpriteTextureCount()) {
        return false;
    }

    const RGBA_8888 color = { 0x60, 0xA0, 0xF8, 0xFF };
    const RGBA_8888 tri_color[3] = { color, color, color };
    const double ratio = Interpolation_GetWorldRate();
    const bool do_interp =
        Interpolation_IsActive() && ratio > 0.0 && ratio < 1.0;

    for (int32_t i = 0; i < M_MAX_BATS; i++) {
        const M_BAT *const bat = &p->bats[i];
        if (!bat->active) {
            continue;
        }

        const XYZ_32 draw_pos = do_interp
            ? (XYZ_32) {
                  .x = (int32_t)LERP(bat->prev_pos.x, bat->pos.x, ratio),
                  .y = (int32_t)LERP(bat->prev_pos.y, bat->pos.y, ratio),
                  .z = (int32_t)LERP(bat->prev_pos.z, bat->pos.z, ratio),
              }
            : bat->pos;
        const int16_t draw_angle = do_interp
            ? (Math_AngleMean(bat->prev_angle << 4, bat->angle << 4, ratio)
               >> 4)
            : bat->angle;
        const uint8_t draw_wing_y_off = do_interp
            ? M_GetInterpolatedWingYOffset(bat, ratio)
            : bat->wing_y_off;

        XYZ_32 world[5] = {};
        Matrix_Push();
        Matrix_TranslateAbs32(draw_pos);
        Matrix_RotY(draw_angle << 4);
        for (int32_t j = 0; j < 5; j++) {
            const XYZ_32 local = {
                .x = m_BatMesh[j].x,
                .y = m_BatMesh[j].y + M_GetWingYOffset(j, draw_wing_y_off),
                .z = m_BatMesh[j].z,
            };
            world[j] = Matrix_MulVec32_M(g_WMatrixPtr, local);
        }
        Matrix_Pop();

        for (size_t j = 0; j < ARRAY_SIZE(m_BatTriangles); j++) {
            const uint8_t *const tri = m_BatTriangles[j];
            const XYZ_32 tri_world[3] = {
                world[tri[0]],
                world[tri[1]],
                world[tri[2]],
            };

            OutputSource_PolyFX_StageTriExtUV(
                tri_world, p->draw.tri_uvw[j], p->draw.tri_tex_size[j], nullptr,
                tri_color, VERT_NO_LIGHTING | VERT_NO_WIBBLE, DRAW_BLEND);
        }
    }

    return true;
}

static void M_Update(M_PRIV *const p)
{
    bool any_alive = false;

    for (int32_t i = 0; i < M_MAX_BATS; i++) {
        M_BAT *const bat = &p->bats[i];
        if (!bat->active) {
            continue;
        }

        M_RememberBat(bat);

        if ((i & 3) == 0 && (Random_GetControl() & 7) == 0) {
            Sound_Effect(SFX_BATS_1, &bat->pos, SPM_NORMAL);
        }

        const int16_t angle = bat->angle << 4;
        const int32_t sin_v = Math_Sin(angle) >> 2;
        const int32_t cos_v = Math_Cos(angle) >> 2;
        bat->pos.x -= ((int64_t)bat->speed * cos_v) >> W2V_SHIFT;
        bat->pos.y -= Random_GetControl() & 3;
        bat->pos.z += ((int64_t)bat->speed * sin_v) >> W2V_SHIFT;
        bat->wing_y_off = (bat->wing_y_off + 11) & 0x3F;

        if (bat->life < 128) {
            bat->pos.y += -4 - (i >> 1);

            if ((Random_GetControl() & 3) == 0) {
                bat->angle =
                    (bat->angle + (Random_GetControl() & 0xFF) - 128) & 0xFFF;
                bat->speed += Random_GetControl() & 3;
            }
        }

        bat->speed += 12;
        CLAMPG(bat->speed, 300);

        const int32_t time4 = Output_GetTimeInGame() * 4;
        if (bat->life != 0 && (time4 & 4) != 0) {
            bat->life--;
            if (bat->life == 0) {
                bat->active = false;
            }
        }

        if (bat->active) {
            any_alive = true;
        }
    }

    p->bats_alive = any_alive;
}

static void M_TriggerBats(M_PRIV *const p, const XYZ_32 pos, int16_t ang)
{
    ang = (ang - 1024) & 0xFFF;

    for (int32_t i = 0; i < M_MAX_BATS; i++) {
        M_BAT *const bat = &p->bats[i];
        bat->pos.x = pos.x + (Random_GetControl() & 0x1FF) - 256;
        bat->pos.y = pos.y - (Random_GetControl() & 0xFF) + 256;
        bat->pos.z = pos.z + (Random_GetControl() & 0x1FF) - 256;
        bat->angle = ((Random_GetControl() & 0x7F) + ang - 64) & 0xFFF;
        bat->speed = (Random_GetControl() & 0x1F) + 64;
        bat->wing_y_off = Random_GetControl() & 0x3F;
        bat->life = (Random_GetControl() & 7) + 144;
        bat->active = true;
        M_RememberBat(bat);
    }

    p->bats_alive = true;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (item->active == 0) {
        return;
    }

    M_PRIV *const p = item->priv;
    if (!p->bats_triggered) {
        M_TriggerBats(p, item->pos, item->rot.y >> 4);
        p->bats_triggered = true;
    } else {
        M_Update(p);
    }

    if (!p->bats_alive) {
        Item_Kill(item_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_BAT_EMITTER, M_Setup)
