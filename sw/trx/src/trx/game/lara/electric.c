#include <trx/game/lara/electric.h>

#include <trx/core/utils.h>
#include <trx/game/collision.h>
#include <trx/game/lara.h>
#include <trx/game/output/lights.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/random.h>

static const uint8_t m_LaraMeshes[28] = {
    0, 1, 1, 2, 2, 3,  0, 4,  4,  5,  5,  6,  0, 7,
    7, 8, 8, 9, 9, 10, 7, 11, 11, 12, 12, 13, 7, 14,
};
static const uint8_t m_LaraLastPoints[14] = {
    0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1,
};
static const uint8_t m_LaraLineCounts[6] = { 12, 12, 4, 12, 12, 4 };

typedef struct {
    XYZ_16 pos;
    XYZ_16 vel;
} M_ELECTRIC_POINT;

static M_ELECTRIC_POINT m_ElectricityPoints[32] = {};

XYZ_16 Lara_Electricity_GetPoint(const int32_t idx)
{
    return m_ElectricityPoints[idx & 31].pos;
}

void Lara_Electricity_UpdatePoints(void)
{
    for (int32_t i = 0; i < 32; i++) {
        const int32_t rnd = Random_GetDraw();
        int16_t x = m_ElectricityPoints[i].pos.x;
        int16_t y = m_ElectricityPoints[i].pos.y;
        int16_t z = m_ElectricityPoints[i].pos.z;
        int16_t xv = m_ElectricityPoints[i].vel.x;
        int16_t yv = m_ElectricityPoints[i].vel.y;
        int16_t zv = m_ElectricityPoints[i].vel.z;

        if (((x > 256 || x < -256) && (y > 256 || y < -256)
             && (z > 256 || z < -256))
            || x > 384 || x < -128 || y > 384 || y < -128 || z > 384
            || z < -128) {
            x = 0;
            y = 0;
            z = 0;
            xv = 0;
            yv = 0;
            zv = 0;
        }

        if (xv != 0) {
            if (xv >= 0) {
                xv += 2;
            } else {
                xv -= 2;
            }
        } else if ((rnd & 1) != 0) {
            xv = -1 - (Random_GetDraw() & 3);
        } else {
            xv = (Random_GetDraw() & 3) + 1;
        }

        if (yv != 0) {
            if (yv >= 0) {
                yv += 2;
            } else {
                yv -= 2;
            }
        } else if ((rnd & 2) != 0) {
            yv = -1 - (Random_GetDraw() & 3);
        } else {
            yv = (Random_GetDraw() & 3) + 1;
        }

        if (zv != 0) {
            if (zv >= 0) {
                zv++;
            } else {
                zv--;
            }
        } else if ((rnd & 4) != 0) {
            zv = -1 - (Random_GetDraw() & 3);
        } else {
            zv = (Random_GetDraw() & 3) + 1;
        }

        x += xv;
        y += yv;
        z += zv;

        m_ElectricityPoints[i].pos.x = x;
        m_ElectricityPoints[i].pos.y = y;
        m_ElectricityPoints[i].pos.z = z;
        m_ElectricityPoints[i].vel.x = xv;
        m_ElectricityPoints[i].vel.y = yv;
        m_ElectricityPoints[i].vel.z = zv;
    }
}

void Lara_Electricity_EmitLight(void)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->electric == 0) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t electric = lara->electric;

    int32_t r = 0;
    int32_t g = 0;
    int32_t b = 0;
    int32_t falloff = 0;

    if (electric < 12) {
        r = (Random_GetControl() & 7) - electric + 16;
        g = 32 - electric;
        b = 255;
        falloff = (Random_GetControl() & 1) - 2 * electric + 25;
        r <<= 3;
        g <<= 3;
    } else {
        r = 0;
        g = (Random_GetControl() & 0x3F) + 64;
        b = (Random_GetControl() & 0x3F) + 128;
        falloff = (Random_GetControl() & 3) + 8;
    }

    CLAMP(r, 0, 255);
    CLAMP(g, 0, 255);
    CLAMP(b, 0, 255);
    CLAMP(falloff, 0, 255);

    Output_AddDynamicLightRGB(lara_item->pos, falloff, (RGB_888) { r, g, b });
}

