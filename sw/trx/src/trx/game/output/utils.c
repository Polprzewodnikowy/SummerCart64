#include <trx/game/output/utils.h>

#include <trx/game/const.h>

void Output_FillMatrix(GLfloat m[4][4], const MATRIX *const source)
{
    m[0][0] = source->_00 / (float)(1 << W2V_SHIFT);
    m[0][1] = source->_10 / (float)(1 << W2V_SHIFT);
    m[0][2] = source->_20 / (float)(1 << W2V_SHIFT);
    m[0][3] = 0.0;

    m[1][0] = source->_01 / (float)(1 << W2V_SHIFT);
    m[1][1] = source->_11 / (float)(1 << W2V_SHIFT);
    m[1][2] = source->_21 / (float)(1 << W2V_SHIFT);
    m[1][3] = 0.0;

    m[2][0] = source->_02 / (float)(1 << W2V_SHIFT);
    m[2][1] = source->_12 / (float)(1 << W2V_SHIFT);
    m[2][2] = source->_22 / (float)(1 << W2V_SHIFT);
    m[2][3] = 0.0;

    m[3][0] = source->_03 / (float)(1 << W2V_SHIFT);
    m[3][1] = source->_13 / (float)(1 << W2V_SHIFT);
    m[3][2] = source->_23 / (float)(1 << W2V_SHIFT);
    m[3][3] = 1.0;
}
