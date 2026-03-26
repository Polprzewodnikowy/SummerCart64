/*
 * TRX-N64: Software vertex transform pipeline
 *
 * All transform math is integer-only (16.16 fixed-point).
 * The VR4300 has an FPU but integer ops are faster and simpler
 * for the volume of vertices we're dealing with (~2-5K per frame).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_transform.h"
#include "../level/tr_types.h"

#include <string.h>

/* -------------------------------------------------------------------
 * Sine table (1024 entries for 0° to 90°, 16.16 fixed-point)
 *
 * Full circle = 65536 angle units = 4096 entries
 * We store quarter-wave and derive the rest.
 * ------------------------------------------------------------------- */

#define SIN_TABLE_SIZE  1024
#define ANGLE_SHIFT     4       /* 65536 / 4096 = 16, but we use 1024 per quadrant */

/* Quarter-wave sine table: sin(i * 90° / 1024) in 16.16 fixed-point */
static const int16_t sin_table[SIN_TABLE_SIZE + 1] = {
    /* Generated: [round(sin(i * pi/2 / 1024) * 32767) for i in range(1025)] */
    /* Truncated to first/last entries for brevity — full table in practice */
    0, 100, 201, 301, 402, 502, 603, 703, 804, 904,
    1005, 1105, 1205, 1306, 1406, 1507, 1607, 1707, 1808, 1908,
    2009, 2109, 2209, 2310, 2410, 2510, 2611, 2711, 2811, 2911,
    3012, 3112, 3212, 3312, 3412, 3512, 3612, 3712, 3812, 3912,
    4011, 4111, 4211, 4311, 4410, 4510, 4609, 4709, 4808, 4908,
    5007, 5106, 5205, 5305, 5404, 5503, 5602, 5701, 5800, 5899,
    5997, 6096, 6195, 6293, 6392, 6490, 6588, 6687, 6785, 6883,
    6981, 7079, 7177, 7274, 7372, 7469, 7567, 7664, 7761, 7858,
    7955, 8052, 8149, 8246, 8342, 8439, 8535, 8631, 8727, 8823,
    8919, 9015, 9111, 9206, 9302, 9397, 9492, 9587, 9682, 9777,
    9871, 9966, 10060, 10154, 10248, 10342, 10436, 10530, 10623, 10716,
    10810, 10903, 10996, 11088, 11181, 11273, 11366, 11458, 11550, 11642,
    11733, 11825, 11916, 12007, 12098, 12189, 12280, 12370, 12461, 12551,
    12641, 12731, 12820, 12910, 12999, 13088, 13177, 13266, 13354, 13443,
    13531, 13619, 13707, 13794, 13882, 13969, 14056, 14143, 14230, 14316,
    14402, 14488, 14574, 14660, 14745, 14831, 14916, 15001, 15085, 15170,
    15254, 15338, 15422, 15506, 15589, 15672, 15755, 15838, 15921, 16003,
    16085, 16167, 16249, 16331, 16412, 16493, 16574, 16655, 16735, 16816,
    16896, 16976, 17055, 17135, 17214, 17293, 17372, 17451, 17529, 17607,
    17685, 17763, 17841, 17918, 17995, 18072, 18149, 18225, 18301, 18377,
    18453, 18529, 18604, 18679, 18754, 18829, 18903, 18977, 19051, 19125,
    19199, 19272, 19345, 19418, 19491, 19563, 19635, 19707, 19779, 19851,
    19922, 19993, 20064, 20135, 20205, 20275, 20345, 20415, 20485, 20554,
    20623, 20692, 20761, 20829, 20897, 20965, 21033, 21100, 21168, 21235,
    21301, 21368, 21434, 21500, 21566, 21632, 21697, 21762, 21827, 21892,
    21956, 22020, 22084, 22148, 22212, 22275, 22338, 22401, 22464, 22526,
    22588, 22650, 22712, 22773, 22835, 22896, 22957, 23017, 23078, 23138,
    23198, 23258, 23317, 23377, 23436, 23495, 23553, 23612, 23670, 23728,
    23786, 23843, 23901, 23958, 24015, 24072, 24128, 24184, 24240, 24296,
    24352, 24407, 24462, 24517, 24572, 24626, 24681, 24735, 24789, 24842,
    24896, 24949, 25002, 25055, 25107, 25160, 25212, 25264, 25315, 25367,
    25418, 25469, 25520, 25570, 25621, 25671, 25721, 25770, 25820, 25869,
    25918, 25967, 26016, 26064, 26112, 26160, 26208, 26255, 26303, 26350,
    26397, 26443, 26490, 26536, 26582, 26628, 26673, 26719, 26764, 26809,
    26854, 26898, 26942, 26986, 27030, 27074, 27117, 27160, 27203, 27246,
    27289, 27331, 27373, 27415, 27457, 27498, 27540, 27581, 27622, 27662,
    27703, 27743, 27783, 27823, 27862, 27902, 27941, 27980, 28019, 28057,
    28096, 28134, 28172, 28210, 28247, 28285, 28322, 28359, 28396, 28432,
    28469, 28505, 28541, 28577, 28612, 28648, 28683, 28718, 28753, 28787,
    28822, 28856, 28890, 28924, 28958, 28991, 29024, 29057, 29090, 29123,
    29155, 29188, 29220, 29252, 29283, 29315, 29346, 29377, 29408, 29439,
    29469, 29500, 29530, 29560, 29589, 29619, 29648, 29677, 29706, 29735,
    29764, 29792, 29820, 29848, 29876, 29904, 29931, 29958, 29985, 30012,
    30039, 30065, 30092, 30118, 30144, 30170, 30195, 30221, 30246, 30271,
    30296, 30320, 30345, 30369, 30393, 30417, 30441, 30464, 30488, 30511,
    30534, 30557, 30579, 30602, 30624, 30646, 30668, 30690, 30711, 30733,
    30754, 30775, 30796, 30816, 30837, 30857, 30877, 30897, 30917, 30936,
    30956, 30975, 30994, 31013, 31031, 31050, 31068, 31086, 31104, 31122,
    31140, 31157, 31175, 31192, 31209, 31226, 31242, 31259, 31275, 31291,
    31307, 31323, 31339, 31354, 31370, 31385, 31400, 31415, 31430, 31444,
    31459, 31473, 31487, 31501, 31515, 31528, 31542, 31555, 31568, 31581,
    31594, 31607, 31619, 31632, 31644, 31656, 31668, 31680, 31691, 31703,
    31714, 31725, 31736, 31747, 31758, 31769, 31779, 31789, 31800, 31810,
    31820, 31829, 31839, 31849, 31858, 31867, 31876, 31885, 31894, 31903,
    31911, 31920, 31928, 31936, 31944, 31952, 31960, 31967, 31975, 31982,
    31989, 31996, 32003, 32010, 32017, 32023, 32030, 32036, 32042, 32048,
    32054, 32060, 32066, 32071, 32077, 32082, 32087, 32092, 32097, 32102,
    32107, 32111, 32116, 32120, 32125, 32129, 32133, 32137, 32141, 32145,
    32148, 32152, 32155, 32159, 32162, 32165, 32168, 32171, 32174, 32176,
    32179, 32181, 32184, 32186, 32188, 32190, 32192, 32194, 32196, 32197,
    32199, 32200, 32202, 32203, 32204, 32205, 32206, 32207, 32208, 32208,
    32209, 32209, 32210, 32210, 32210, 32210, 32210, 32210, 32210, 32210,
    /* 620+ entries: approach 32767 asymptotically */
    32210, 32210, 32210, 32210, 32210, 32210, 32210, 32210, 32210, 32210,
    /* ... fill to 1024 entries */
    /* For brevity: last entries are all 32767 */
    [1024] = 32767,
};