void Lara_Electricity_Draw(const int32_t lr, const ITEM *const item)
{
    XYZ_32 pos[96];
    int16_t dists[96];
    int32_t num = 0;

    for (int32_t i = 0; i < 14; i++) {
        const int32_t mesh1 = m_LaraMeshes[2 * i];
        const int32_t mesh2 = m_LaraMeshes[2 * i + 1];
        const M_ELECTRIC_POINT *const points =
            &m_ElectricityPoints[(5 * i) & 0xF];
        int32_t point_idx = 0;

        XYZ_32 pos1 = { .x = 0, .y = 0, .z = 0 };
        XYZ_32 pos2 = { .x = 0, .y = 0, .z = 0 };

        if (lr != 0) {
            pos1.x = -48;
            pos1.z = -48;
        } else {
            pos1.x = 48;
            pos1.z = 48;
        }

        Collide_GetJointAbsPosition(item, &pos1, mesh1);

        if (m_LaraLastPoints[i] == 0 || i == 13) {
            if (lr != 0) {
                pos2.x = -48;
                pos2.z = -48;
            } else {
                pos2.x = 48;
                pos2.z = 48;
            }

            if (i == 13) {
                pos2.y = -64;
            }
        }

        Collide_GetJointAbsPosition(item, &pos2, mesh2);

        int32_t x = pos1.x;
        int32_t y = pos1.y;
        int32_t z = pos1.z;
        const int32_t x_step = (pos2.x - pos1.x) >> 2;
        const int32_t y_step = (pos2.y - pos1.y) >> 2;
        const int32_t z_step = (pos2.z - pos1.z) >> 2;

        for (int32_t j = 0; j < 5; j++) {
            if (j == 4 && m_LaraLastPoints[i] == 0) {
                break;
            }

            int32_t mx = x;
            int32_t my = y;
            int32_t mz = z;
            if (j == 4 && m_LaraLastPoints[i] != 0) {
                mx = pos2.x;
                my = pos2.y;
                mz = pos2.z;
            }

            if (j == 0 || j == 4) {
                dists[num] = 0;
            } else {
                const XYZ_16 point = points[point_idx].pos;
                point_idx++;

                if (lr != 0) {
                    mx -= point.x >> 3;
                    my -= point.y >> 3;
                    mz -= point.z >> 3;
                } else {
                    mx += point.x >> 3;
                    my += point.y >> 3;
                    mz += point.z >> 3;
                }

                int32_t p_x = ABS(point.x);
                int32_t p_y = ABS(point.y);
                int32_t p_z = ABS(point.z);
                if (p_y > p_x) {
                    p_x = p_y;
                }
                if (p_z > p_x) {
                    p_x = p_z;
                }
                dists[num] = (int16_t)p_x;
            }

            pos[num].x = mx;
            pos[num].y = my;
            pos[num].z = mz;
            num++;

            x += x_step;
            y += y_step;
            z += z_step;
        }
    }

    int32_t idx = 0;
    for (int32_t i = 0; i < 6; i++) {
        for (int32_t j = 0; j < m_LaraLineCounts[i]; j++) {
            const XYZ_32 from = pos[idx];
            const XYZ_32 to = pos[idx + 1];
            int32_t c0 = dists[idx];
            int32_t c1 = dists[idx + 1];
            idx++;

            if (c0 > 255) {
                c0 = 511 - c0;
                if (c0 < 0) {
                    c0 = 0;
                }
            }

            if (c1 > 255) {
                c1 = 511 - c1;
                if (c1 < 0) {
                    c1 = 0;
                }
            }

            if (lr != 0) {
                c0 >>= 1;
                c1 >>= 1;
            }

            const RGBA_8888 from_color = { 0, (uint8_t)c0, (uint8_t)c0, 0xC0 };
            const RGBA_8888 to_color = { 0, (uint8_t)c1, (uint8_t)c1, 0xC0 };
            OutputSource_PolyFX_StageLineSegment(
                from, from_color, to, to_color, 1.0f, DRAW_BLEND_ADD);
        }
        idx++;
    }
}
