#include <trx/game/objects/draw.h>

#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/inventory.h>
#include <trx/game/items/col.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/output/vars.h>
#include <trx/version.h>

static BOUNDS_16 M_GetBoundingBox(
    const OBJECT *const obj, const ANIM_FRAME *const frame,
    const uint32_t mesh_bits)
{
    const XYZ_16 *const mesh_rots =
        frame != nullptr ? frame->mesh_rots : nullptr;

    Matrix_PushUnit();
    if (frame != nullptr) {
        Matrix_TranslateRel16(frame->offset);
    }
    if (mesh_rots != nullptr) {
        Matrix_Rot16(mesh_rots[0]);
    }

    BOUNDS_16 new_bounds = {
        .min.x = 0x7FFF,
        .min.y = 0x7FFF,
        .min.z = 0x7FFF,
        .max.x = -0x7FFF,
        .max.y = -0x7FFF,
        .max.z = -0x7FFF,
    };

    for (int32_t mesh_idx = 0; mesh_idx < obj->mesh_count; mesh_idx++) {
        if (mesh_idx != 0) {
            const ANIM_BONE *const bone = Object_GetBone(obj, mesh_idx - 1);
            if (bone->matrix_pop) {
                Matrix_Pop();
            }

            if (bone->matrix_push) {
                Matrix_Push();
            }

            Matrix_TranslateRel32(bone->pos);
            if (mesh_rots != nullptr) {
                Matrix_Rot16(mesh_rots[mesh_idx]);
            }
        }

        if (!(mesh_bits & (1 << mesh_idx))) {
            continue;
        }

        const OBJECT_MESH *const mesh =
            Object_GetMesh(obj->mesh_idx + mesh_idx);
        for (int32_t i = 0; i < mesh->num_vertices; i++) {
            // clang-format off
            const XYZ_16 *const vertex = &mesh->vertices[i];
            const MATRIX *const mptr = g_MatrixPtr;
            const double xv = (
                mptr->_00 * vertex->x +
                mptr->_01 * vertex->y +
                mptr->_02 * vertex->z +
                mptr->_03
            );
            const double yv = (
                mptr->_10 * vertex->x +
                mptr->_11 * vertex->y +
                mptr->_12 * vertex->z +
                mptr->_13
            );
            double zv = (
                mptr->_20 * vertex->x +
                mptr->_21 * vertex->y +
                mptr->_22 * vertex->z +
                mptr->_23
            );
            // clang-format on

            const int32_t x = ((int32_t)xv) >> W2V_SHIFT;
            const int32_t y = ((int32_t)yv) >> W2V_SHIFT;
            const int32_t z = ((int32_t)zv) >> W2V_SHIFT;

            new_bounds.min.x = MIN(new_bounds.min.x, x);
            new_bounds.min.y = MIN(new_bounds.min.y, y);
            new_bounds.min.z = MIN(new_bounds.min.z, z);
            new_bounds.max.x = MAX(new_bounds.max.x, x);
            new_bounds.max.y = MAX(new_bounds.max.y, y);
            new_bounds.max.z = MAX(new_bounds.max.z, z);
        }
    }

    Matrix_Pop();
    return new_bounds;
}

bool Object_DrawUnclippedItem(const ITEM *const item)
{
    const int32_t left = g_PhdLeft;
    const int32_t top = g_PhdTop;
    const int32_t right = g_PhdRight;
    const int32_t bottom = g_PhdBottom;

    g_PhdLeft = Viewport_GetMinX(VIEWPORT_GAME);
    g_PhdTop = Viewport_GetMinY(VIEWPORT_GAME);
    g_PhdRight = Viewport_GetMaxX(VIEWPORT_GAME);
    g_PhdBottom = Viewport_GetMaxY(VIEWPORT_GAME);

    Object_DrawAnimatingItem(item);

    g_PhdLeft = left;
    g_PhdTop = top;
    g_PhdRight = right;
    g_PhdBottom = bottom;
    return true;
}

void Object_DrawMesh(
    const int32_t mesh_idx, const CLIP clip, const bool interpolated)
{
    const OBJECT_MESH *const mesh = Object_GetMesh(mesh_idx);
    if (interpolated) {
        Output_DrawObjectMesh_I(mesh, clip);
    } else {
        Output_DrawObjectMesh(mesh, clip);
    }
}

void Object_DrawStaticObject(
    const OBJECT *const obj, const ANIM_FRAME *const frame)
{
    Matrix_Push();
    Object_DrawMesh(obj->mesh_idx, 0, false);
    for (int32_t i = 1; i < obj->mesh_count; i++) {
        const ANIM_BONE *const bone = Object_GetBone(obj, i - 1);
        if (bone->matrix_pop) {
            Matrix_Pop();
        }
        if (bone->matrix_push) {
            Matrix_Push();
        }

        Matrix_TranslateRel32(bone->pos);
        Matrix_Rot16(frame->mesh_rots[i]);
        Object_DrawMesh(obj->mesh_idx + i, 0, false);
    }
    Matrix_Pop();
}