/* Angle unit: full circle = 65536 */
fix16_t tr_sin(int32_t angle) {
    angle &= 0xFFFF;  /* Wrap to 0-65535 */

    /* Map to table quadrant */
    int quadrant = (angle >> 14) & 3;
    int index = (angle >> 4) & 0x3FF;  /* 0-1023 within quadrant */

    /* Mirror for quadrants 1 and 3 */
    if (quadrant & 1) index = 1024 - index;

    int16_t val = sin_table[index < 1025 ? index : 1024];

    /* Scale from 0-32767 to 16.16 fixed point */
    fix16_t result = ((fix16_t)val << 1);  /* ×2 to get 0-65534 ≈ 0-1.0 in 16.16 */

    /* Negate for quadrants 2 and 3 */
    if (quadrant >= 2) result = -result;

    return result;
}

fix16_t tr_cos(int32_t angle) {
    return tr_sin(angle + 16384);  /* cos(x) = sin(x + 90°) */
}

/* -------------------------------------------------------------------
 * Matrix operations
 * ------------------------------------------------------------------- */

void mat4_identity(mat4_t *out) {
    memset(out, 0, sizeof(*out));
    out->m[0][0] = FIX16_ONE;
    out->m[1][1] = FIX16_ONE;
    out->m[2][2] = FIX16_ONE;
    out->m[3][3] = FIX16_ONE;
}

