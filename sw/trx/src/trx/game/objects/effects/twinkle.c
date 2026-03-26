#include <trx/game/objects/effects/twinkle.h>

#include <trx/core/utils.h>
#include <trx/game/collision.h>
#include <trx/game/const.h>
#include <trx/game/effects.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/spawn.h>

#define M_DISAPPEAR_RANGE STEP_L

static void M_SpawnTwinkle(const GAME_VECTOR *const pos)
{
    const int16_t effect_num = Effect_Create(pos->room_num);
    if (effect_num != NO_EFFECT) {
        EFFECT *const effect = Effect_Get(effect_num);
        effect->pos = pos->pos;
        effect->counter = Object_Get(O_TWINKLE)->mesh_count;
        effect->object_id = O_TWINKLE;
        effect->frame_num = 0;
    }
}

static XYZ_32 M_GetTargetPos(const ITEM *const item)
{
    XYZ_32 pos = item->pos;
    if (item->object_id == O_DRAGON_FRONT) {
        const int32_t c = Math_Cos(item->rot.y);
        const int32_t s = Math_Sin(item->rot.y);
        pos.x += (c * 1100 + s * 490) >> W2V_SHIFT;
        pos.z += (c * 490 - s * 1100) >> W2V_SHIFT;
        pos.y -= 540;
    }
    return pos;
}

static void M_NudgeTowardsItem(
    EFFECT *const effect, const XYZ_32 *const target_pos)
{
    effect->pos.x += (target_pos->x - effect->pos.x) >> 4;
    effect->pos.y += (target_pos->y - effect->pos.y) >> 4;
    effect->pos.z += (target_pos->z - effect->pos.z) >> 4;
}

static bool M_ShouldDisappear(
    const EFFECT *const effect, const XYZ_32 *const target_pos)
{
    const int32_t dx = ABS(effect->pos.x - target_pos->x);
    const int32_t dy = ABS(effect->pos.y - target_pos->y);
    const int32_t dz = ABS(effect->pos.z - target_pos->z);
    return dx < M_DISAPPEAR_RANGE && dy < M_DISAPPEAR_RANGE
        && dz < M_DISAPPEAR_RANGE;
}

static void M_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    const OBJECT *const obj = Object_Get(effect->object_id);

    effect->frame_num--;
    if (effect->frame_num <= obj->mesh_count) {
        effect->frame_num = 0;
    }

    if (effect->counter < 0) {
        effect->counter++;
        if (effect->counter == 0) {
            Effect_Kill(effect_num);
        }
        return;
    }

    const ITEM *const item = Item_Get(effect->counter);
    const XYZ_32 target_pos = M_GetTargetPos(item);
    M_NudgeTowardsItem(effect, &target_pos);
    if (M_ShouldDisappear(effect, &target_pos)) {
        Effect_Kill(effect_num);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
}

void Twinkle_SparkleItem(const ITEM *const item, const uint32_t mesh_mask)
{
    SPHERE spheres[34];
    const int32_t num_spheres = Collide_GetSpheres(item, spheres, true);

    GAME_VECTOR effect_pos = {
        .pos = {},
        .room_num = item->room_num,
    };

    for (int32_t i = 0; i < num_spheres; i++) {
        if ((mesh_mask & (1 << i)) == 0) {
            continue;
        }
        const SPHERE *const sphere = &spheres[i];
        effect_pos.x =
            sphere->pos.x + sphere->r * (Random_GetDraw() - 0x4000) / 0x4000;
        effect_pos.y =
            sphere->pos.y + sphere->r * (Random_GetDraw() - 0x4000) / 0x4000;
        effect_pos.z =
            sphere->pos.z + sphere->r * (Random_GetDraw() - 0x4000) / 0x4000;
        M_SpawnTwinkle(&effect_pos);
    }
}

REGISTER_OBJECT(O_TWINKLE, M_Setup)