bool Object_DrawAnimatingItem(const ITEM *item)
{
    return Object_DrawAnimatingItemWithSwap(item, nullptr);
}

bool Object_DrawAnimatingItemWithSwap(
    const ITEM *const item, const OBJECT *const mesh_swap)
{
    ANIM_FRAME *frames[2];
    int32_t rate;
    const int32_t frac = Item_GetFrames(item, frames, &rate);
    const OBJECT *const obj = Object_Get(item->object_id);
    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);

    const OBJECT *swap_obj = mesh_swap;
    if (swap_obj != nullptr && !swap_obj->loaded) {
        swap_obj = nullptr;
    }
    if (swap_obj != nullptr) {
        ASSERT(swap_obj->mesh_count == obj->mesh_count);
    }

    if (obj->shadow_size != 0) {
        Output_DrawShadow(obj->shadow_size, bounds, item);
    }

    Matrix_Push();
    Matrix_TranslateAbs32(item->interp.result.pos);
    Matrix_Rot16(item->interp.result.rot);

    const CLIP clip = Output_CheckBoundsClip(bounds);
    if (clip == CLIP_NOT_VISIBLE) {
        Matrix_Pop();
        return false;
    }

    Output_CalculateObjectLighting(item, bounds);

    const int16_t *extra_rotation = item->extra_rotations;

    bool result = Object_DrawInterpolatedObjectWithSwap(
        obj, item->mesh_bits, extra_rotation, frames[0], frames[1], frac, rate,
        swap_obj);
    if (g_Config.debug.enable_debug_bounding_boxes) {
        Output_DrawCuboid(bounds);
    }
    Matrix_Pop();
    return result;
}

bool Object_DrawInterpolatedObject(
    const OBJECT *const obj, const uint32_t mesh_mask,
    const int16_t *extra_rotation, const ANIM_FRAME *const frame1,
    const ANIM_FRAME *const frame2, const int32_t frac, const int32_t rate)
{
    return Object_DrawInterpolatedObjectWithSwap(
        obj, mesh_mask, extra_rotation, frame1, frame2, frac, rate, nullptr);
}

bool Object_DrawInterpolatedObjectWithSwap(
    const OBJECT *const obj, const uint32_t mesh_mask,
    const int16_t *extra_rotation, const ANIM_FRAME *const frame1,
    const ANIM_FRAME *const frame2, const int32_t frac, const int32_t rate,
    const OBJECT *const mesh_swap)
{
    if (frame1 == nullptr) {
        return false;
    }
    ASSERT(frame1 != nullptr);
    const CLIP clip = Output_CheckBoundsClip(&frame1->bounds);
    if (clip == CLIP_NOT_VISIBLE) {
        return false;
    }

    ASSERT(rate != 0);
    Matrix_Push();

    if (frac != 0) {
        for (int32_t mesh_idx = 0; mesh_idx < obj->mesh_count; mesh_idx++) {
            if (mesh_idx == 0) {
                Matrix_InitInterpolate(frac, rate);
                Matrix_TranslateRel16_ID(frame1->offset, frame2->offset);
                Matrix_Rot16_ID(
                    frame1->mesh_rots[mesh_idx], frame2->mesh_rots[mesh_idx]);
                Object_ApplyExtraRotation(&extra_rotation, obj->base_rot, true);
            } else {
                const ANIM_BONE *const bone = Object_GetBone(obj, mesh_idx - 1);
                if (bone->matrix_pop) {
                    Matrix_Pop_I();
                }
                if (bone->matrix_push) {
                    Matrix_Push_I();
                }

                Matrix_TranslateRel32_I(bone->pos);
                Matrix_Rot16_ID(
                    frame1->mesh_rots[mesh_idx], frame2->mesh_rots[mesh_idx]);
                Object_ApplyExtraRotation(&extra_rotation, bone->rot, true);
            }

            if ((mesh_mask & (1u << mesh_idx)) != 0) {
                Object_DrawMesh(obj->mesh_idx + mesh_idx, clip, true);
            } else if (mesh_swap != nullptr) {
                Object_DrawMesh(mesh_swap->mesh_idx + mesh_idx, clip, true);
            }
        }
    } else {
        for (int32_t mesh_idx = 0; mesh_idx < obj->mesh_count; mesh_idx++) {
            if (mesh_idx == 0) {
                Matrix_TranslateRel16(frame1->offset);
                Matrix_Rot16(frame1->mesh_rots[mesh_idx]);
                Object_ApplyExtraRotation(
                    &extra_rotation, obj->base_rot, false);
            } else {
                const ANIM_BONE *const bone = Object_GetBone(obj, mesh_idx - 1);
                if (bone->matrix_pop) {
                    Matrix_Pop();
                }
                if (bone->matrix_push) {
                    Matrix_Push();
                }

                Matrix_TranslateRel32(bone->pos);
                Matrix_Rot16(frame1->mesh_rots[mesh_idx]);
                Object_ApplyExtraRotation(&extra_rotation, bone->rot, false);
            }

            if ((mesh_mask & (1u << mesh_idx)) != 0) {
                Object_DrawMesh(obj->mesh_idx + mesh_idx, clip, false);
            } else if (mesh_swap != nullptr) {
                Object_DrawMesh(mesh_swap->mesh_idx + mesh_idx, clip, false);
            }
        }
    }

    Matrix_Pop();
    return true;
}

