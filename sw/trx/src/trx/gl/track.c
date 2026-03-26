#include <trx/gl/track.h>

TRX_GL_METRICS g_TRX_GL_Metrics;

void TRX_GL_Track_Reset(void)
{
    g_TRX_GL_Metrics = (TRX_GL_METRICS) { 0 };
}

TRX_GL_METRICS TRX_GL_Track_GetMetrics(void)
{
    return g_TRX_GL_Metrics;
}
