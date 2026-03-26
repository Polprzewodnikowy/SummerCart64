#include <trx/game/matrix.h>

#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/const.h>

#include <float.h>
#include <math.h>

#define MAX_MATRICES 40
#define MAX_NESTED_MATRICES 32

static MATRIX m_MatrixStack[MAX_MATRICES] = {};
static MATRIX m_WMatrixStack[MAX_MATRICES] = {};
static int32_t m_IMRate = 0;
static int32_t m_IMFrac = 0;
static MATRIX *m_IMMatrixPtr = nullptr;
static MATRIX *m_WIMMatrixPtr = nullptr;
static MATRIX m_IMMatrixStack[MAX_NESTED_MATRICES] = {};
static MATRIX m_WIMMatrixStack[MAX_NESTED_MATRICES] = {};

MATRIX *g_MatrixPtr = &m_MatrixStack[0];
MATRIX *g_WMatrixPtr = &m_WMatrixStack[0];

XYZ_32 g_ViewPos = {};
MATRIX g_ViewMatrix = {};
MATRIX g_IDMatrix = {
    // clang-format off
    ._00 = 1 << W2V_SHIFT, ._01 = 0, ._02 = 0, ._03 = 0,
    ._10 = 0, ._11 = 1 << W2V_SHIFT, ._12 = 0, ._13 = 0,
    ._20 = 0, ._21 = 0, ._22 = 1 << W2V_SHIFT, ._23 = 0,
    // clang-format on
};

static inline void M_QuaternionNormalize(QUATERNION *q)
{
    const double n2 = q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w;
    if (n2 > 0.0) {
        const double inv = 1.0 / sqrt(n2);
        q->x *= inv;
        q->y *= inv;
        q->z *= inv;
        q->w *= inv;
    } else {
        // fallback: identity
        q->x = q->y = q->z = 0.0;
        q->w = 1.0;
    }
}

// One inexpensive polar-decomposition iteration to orthonormalize R (3x3).
// R <- R * (3I - R^T R) / 2  (Newton step toward orthogonal)
// Works great if R is already close to rotation.
static void M_Double3x3Ortho(double r[3][3])
{
    double rt_r[3][3] = {};
    for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
            for (int32_t k = 0; k < 3; k++) {
                rt_r[i][j] += r[k][i] * r[k][j];
            }
        }
    }

    double m[3][3];
    for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
            m[i][j] = 3.0 * (i == j) - rt_r[i][j];
        }
    }

    double rn[3][3] = {};
    for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
            for (int32_t k = 0; k < 3; k++) {
                rn[i][j] += r[i][k] * m[k][j];
            }
        }
    }

    // divide by 2
    for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
            r[i][j] = 0.5 * rn[i][j];
        }
    }
}

// Extract 3x3 rotation (in doubles) from fixed-point MATRIX.
static void M_Double3x3FromMatrix(const MATRIX *const m, double e[3][3])
{
    const double s = (1 << W2V_SHIFT);
    e[0][0] = m->_00 / s;
    e[0][1] = m->_01 / s;
    e[0][2] = m->_02 / s;
    e[1][0] = m->_10 / s;
    e[1][1] = m->_11 / s;
    e[1][2] = m->_12 / s;
    e[2][0] = m->_20 / s;
    e[2][1] = m->_21 / s;
    e[2][2] = m->_22 / s;
}

// Remove uniform scale if present (estimate from row lengths).
// Use average row length to reduce noise.
static void M_Double3x3RemoveScale(double e[3][3])
{
    const double rl0 =
        sqrt(e[0][0] * e[0][0] + e[0][1] * e[0][1] + e[0][2] * e[0][2]);
    const double rl1 =
        sqrt(e[1][0] * e[1][0] + e[1][1] * e[1][1] + e[1][2] * e[1][2]);
    const double rl2 =
        sqrt(e[2][0] * e[2][0] + e[2][1] * e[2][1] + e[2][2] * e[2][2]);
    const double scale = (rl0 + rl1 + rl2) / 3.0;
    if (scale <= 0.0) {
        return;
    }
    const double inv = 1.0 / scale;
    for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
            e[i][j] *= inv;
        }
    }
}