void Object_ApplyExtraRotation(
    const int16_t **const extra_rotation, const XYZ_BOOL rot_flags,
    const bool interpolated)
{
    const int16_t *rot_ptr = *extra_rotation;
    if (rot_ptr == nullptr) {
        return;
    }

#define APPLY_ROTATION(axis_, flag_)                                           \
    if (rot_flags.flag_) {                                                     \
        if (interpolated) {                                                    \
            Matrix_Rot##axis_##_I(*rot_ptr++);                                 \
        } else {                                                               \
            Matrix_Rot##axis_(*rot_ptr++);                                     \
        }                                                                      \
    }

    APPLY_ROTATION(Y, y);
    APPLY_ROTATION(X, x);
    APPLY_ROTATION(Z, z);

#undef APPLY_ROTATION
    *extra_rotation = rot_ptr;
}

bool Object_DrawSpriteItem(const ITEM *const item)
{
    const RGB_F tint = Output_GetTint();
    SHADE shade = item->shade;
    if (shade.value_1 < 0) {
        shade.value_1 = SHADE_NEUTRAL;
    }

    if (g_TRVersion > 1) {
        Output_CalculateStaticMeshLight(
            item->interp.result.pos, shade, Room_Get(item->room_num));
        shade.value_1 = Output_GetLightAdder() + SHADE_NEUTRAL;
    }

    const OBJECT *const obj = Object_Get(item->object_id);
    Output_DrawSprite(
        item->interp.result.pos.x, item->interp.result.pos.y,
        item->interp.result.pos.z, obj->mesh_idx - item->frame_num,
        shade.value_1, tint, DRAW_BLEND);
    return true;
}

bool Object_DrawPickupItem(const ITEM *const item)
{
    if ((item->flags & IF_INVISIBLE) != 0) {
        return false;
    }

    if (!g_Config.visuals.enable_3d_pickups
        && Object_Get(item->object_id)->loaded) {
        return Object_DrawSpriteItem(item);
    }

    // Convert item to menu display item.
    const OBJECT *obj = Object_TryGet(Inv_GetItemPickup(item->object_id));
    if (obj == nullptr || !obj->loaded || obj->mesh_count < 0) {
        obj = Object_TryGet(Inv_GetItemOption(item->object_id));
    }

    if (obj == nullptr || !obj->loaded || obj->mesh_count < 0) {
        return Object_DrawSpriteItem(item);
    }

    // Standardize the bounds and offsets of all pickup items, and handle cases
    // such as the prayer wheels in Barkhang Monastery, which have no frames.
    const BOUNDS_16 bounds = M_GetBoundingBox(obj, nullptr, item->mesh_bits);
    XYZ_16 offset = {};

    const XYZ_16 *mesh_rots = nullptr;
    if (obj->anim_idx != NO_ANIM) {
        const ANIM_FRAME *const frame = obj->frame_base;
        mesh_rots = frame->mesh_rots;
        offset = frame->offset;
        if (Object_IsType(item->object_id, g_ElevatedPickupObjects)) {
            offset.y = (frame->bounds.min.y - frame->offset.y) / 2;
        } else {
            offset.y -= frame->bounds.max.y;
        }
    } else {
        offset.y = (bounds.max.y - bounds.min.y) / -2;
    }

    Matrix_Push();
    Matrix_TranslateAbs32(item->interp.result.pos);
    Matrix_Rot16(item->interp.result.rot);
    Matrix_TranslateRel16(offset);

    Output_CalculateLight(item->pos, item->room_num);

    const CLIP clip = Output_CheckBoundsClip(&bounds);
    if (clip != CLIP_NOT_VISIBLE) {
        for (int32_t mesh_idx = 0; mesh_idx < obj->mesh_count; mesh_idx++) {
            if (mesh_idx > 0) {
                const ANIM_BONE *const bone = Object_GetBone(obj, mesh_idx - 1);
                if (bone->matrix_pop) {
                    Matrix_Pop();
                }
                if (bone->matrix_push) {
                    Matrix_Push();
                }

                Matrix_TranslateRel32(bone->pos);
            }

            if (mesh_rots != nullptr) {
                Matrix_Rot16(mesh_rots[mesh_idx]);
            }

            if ((item->mesh_bits & (1 << mesh_idx)) != 0) {
                Object_DrawMesh(obj->mesh_idx + mesh_idx, clip, false);
            }
        }
    }

    Matrix_Pop();
    return true;
}
