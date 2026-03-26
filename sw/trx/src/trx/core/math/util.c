#include <trx/core/math/util.h>

bool Bounds32_Intersect(const BOUNDS_32 *const a, const BOUNDS_32 *const b)
{
    return !(
        a->min.x > b->max.x || a->max.x < b->min.x || a->min.y > b->max.y
        || a->max.y < b->min.y || a->min.z > b->max.z || a->max.z < b->min.z);
}