// Write 3x3 back to MATRIX as fixed-point, with rounding.
static void M_Double3x3ToMatrix(const double e[3][3], MATRIX *m)
{
    const double s = (double)(1 << W2V_SHIFT);
    m->_00 = (int32_t)llround(e[0][0] * s);
    m->_01 = (int32_t)llround(e[0][1] * s);
    m->_02 = (int32_t)llround(e[0][2] * s);
    m->_10 = (int32_t)llround(e[1][0] * s);
    m->_11 = (int32_t)llround(e[1][1] * s);
    m->_12 = (int32_t)llround(e[1][2] * s);
    m->_20 = (int32_t)llround(e[2][0] * s);
    m->_21 = (int32_t)llround(e[2][1] * s);
    m->_22 = (int32_t)llround(e[2][2] * s);
}

static void M_MatrixToQuaternion(const MATRIX *m, QUATERNION *q)
{
    double e[3][3];
    M_Double3x3FromMatrix(m, e);
    M_Double3x3RemoveScale(e);
    // Orthonormalize (fast, one Newton step is usually enough).
    M_Double3x3Ortho(e);

    const double tr = e[0][0] + e[1][1] + e[2][2];
    if (tr > 0.0) {
        const double s = sqrt(tr + 1.0) * 2.0; // 4*w
        q->w = 0.25 * s;
        q->x = (e[2][1] - e[1][2]) / s;
        q->y = (e[0][2] - e[2][0]) / s;
        q->z = (e[1][0] - e[0][1]) / s;
    } else {
        // Pick the biggest diagonal for numerical stability
        int32_t i = 0;
        if (e[1][1] > e[0][0]) {
            i = 1;
        }
        if (e[2][2] > e[i][i]) {
            i = 2;
        }
        const int32_t j = (i + 1) % 3;
        const int32_t k = (i + 2) % 3;
        const double s = sqrt(e[i][i] - e[j][j] - e[k][k] + 1.0) * 2.0;
        double qv[3];
        qv[i] = 0.25 * s;
        qv[j] = (e[j][i] + e[i][j]) / s;
        qv[k] = (e[k][i] + e[i][k]) / s;
        q->x = qv[0];
        q->y = qv[1];
        q->z = qv[2];
        q->w = (e[k][j] - e[j][k]) / s;
    }
    M_QuaternionNormalize(q);
}

static void M_MatrixFromQuaternion(const QUATERNION *const qin, MATRIX *const m)
{
    QUATERNION q = *qin;
    M_QuaternionNormalize(&q);

    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    double e[3][3];
    e[0][0] = 1.0 - 2.0 * (yy + zz);
    e[0][1] = 2.0 * (xy - wz);
    e[0][2] = 2.0 * (xz + wy);
    e[1][0] = 2.0 * (xy + wz);
    e[1][1] = 1.0 - 2.0 * (xx + zz);
    e[1][2] = 2.0 * (yz - wx);
    e[2][0] = 2.0 * (xz - wy);
    e[2][1] = 2.0 * (yz + wx);
    e[2][2] = 1.0 - 2.0 * (xx + yy);

    // Optional: one more orthonormalization step to crush rounding noise
    M_Double3x3Ortho(e);
    M_Double3x3ToMatrix(e, m);
}

static void M_QuaternionSlerp(
    const QUATERNION *const qa, const QUATERNION *const qb, const double t,
    QUATERNION *const out)
{
    QUATERNION a = *qa, b = *qb;
    M_QuaternionNormalize(&a);
    M_QuaternionNormalize(&b);

    double cosom = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (cosom < 0.0) { // take shortest path
        cosom = -cosom;
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }

    // Guard acos input, and use nlerp for tiny angles
    CLAMP(cosom, -1.0, 1.0);

    // Threshold tuned for double precision
    const double EPS = 1e-12;
    double scale0;
    double scale1;
    if (1.0 - cosom > EPS) {
        const double omega = acos(cosom);
        const double sinom = 1.0 / sin(omega);
        scale0 = sin((1.0 - t) * omega) * sinom;
        scale1 = sin(t * omega) * sinom;
    } else {
        // Nearly parallel: nlerp, then normalize
        scale0 = 1.0 - t;
        scale1 = t;
    }

    out->x = scale0 * a.x + scale1 * b.x;
    out->y = scale0 * a.y + scale1 * b.y;
    out->z = scale0 * a.z + scale1 * b.z;
    out->w = scale0 * a.w + scale1 * b.w;
    M_QuaternionNormalize(out);
}