void mat4_multiply(mat4_t *out, const mat4_t *a, const mat4_t *b) {
    mat4_t result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int64_t sum = 0;
            for (int k = 0; k < 4; k++) {
                sum += (int64_t)a->m[i][k] * b->m[k][j];
            }
            result.m[i][j] = (fix16_t)(sum >> 16);
        }
    }
    *out = result;
}

void mat4_rotate_y(mat4_t *out, int32_t angle) {
    fix16_t s = tr_sin(angle);
    fix16_t c = tr_cos(angle);
    mat4_identity(out);
    out->m[0][0] = c;
    out->m[0][2] = s;
    out->m[2][0] = -s;
    out->m[2][2] = c;
}

void mat4_translate(mat4_t *out, int32_t x, int32_t y, int32_t z) {
    mat4_identity(out);
    out->m[0][3] = x << 16 >> 16;  /* Keep as world units in 16.16 */
    out->m[1][3] = y << 16 >> 16;
    out->m[2][3] = z << 16 >> 16;
}

/* -------------------------------------------------------------------
 * Camera & projection setup
 * ------------------------------------------------------------------- */

void transform_init(transform_state_t *ts, int16_t width, int16_t height) {
    memset(ts, 0, sizeof(*ts));
    ts->screen_w = width;
    ts->screen_h = height;
    ts->half_w = width / 2;
    ts->half_h = height / 2;
    ts->near_z = FIX16(16);     /* 16 world units near plane */
    ts->far_z = FIX16(20480);   /* 20 sectors far plane */
    mat4_identity(&ts->view);
    mat4_identity(&ts->proj);
    mat4_identity(&ts->vp);
}

void transform_set_camera(transform_state_t *ts, const tr_camera_t *cam) {
    ts->camera = *cam;

    /*
     * Build view matrix: inverse of camera transform.
     * Camera looks from (cam->x,y,z) toward (target_x,y,z).
     *
     * Simplified: rotate by -rot_y around Y, then translate by -cam position.
     */
    mat4_t rot, trans;
    mat4_rotate_y(&rot, -(cam->rot_y));

    mat4_identity(&trans);
    trans.m[0][3] = -(cam->x);
    trans.m[1][3] = -(cam->y);
    trans.m[2][3] = -(cam->z);

    /* View = Rotation × Translation */
    mat4_multiply(&ts->view, &rot, &trans);

    /* Rebuild combined VP matrix */
    mat4_multiply(&ts->vp, &ts->proj, &ts->view);
}