static void M_ScaleX(MATRIX *const m, const int32_t scale)
{
    m->_00 = ((int64_t)m->_00 * scale) >> W2V_SHIFT;
    m->_10 = ((int64_t)m->_10 * scale) >> W2V_SHIFT;
    m->_20 = ((int64_t)m->_20 * scale) >> W2V_SHIFT;
}

static void M_ScaleY(MATRIX *const m, const int32_t scale)
{
    m->_01 = ((int64_t)m->_01 * scale) >> W2V_SHIFT;
    m->_11 = ((int64_t)m->_11 * scale) >> W2V_SHIFT;
    m->_21 = ((int64_t)m->_21 * scale) >> W2V_SHIFT;
}

static void M_ScaleZ(MATRIX *const m, const int32_t angle)
{
    m->_02 = ((int64_t)m->_02 * angle) >> W2V_SHIFT;
    m->_12 = ((int64_t)m->_12 * angle) >> W2V_SHIFT;
    m->_22 = ((int64_t)m->_22 * angle) >> W2V_SHIFT;
}

static void M_RotX(MATRIX *const m, const int16_t angle)
{
    if (angle == 0) {
        return;
    }
    const int32_t sx = Math_Sin(angle);
    const int32_t cx = Math_Cos(angle);

    int32_t r0, r1;
    r0 = m->_01 * cx + m->_02 * sx;
    r1 = m->_02 * cx - m->_01 * sx;
    m->_01 = r0 >> W2V_SHIFT;
    m->_02 = r1 >> W2V_SHIFT;

    r0 = m->_11 * cx + m->_12 * sx;
    r1 = m->_12 * cx - m->_11 * sx;
    m->_11 = r0 >> W2V_SHIFT;
    m->_12 = r1 >> W2V_SHIFT;

    r0 = m->_21 * cx + m->_22 * sx;
    r1 = m->_22 * cx - m->_21 * sx;
    m->_21 = r0 >> W2V_SHIFT;
    m->_22 = r1 >> W2V_SHIFT;
}

static void M_RotY(MATRIX *const m, const int16_t angle)
{
    if (angle == 0) {
        return;
    }

    const int32_t sy = Math_Sin(angle);
    const int32_t cy = Math_Cos(angle);

    int32_t r0, r1;
    r0 = m->_00 * cy - m->_02 * sy;
    r1 = m->_02 * cy + m->_00 * sy;
    m->_00 = r0 >> W2V_SHIFT;
    m->_02 = r1 >> W2V_SHIFT;

    r0 = m->_10 * cy - m->_12 * sy;
    r1 = m->_12 * cy + m->_10 * sy;
    m->_10 = r0 >> W2V_SHIFT;
    m->_12 = r1 >> W2V_SHIFT;

    r0 = m->_20 * cy - m->_22 * sy;
    r1 = m->_22 * cy + m->_20 * sy;
    m->_20 = r0 >> W2V_SHIFT;
    m->_22 = r1 >> W2V_SHIFT;
}

static void M_RotZ(MATRIX *const m, const int16_t angle)
{
    if (angle == 0) {
        return;
    }

    const int32_t sz = Math_Sin(angle);
    const int32_t cz = Math_Cos(angle);

    int32_t r0, r1;
    r0 = m->_00 * cz + m->_01 * sz;
    r1 = m->_01 * cz - m->_00 * sz;
    m->_00 = r0 >> W2V_SHIFT;
    m->_01 = r1 >> W2V_SHIFT;

    r0 = m->_10 * cz + m->_11 * sz;
    r1 = m->_11 * cz - m->_10 * sz;
    m->_10 = r0 >> W2V_SHIFT;
    m->_11 = r1 >> W2V_SHIFT;

    r0 = m->_20 * cz + m->_21 * sz;
    r1 = m->_21 * cz - m->_20 * sz;
    m->_20 = r0 >> W2V_SHIFT;
    m->_21 = r1 >> W2V_SHIFT;
}

static void M_RotYXZ(MATRIX *const m, const XYZ_16 rotation)
{
    M_RotY(m, rotation.y);
    M_RotX(m, rotation.x);
    M_RotZ(m, rotation.z);
}

static void M_TranslateRel(MATRIX *const m, const XYZ_32 offset)
{
    m->_03 += offset.x * m->_00 + offset.y * m->_01 + offset.z * m->_02;
    m->_13 += offset.x * m->_10 + offset.y * m->_11 + offset.z * m->_12;
    m->_23 += offset.x * m->_20 + offset.y * m->_21 + offset.z * m->_22;
}

static void M_TranslateSet(MATRIX *const m, const XYZ_32 pos)
{
    const int64_t scale = (int64_t)(1 << W2V_SHIFT);
    m->_03 = (int64_t)pos.x * scale;
    m->_13 = (int64_t)pos.y * scale;
    m->_23 = (int64_t)pos.z * scale;
}

void Matrix_Mul3x3_M(
    MATRIX *const out, const MATRIX *const lhs, const MATRIX *const rhs)
{
#define L_MUL(r_, c_)                                                          \
    (((lhs->_##r_##0 * rhs->_0##c_) + (lhs->_##r_##1 * rhs->_1##c_)            \
      + (lhs->_##r_##2 * rhs->_2##c_))                                         \
     >> W2V_SHIFT)

    const int64_t r00 = L_MUL(0, 0);
    const int64_t r01 = L_MUL(0, 1);
    const int64_t r02 = L_MUL(0, 2);
    const int64_t r10 = L_MUL(1, 0);
    const int64_t r11 = L_MUL(1, 1);
    const int64_t r12 = L_MUL(1, 2);
    const int64_t r20 = L_MUL(2, 0);
    const int64_t r21 = L_MUL(2, 1);
    const int64_t r22 = L_MUL(2, 2);

    out->_00 = r00;
    out->_01 = r01;
    out->_02 = r02;
    out->_10 = r10;
    out->_11 = r11;
    out->_12 = r12;
    out->_20 = r20;
    out->_21 = r21;
    out->_22 = r22;

#undef L_MUL
}

static void M_InterpolateArm(MATRIX *const m, const MATRIX *const mi)
{
    m->_00 = m[-2]._00;
    m->_01 = m[-2]._01;
    m->_02 = m[-2]._02;
    m->_03 += ((mi->_03 - m->_03) * m_IMFrac) / m_IMRate;
    m->_10 = m[-2]._10;
    m->_11 = m[-2]._11;
    m->_12 = m[-2]._12;
    m->_13 += ((mi->_13 - m->_13) * m_IMFrac) / m_IMRate;
    m->_20 = m[-2]._20;
    m->_21 = m[-2]._21;
    m->_22 = m[-2]._22;
    m->_23 += ((mi->_23 - m->_23) * m_IMFrac) / m_IMRate;
}

static void M_Interpolate(
    const MATRIX *const m1, const MATRIX *const m2, MATRIX *const result)
{
    double rate = (m_IMRate != 0) ? ((double)m_IMFrac / (double)m_IMRate) : 0.0;
    CLAMP(rate, 0.0, 1.0);

    QUATERNION q1, q2, q;
    M_MatrixToQuaternion(m1, &q1);
    M_MatrixToQuaternion(m2, &q2);
    M_QuaternionSlerp(&q1, &q2, rate, &q);
    M_MatrixFromQuaternion(&q, result);

    result->_03 = (int32_t)llround(m1->_03 + (m2->_03 - m1->_03) * rate);
    result->_13 = (int32_t)llround(m1->_13 + (m2->_13 - m1->_13) * rate);
    result->_23 = (int32_t)llround(m1->_23 + (m2->_23 - m1->_23) * rate);
}

void Matrix_ResetStack(void)
{
    g_MatrixPtr = &m_MatrixStack[0];
    g_WMatrixPtr = &m_WMatrixStack[0];
}

void Matrix_GenerateW2V(const XYZ_32 *pos, const XYZ_16 *rot)
{
    const int32_t sx = Math_Sin(rot->x);
    const int32_t cx = Math_Cos(rot->x);
    const int32_t sy = Math_Sin(rot->y);
    const int32_t cy = Math_Cos(rot->y);
    const int32_t sz = Math_Sin(rot->z);
    const int32_t cz = Math_Cos(rot->z);

    g_ViewPos = *pos;
    g_ViewMatrix._00 = TRIGMULT3(sx, sy, sz) + TRIGMULT2(cy, cz);
    g_ViewMatrix._01 = TRIGMULT2(cx, sz);
    g_ViewMatrix._02 = TRIGMULT3(sx, cy, sz) - TRIGMULT2(sy, cz);
    g_ViewMatrix._10 = TRIGMULT3(sx, sy, cz) - TRIGMULT2(cy, sz);
    g_ViewMatrix._11 = TRIGMULT2(cx, cz);
    g_ViewMatrix._12 = TRIGMULT3(sx, cy, cz) + TRIGMULT2(sy, sz);
    g_ViewMatrix._20 = TRIGMULT2(cx, sy);
    g_ViewMatrix._21 = -sx;
    g_ViewMatrix._22 = TRIGMULT2(cx, cy);
    g_ViewMatrix._03 = 0;
    g_ViewMatrix._13 = 0;
    g_ViewMatrix._23 = 0;
    M_TranslateRel(&g_ViewMatrix, (XYZ_32) { -pos->x, -pos->y, -pos->z });

    g_MatrixPtr = &m_MatrixStack[0];
    m_MatrixStack[0] = g_ViewMatrix;

    g_WMatrixPtr = &m_WMatrixStack[0];
    g_WMatrixPtr[0] = g_IDMatrix;
}

bool Matrix_Push(void)
{
    if (g_MatrixPtr + 1 - m_MatrixStack >= MAX_MATRICES) {
        return false;
    }
    if (g_WMatrixPtr + 1 - m_WMatrixStack >= MAX_MATRICES) {
        return false;
    }
    g_MatrixPtr++;
    g_MatrixPtr[0] = g_MatrixPtr[-1];
    g_WMatrixPtr++;
    g_WMatrixPtr[0] = g_WMatrixPtr[-1];
    return true;
}

bool Matrix_PushUnit(void)
{
    if (g_MatrixPtr + 1 - m_MatrixStack >= MAX_MATRICES) {
        return false;
    }
    if (g_WMatrixPtr + 1 - m_WMatrixStack >= MAX_MATRICES) {
        return false;
    }
    g_MatrixPtr++;
    *g_MatrixPtr = g_IDMatrix;
    g_WMatrixPtr++;
    *g_WMatrixPtr = g_IDMatrix;
    return true;
}

void Matrix_Pop(void)
{
    g_MatrixPtr--;
    g_WMatrixPtr--;
}

void Matrix_Scale(const int32_t scale)
{
    Matrix_ScaleX(scale);
    Matrix_ScaleY(scale);
    Matrix_ScaleZ(scale);
}

void Matrix_ScaleX(const int32_t scale)
{
    M_ScaleX(g_MatrixPtr, scale);
    M_ScaleX(g_WMatrixPtr, scale);
}

void Matrix_ScaleY(const int32_t scale)
{
    M_ScaleY(g_MatrixPtr, scale);
    M_ScaleY(g_WMatrixPtr, scale);
}

void Matrix_ScaleZ(const int32_t scale)
{
    M_ScaleZ(g_MatrixPtr, scale);
    M_ScaleZ(g_WMatrixPtr, scale);
}

void Matrix_RotX(const int16_t angle)
{
    M_RotX(g_MatrixPtr, angle);
    M_RotX(g_WMatrixPtr, angle);
}

void Matrix_RotY(const int16_t angle)
{
    M_RotY(g_MatrixPtr, angle);
    M_RotY(g_WMatrixPtr, angle);
}

void Matrix_RotZ(const int16_t angle)
{
    M_RotZ(g_MatrixPtr, angle);
    M_RotZ(g_WMatrixPtr, angle);
}

void Matrix_Rot16(const XYZ_16 rotation)
{
    M_RotYXZ(g_MatrixPtr, rotation);
    M_RotYXZ(g_WMatrixPtr, rotation);
}

void Matrix_RotX_M(MATRIX *const m, const int16_t angle)
{
    M_RotX(m, angle);
}

void Matrix_RotY_M(MATRIX *const m, const int16_t angle)
{
    M_RotY(m, angle);
}

void Matrix_RotZ_M(MATRIX *const m, const int16_t angle)
{
    M_RotZ(m, angle);
}

void Matrix_Slerp3x3_M(
    MATRIX *const lhs_out, const MATRIX *const rhs, const double t)
{
    QUATERNION q1, q2, q;
    M_MatrixToQuaternion(lhs_out, &q1);
    M_MatrixToQuaternion(rhs, &q2);

    double clamped_t = t;
    CLAMP(clamped_t, 0.0, 1.0);
    M_QuaternionSlerp(&q1, &q2, clamped_t, &q);
    M_MatrixFromQuaternion(&q, lhs_out);
}

void Matrix_Mul3x3(const MATRIX *const rhs)
{
    Matrix_Mul3x3_M(g_MatrixPtr, g_MatrixPtr, rhs);
    Matrix_Mul3x3_M(g_WMatrixPtr, g_WMatrixPtr, rhs);
}

void Matrix_TranslateRel(const int32_t dx, const int32_t dy, const int32_t dz)
{
    Matrix_TranslateRel32((XYZ_32) { dx, dy, dz });
}

void Matrix_TranslateRel16(const XYZ_16 offset)
{
    Matrix_TranslateRel32(XYZ_32_From16(offset));
}

void Matrix_TranslateRel32(const XYZ_32 offset)
{
    M_TranslateRel(g_MatrixPtr, offset);
    M_TranslateRel(g_WMatrixPtr, offset);
}

void Matrix_TranslateAbs(const int32_t x, const int32_t y, const int32_t z)
{
    MATRIX *const m = g_MatrixPtr;
    const MATRIX *const v = &g_ViewMatrix;
    m->_03 = x * v->_00 + y * v->_01 + z * v->_02 + v->_03;
    m->_13 = x * v->_10 + y * v->_11 + z * v->_12 + v->_13;
    m->_23 = x * v->_20 + y * v->_21 + z * v->_22 + v->_23;
    M_TranslateSet(g_WMatrixPtr, (XYZ_32) { x, y, z });
}

void Matrix_TranslateAbs16(const XYZ_16 offset)
{
    Matrix_TranslateAbs(offset.x, offset.y, offset.z);
}

void Matrix_TranslateAbs32(const XYZ_32 offset)
{
    Matrix_TranslateAbs(offset.x, offset.y, offset.z);
}

void Matrix_TranslateSet32(const XYZ_32 origin)
{
    M_TranslateSet(g_MatrixPtr, origin);
    M_TranslateSet(g_WMatrixPtr, origin);
}

void Matrix_TranslateSet32_M(MATRIX *const m, const XYZ_32 origin)
{
    M_TranslateSet(m, origin);
}

void Matrix_InitInterpolate(const int32_t frac, const int32_t rate)
{
    m_IMFrac = frac;
    m_IMRate = rate;
    m_IMMatrixPtr = &m_IMMatrixStack[0];
    *m_IMMatrixPtr = *g_MatrixPtr;
    m_WIMMatrixPtr = &m_WIMMatrixStack[0];
    *m_WIMMatrixPtr = *g_WMatrixPtr;
}

void Matrix_Interpolate(void)
{
    M_Interpolate(g_MatrixPtr, m_IMMatrixPtr, g_MatrixPtr);
    M_Interpolate(g_WMatrixPtr, m_WIMMatrixPtr, g_WMatrixPtr);
}

void Matrix_InterpolateArm(void)
{
    M_InterpolateArm(g_MatrixPtr, m_IMMatrixPtr);
    M_InterpolateArm(g_WMatrixPtr, m_WIMMatrixPtr);
}

void Matrix_Push_I(void)
{
    Matrix_Push();
    m_IMMatrixPtr[1] = m_IMMatrixPtr[0];
    m_IMMatrixPtr++;
    m_WIMMatrixPtr[1] = m_WIMMatrixPtr[0];
    m_WIMMatrixPtr++;
}

void Matrix_Pop_I(void)
{
    Matrix_Pop();
    m_IMMatrixPtr--;
    m_WIMMatrixPtr--;
}

void Matrix_TranslateRel_I(const int32_t x, const int32_t y, const int32_t z)
{
    Matrix_TranslateRel32_I((XYZ_32) { x, y, z });
}

void Matrix_TranslateRel16_I(const XYZ_16 offset)
{
    Matrix_TranslateRel32_I(XYZ_32_From16(offset));
}

void Matrix_TranslateRel32_I(const XYZ_32 offset)
{
    M_TranslateRel(g_MatrixPtr, offset);
    M_TranslateRel(g_WMatrixPtr, offset);
    M_TranslateRel(m_IMMatrixPtr, offset);
    M_TranslateRel(m_WIMMatrixPtr, offset);
}

void Matrix_TranslateRel_ID(
    const int32_t x, const int32_t y, const int32_t z, const int32_t x2,
    const int32_t y2, const int32_t z2)
{
    Matrix_TranslateRel32_ID((XYZ_32) { x, y, z }, (XYZ_32) { x2, y2, z2 });
}

void Matrix_TranslateRel16_ID(const XYZ_16 offset_1, const XYZ_16 offset_2)
{
    Matrix_TranslateRel32_ID(XYZ_32_From16(offset_1), XYZ_32_From16(offset_2));
}

void Matrix_TranslateRel32_ID(const XYZ_32 offset_1, const XYZ_32 offset_2)
{
    M_TranslateRel(g_MatrixPtr, offset_1);
    M_TranslateRel(g_WMatrixPtr, offset_1);
    M_TranslateRel(m_IMMatrixPtr, offset_2);
    M_TranslateRel(m_WIMMatrixPtr, offset_2);
}

void Matrix_RotY_I(const int16_t angle)
{
    M_RotY(g_MatrixPtr, angle);
    M_RotY(g_WMatrixPtr, angle);
    M_RotY(m_IMMatrixPtr, angle);
    M_RotY(m_WIMMatrixPtr, angle);
}

void Matrix_RotX_I(const int16_t angle)
{
    M_RotX(g_MatrixPtr, angle);
    M_RotX(g_WMatrixPtr, angle);
    M_RotX(m_IMMatrixPtr, angle);
    M_RotX(m_WIMMatrixPtr, angle);
}

void Matrix_RotZ_I(const int16_t angle)
{
    M_RotZ(g_MatrixPtr, angle);
    M_RotZ(g_WMatrixPtr, angle);
    M_RotZ(m_IMMatrixPtr, angle);
    M_RotZ(m_WIMMatrixPtr, angle);
}

void Matrix_Rot16_I(const XYZ_16 rotation)
{
    M_RotYXZ(g_MatrixPtr, rotation);
    M_RotYXZ(g_WMatrixPtr, rotation);
    M_RotYXZ(m_IMMatrixPtr, rotation);
    M_RotYXZ(m_WIMMatrixPtr, rotation);
}

void Matrix_Rot16_ID(const XYZ_16 rotation_1, const XYZ_16 rotation_2)
{
    M_RotYXZ(g_MatrixPtr, rotation_1);
    M_RotYXZ(g_WMatrixPtr, rotation_1);
    M_RotYXZ(m_IMMatrixPtr, rotation_2);
    M_RotYXZ(m_WIMMatrixPtr, rotation_2);
}

void Matrix_LookAt(
    const int32_t source_x, const int32_t source_y, const int32_t source_z,
    const int32_t target_x, const int32_t target_y, const int32_t target_z,
    const int16_t roll)
{
    int16_t angles[2];
    Math_GetVectorAngles(
        target_x - source_x, target_y - source_y, target_z - source_z, angles);

    const XYZ_32 view_pos = {
        .x = source_x,
        .y = source_y,
        .z = source_z,
    };
    const XYZ_16 view_rot = {
        .x = angles[1],
        .y = angles[0],
        .z = roll,
    };
    Matrix_GenerateW2V(&view_pos, &view_rot);
}

XYZ_32 Matrix_MulVec32_M(const MATRIX *const m, const XYZ_32 v)
{
    return (XYZ_32) {
        (m->_00 * v.x + m->_01 * v.y + m->_02 * v.z + m->_03) >> W2V_SHIFT,
        (m->_10 * v.x + m->_11 * v.y + m->_12 * v.z + m->_13) >> W2V_SHIFT,
        (m->_20 * v.x + m->_21 * v.y + m->_22 * v.z + m->_23) >> W2V_SHIFT,
    };
}

XYZ_32 Matrix_GetOffset_M(const MATRIX *const m)
{
    return (XYZ_32) {
        .x = m->_03 >> W2V_SHIFT,
        .y = m->_13 >> W2V_SHIFT,
        .z = m->_23 >> W2V_SHIFT,
    };
}