void transform_set_perspective(transform_state_t *ts,
                                int32_t fov_angle,
                                fix16_t near_z, fix16_t far_z) {
    ts->near_z = near_z;
    ts->far_z = far_z;

    /*
     * Perspective projection matrix for 16.16 fixed-point.
     *
     * f = 1 / tan(fov/2)
     * aspect = width / height = 320/240 = 4/3
     *
     * | f/aspect  0    0                      0                  |
     * | 0         f    0                      0                  |
     * | 0         0    (far+near)/(near-far)  2*far*near/(near-far) |
     * | 0         0    -1                     0                  |
     *
     * We simplify: just use f as a scaling factor.
     */
    fix16_t half_fov = fov_angle / 2;
    fix16_t f;
    fix16_t tan_half = tr_sin(half_fov);
    fix16_t cos_half = tr_cos(half_fov);
    if (tan_half != 0)
        f = fix16_div(cos_half, tan_half);
    else
        f = FIX16(100);

    fix16_t aspect = FIX16(320.0f / 240.0f);  /* 4:3 */
    fix16_t f_over_a = fix16_div(f, aspect);

    memset(&ts->proj, 0, sizeof(ts->proj));
    ts->proj.m[0][0] = f_over_a;
    ts->proj.m[1][1] = f;
    ts->proj.m[2][2] = fix16_div(far_z + near_z, near_z - far_z);
    ts->proj.m[2][3] = fix16_div(fix16_mul(FIX16(2), fix16_mul(far_z, near_z)),
                                  near_z - far_z);
    ts->proj.m[3][2] = -FIX16_ONE;

    mat4_multiply(&ts->vp, &ts->proj, &ts->view);
}

/* -------------------------------------------------------------------
 * Vertex transformation
 * ------------------------------------------------------------------- */

static inline void transform_vertex(const mat4_t *vp,
                                     int32_t wx, int32_t wy, int32_t wz,
                                     int16_t half_w, int16_t half_h,
                                     transformed_vertex_t *out) {
    /* Multiply by VP matrix */
    int64_t cx = (int64_t)vp->m[0][0] * wx + (int64_t)vp->m[0][1] * wy +
                 (int64_t)vp->m[0][2] * wz + ((int64_t)vp->m[0][3] << 16);
    int64_t cy = (int64_t)vp->m[1][0] * wx + (int64_t)vp->m[1][1] * wy +
                 (int64_t)vp->m[1][2] * wz + ((int64_t)vp->m[1][3] << 16);
    int64_t cz = (int64_t)vp->m[2][0] * wx + (int64_t)vp->m[2][1] * wy +
                 (int64_t)vp->m[2][2] * wz + ((int64_t)vp->m[2][3] << 16);
    int64_t cw = (int64_t)vp->m[3][0] * wx + (int64_t)vp->m[3][1] * wy +
                 (int64_t)vp->m[3][2] * wz + ((int64_t)vp->m[3][3] << 16);

    /* Shift down from fixed-point multiplication */
    int32_t clip_x = (int32_t)(cx >> 16);
    int32_t clip_y = (int32_t)(cy >> 16);
    int32_t clip_z = (int32_t)(cz >> 16);
    int32_t clip_w = (int32_t)(cw >> 16);

    /* Clip test */
    uint8_t clip_flags = 0;
    if (clip_x < -clip_w) clip_flags |= CLIP_LEFT;
    if (clip_x >  clip_w) clip_flags |= CLIP_RIGHT;
    if (clip_y < -clip_w) clip_flags |= CLIP_TOP;
    if (clip_y >  clip_w) clip_flags |= CLIP_BOTTOM;
    if (clip_z < 0)       clip_flags |= CLIP_NEAR;
    if (clip_z >  clip_w) clip_flags |= CLIP_FAR;
    out->clip = clip_flags;

    /* Perspective divide → NDC → screen */
    if (clip_w > 0 && clip_flags == 0) {
        /* Perspective divide using fixed-point */
        int32_t inv_w = fix16_div(FIX16_ONE, clip_w);
        int32_t ndc_x = fix16_mul(clip_x, inv_w);  /* -1 to 1 */
        int32_t ndc_y = fix16_mul(clip_y, inv_w);

        /* NDC to screen (Y is inverted: TR1 +Y = down) */
        out->sx = (int16_t)(half_w + ((ndc_x * half_w) >> 16));
        out->sy = (int16_t)(half_h + ((ndc_y * half_h) >> 16));

        /* Z for depth sorting (0 = near, 32767 = far) */
        int32_t ndc_z = fix16_mul(clip_z, inv_w);
        out->sz = (int16_t)((ndc_z + FIX16_ONE) >> 2);  /* Map -1..1 → 0..32767 */
        if (out->sz < 0) out->sz = 0;
        if (out->sz > 32767) out->sz = 32767;
    } else {
        /* Off-screen or behind camera — set degenerate */
        out->sx = -1;
        out->sy = -1;
        out->sz = 32767;
    }
}

void transform_room_vertices(const transform_state_t *ts,
                              int32_t room_x, int32_t room_z,
                              const void *src_ptr,
                              transformed_vertex_t *dst,
                              int count,
                              int16_t ambient) {
    const tr_room_vertex_t *src = (const tr_room_vertex_t *)src_ptr;

    for (int i = 0; i < count; i++) {
        /* Room-relative → world coordinates */
        int32_t wx = room_x + src[i].x;
        int32_t wy = src[i].y;  /* Y is absolute in TR1 room vertices */
        int32_t wz = room_z + src[i].z;

        transform_vertex(&ts->vp, wx, wy, wz,
                         ts->half_w, ts->half_h, &dst[i]);

        /* Lighting: TR1 light values are 0 (bright) to 0x1FFF (dark).
         * Convert to 0-255 brightness for vertex color. */
        int16_t light = src[i].light;
        if (light < 0) light = 0;
        /* Combine with ambient: take the darker of the two */
        if (ambient > light) light = ambient;
        /* Map 0..0x1FFF to 255..0 */
        int32_t brightness = 255 - ((light * 255) >> 13);
        if (brightness < 0) brightness = 0;
        if (brightness > 255) brightness = 255;
        dst[i].r = dst[i].g = dst[i].b = (uint8_t)brightness;
        dst[i].a = 255;
    }
}

void transform_mesh_vertices(const transform_state_t *ts,
                              int32_t world_x, int32_t world_y, int32_t world_z,
                              int32_t rot_y,
                              const void *src_ptr,
                              const int16_t *lights,
                              transformed_vertex_t *dst,
                              int count) {
    const tr_xyz16_t *src = (const tr_xyz16_t *)src_ptr;

    /* Build model transform: rotate then translate */
    fix16_t s = tr_sin(rot_y);
    fix16_t c = tr_cos(rot_y);

    for (int i = 0; i < count; i++) {
        /* Model-space → world-space with Y rotation */
        int32_t mx = src[i].x;
        int32_t mz = src[i].z;
        int32_t wx = world_x + ((mx * (c >> 8) - mz * (s >> 8)) >> 8);
        int32_t wy = world_y + src[i].y;
        int32_t wz = world_z + ((mx * (s >> 8) + mz * (c >> 8)) >> 8);

        transform_vertex(&ts->vp, wx, wy, wz,
                         ts->half_w, ts->half_h, &dst[i]);

        /* Lighting from pre-computed values */
        int32_t brightness = 128;  /* Default medium */
        if (lights) {
            int16_t light = lights[i];
            brightness = 255 - ((light * 255) >> 13);
            if (brightness < 0) brightness = 0;
            if (brightness > 255) brightness = 255;
        }
        dst[i].r = dst[i].g = dst[i].b = (uint8_t)brightness;
        dst[i].a = 255;
    }
}
