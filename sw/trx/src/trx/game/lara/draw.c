#include <trx/game/lara/draw.h>

#include <trx/config.h>
#include <trx/game/gun/misc.h>
#include <trx/game/items/utils.h>
#include <trx/game/lara.h>
#include <trx/game/lara/electric.h>
#include <trx/game/lara/pose.h>
#include <trx/game/matrix.h>
#include <trx/game/output.h>
#include <trx/game/output/state.h>
#include <trx/game/output/vars.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

static bool m_CacheMatrices = false;
static bool m_IsLara = true;

static void M_CacheMatrix(const LARA_MESH mesh)
{
    if (!m_CacheMatrices) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->mesh_pos_matrices[mesh] = *g_WMatrixPtr;
}

static void M_DrawEquipmentMesh(
    const OBJECT_MESH *const mesh, const CLIP clip, const bool interpolated)
{
    const GAME_VECTOR pos = {
        .room_num = Lara_GetItem()->room_num,
        .pos = Matrix_MulVec32_M(
            g_WMatrixPtr,
            (XYZ_32) {
                mesh->center.x,
                mesh->center.y - 24,
                mesh->center.z,
            }),
    };
    Output_PushTintOverride(Lara_GetMeshTint(pos));
    if (interpolated) {
        Output_DrawObjectMesh_I(mesh, clip);
    } else {
        Output_DrawObjectMesh(mesh, clip);
    }
    Output_PopTintOverride();
}

static void M_DrawLaraMesh(
    const ITEM *const item, const LARA_MESH mesh_num, const CLIP clip,
    const bool interpolated)
{
    if (m_IsLara && !Item_IsMeshVisible(item, mesh_num)) {
        return;
    }

    const OBJECT_MESH *const mesh = Lara_Mesh_Get(mesh_num);
    XYZ_32 origin = XYZ_32_From16(mesh->center);
    switch (mesh_num) {
    case LM_TORSO:
    case LM_CALF_L:
    case LM_CALF_R:
    case LM_FOOT_L:
    case LM_FOOT_R:
        origin.y -= 24;
        break;
    case LM_HEAD:
        origin.y -= 8;
        break;
    default:
        break;
    }
    const GAME_VECTOR pos = {
        .room_num = item->room_num,
        .pos = Matrix_MulVec32_M(g_WMatrixPtr, origin),
    };
    Output_PushTintOverride(Lara_GetMeshTint(pos));
    if (interpolated) {
        Output_DrawObjectMesh_I(mesh, clip);
    } else {
        Output_DrawObjectMesh(mesh, clip);
    }
    Output_PopTintOverride();
}

static void M_DrawBodyPart(
    const LARA_MESH mesh, const ANIM_BONE *const bone,
    const XYZ_16 *mesh_rots_1, const XYZ_16 *mesh_rots_2, const CLIP clip)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (mesh_rots_2 != nullptr) {
        Matrix_TranslateRel32_I(bone[mesh - 1].pos);
        Matrix_Rot16_ID(mesh_rots_1[mesh], mesh_rots_2[mesh]);
        M_CacheMatrix(mesh);
        M_DrawLaraMesh(Lara_GetItem(), mesh, clip, true);
    } else {
        Matrix_TranslateRel32(bone[mesh - 1].pos);
        Matrix_Rot16(mesh_rots_1[mesh]);
        M_CacheMatrix(mesh);
        M_DrawLaraMesh(Lara_GetItem(), mesh, clip, false);
    }
}

static inline void M_DrawEquipment(
    const LARA_MESH mesh, const CLIP clip, const bool interpolated)
{
    if (!m_IsLara) {
        return;
    }

    if (!Item_IsMeshVisible(Lara_GetItem(), mesh)) {
        return;
    }

    const LARA_SKIN_EQUIPMENT *const equipment = Lara_Skin_GetEquipment(mesh);
    if (!equipment->visible || equipment->mesh == nullptr) {
        return;
    }

    M_DrawEquipmentMesh(equipment->mesh, clip, interpolated);
}

static bool M_Draw_I(
    const ITEM *const item, const ANIM_FRAME *const frame1,
    const ANIM_FRAME *const frame2, const int32_t frac, const int32_t rate)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);

    if (!Lara_Vehicle_IsMounted()) {
        Output_DrawShadow(obj->shadow_size, bounds, item);
    }

    MATRIX saved_matrix = *g_MatrixPtr;
    MATRIX wsaved_matrix = *g_WMatrixPtr;

    Matrix_Push();
    Matrix_TranslateAbs32(item->interp.result.pos);
    Matrix_Rot16(item->interp.result.rot);

    const CLIP clip = Output_CheckBoundsClip(&frame1->bounds);
    if (clip == CLIP_NOT_VISIBLE && !m_IsLara) {
        m_CacheMatrices = false;
        Matrix_Pop();
        return false;
    }

    if (g_Config.debug.enable_debug_bounding_boxes) {
        Output_DrawCuboid(&frame1->bounds);
    }

    m_CacheMatrices = m_IsLara;
    if (m_CacheMatrices) {
        lara->mesh_pos_matrices_valid = false;
    }

    Matrix_Push();
    Output_CalculateObjectLighting(item, &frame1->bounds);

    const ANIM_BONE *const bone =
        m_IsLara ? Lara_Skin_GetBoneBase() : Object_GetBone(obj, 0);
    const XYZ_16 *mesh_rots_1 = frame1->mesh_rots;
    const XYZ_16 *mesh_rots_2 = frame2->mesh_rots;

    Matrix_InitInterpolate(frac, rate);
    Matrix_TranslateRel16_ID(frame1->offset, frame2->offset);
    Matrix_Rot16_ID(mesh_rots_1[LM_HIPS], mesh_rots_2[LM_HIPS]);
    M_CacheMatrix(LM_HIPS);
    M_DrawLaraMesh(item, LM_HIPS, clip, true);
    M_DrawEquipment(LM_HIPS, clip, true);

    Matrix_Push_I();
    M_DrawBodyPart(LM_THIGH_L, bone, mesh_rots_1, mesh_rots_2, clip);
    M_DrawEquipment(LM_THIGH_L, clip, true);
    M_DrawBodyPart(LM_CALF_L, bone, mesh_rots_1, mesh_rots_2, clip);
    M_DrawBodyPart(LM_FOOT_L, bone, mesh_rots_1, mesh_rots_2, clip);
    Matrix_Pop_I();

    Matrix_Push_I();
    M_DrawBodyPart(LM_THIGH_R, bone, mesh_rots_1, mesh_rots_2, clip);
    M_DrawEquipment(LM_THIGH_R, clip, true);
    M_DrawBodyPart(LM_CALF_R, bone, mesh_rots_1, mesh_rots_2, clip);
    M_DrawBodyPart(LM_FOOT_R, bone, mesh_rots_1, mesh_rots_2, clip);
    Matrix_Pop_I();

    Matrix_TranslateRel32_I(bone[LM_TORSO - 1].pos);
    if (Lara_IsM16Active()) {
        mesh_rots_2 =
            lara->right_arm.frame_base[lara->right_arm.frame_num].mesh_rots;
        mesh_rots_1 = mesh_rots_2;
    }

    Matrix_Rot16_ID(mesh_rots_1[LM_TORSO], mesh_rots_2[LM_TORSO]);
    Matrix_Rot16_I(lara->interp.result.torso_rot);
    M_CacheMatrix(LM_TORSO);
    M_DrawLaraMesh(item, LM_TORSO, clip, true);
    M_DrawEquipment(LM_TORSO, clip, true);

    Matrix_Push_I();
    Matrix_TranslateRel32_I(bone[LM_HEAD - 1].pos);
    Matrix_Rot16_ID(mesh_rots_1[LM_HEAD], mesh_rots_2[LM_HEAD]);
    Matrix_Rot16_I(lara->interp.result.head_rot);
    M_CacheMatrix(LM_HEAD);
    M_DrawLaraMesh(item, LM_HEAD, clip, true);
    M_DrawEquipment(LM_HEAD, clip, true);

    *g_MatrixPtr = saved_matrix;
    *g_WMatrixPtr = wsaved_matrix;
    if (m_IsLara) {
        Lara_Hair_Draw();
    }
    Matrix_Pop_I();

    LARA_GUN_TYPE gun_type = LGT_UNARMED;
    if (lara->gun_status == LGS_READY || lara->gun_status == LGS_SPECIAL
        || lara->gun_status == LGS_DRAW || lara->gun_status == LGS_UNDRAW) {
        gun_type = lara->gun_type;
    }

    switch (gun_type) {
    case LGT_UNARMED:
    case LGT_FLARE:
        Matrix_Push_I();
        M_DrawBodyPart(LM_UARM_R, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawBodyPart(LM_LARM_R, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawBodyPart(LM_HAND_R, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawEquipment(LM_HAND_R, clip, true);
        Matrix_Pop_I();

        Matrix_Push_I();
        Matrix_TranslateRel32_I(bone[LM_UARM_L - 1].pos);
        if (lara->flare.control) {
            const ANIM *const anim = Anim_GetAnim(lara->left_arm.anim_num);
            mesh_rots_1 =
                lara->left_arm
                    .frame_base[lara->left_arm.frame_num - anim->frame_base]
                    .mesh_rots;
            mesh_rots_2 = mesh_rots_1;
        }

        Matrix_Rot16_ID(mesh_rots_1[LM_UARM_L], mesh_rots_2[LM_UARM_L]);
        M_CacheMatrix(LM_UARM_L);
        M_DrawLaraMesh(item, LM_UARM_L, clip, true);

        M_DrawBodyPart(LM_LARM_L, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawBodyPart(LM_HAND_L, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawEquipment(LM_HAND_L, clip, true);

        if (g_TRVersion < 3 && lara->gun_type == LGT_FLARE
            && lara->left_arm.flash_gun) {
            Gun_DrawFlash(LGT_FLARE, clip, true);
        }
        Matrix_Pop();
        break;

    case LGT_PISTOLS:
    case LGT_MAGNUMS:
    case LGT_AUTOS:
    case LGT_DESERT_EAGLE:
    case LGT_UZIS: {
        Matrix_Push_I();
        Matrix_TranslateRel32_I(bone[LM_UARM_R - 1].pos);
        Matrix_InterpolateArm();
        if (gun_type == LGT_DESERT_EAGLE) {
            Matrix_Rot16(lara->interp.result.torso_rot);
        } else {
            Matrix_Rot16(lara->right_arm.interp.result.rot);
        }

        const ANIM *anim = Anim_GetAnim(lara->right_arm.anim_num);
        mesh_rots_1 =
            lara->right_arm
                .frame_base[lara->right_arm.frame_num - anim->frame_base]
                .mesh_rots;
        Matrix_Rot16(mesh_rots_1[LM_UARM_R]);
        M_CacheMatrix(LM_UARM_R);
        M_DrawLaraMesh(item, LM_UARM_R, clip, false);

        M_DrawBodyPart(LM_LARM_R, bone, mesh_rots_1, nullptr, clip);
        M_DrawBodyPart(LM_HAND_R, bone, mesh_rots_1, nullptr, clip);
        M_DrawEquipment(LM_HAND_R, clip, false);
        if (lara->right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
            wsaved_matrix = *g_WMatrixPtr;
        }
        Matrix_Pop_I();

        Matrix_Push_I();
        Matrix_TranslateRel32_I(bone[LM_UARM_L - 1].pos);
        Matrix_InterpolateArm();
        if (gun_type == LGT_DESERT_EAGLE) {
            Matrix_Rot16(lara->interp.result.torso_rot);
        } else {
            Matrix_Rot16(lara->left_arm.interp.result.rot);
        }

        anim = Anim_GetAnim(lara->left_arm.anim_num);
        mesh_rots_1 =
            lara->left_arm
                .frame_base[lara->left_arm.frame_num - anim->frame_base]
                .mesh_rots;
        Matrix_Rot16(mesh_rots_1[LM_UARM_L]);
        M_CacheMatrix(LM_UARM_L);
        M_DrawLaraMesh(item, LM_UARM_L, clip, false);

        M_DrawBodyPart(LM_LARM_L, bone, mesh_rots_1, nullptr, clip);
        M_DrawBodyPart(LM_HAND_L, bone, mesh_rots_1, nullptr, clip);
        M_DrawEquipment(LM_HAND_L, clip, false);

        if (lara->left_arm.flash_gun) {
            Gun_DrawFlash(gun_type, clip, false);
        }
        if (lara->right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            *g_WMatrixPtr = wsaved_matrix;
            Gun_DrawFlash(gun_type, clip, false);
        }
        Matrix_Pop();
        break;
    }

    case LGT_SHOTGUN:
    case LGT_M16:
    case LGT_MP5:
    case LGT_GRENADE:
    case LGT_ROCKET:
    case LGT_HARPOON: {
        Matrix_Push_I();
        Matrix_TranslateRel32_I(bone[LM_UARM_R - 1].pos);
        mesh_rots_1 =
            lara->right_arm.frame_base[lara->right_arm.frame_num].mesh_rots;
        mesh_rots_2 = mesh_rots_1;
        Matrix_Rot16_ID(mesh_rots_1[LM_UARM_R], mesh_rots_2[LM_UARM_R]);
        M_CacheMatrix(LM_UARM_R);
        M_DrawLaraMesh(item, LM_UARM_R, clip, true);

// NOTE: gcc wrongly complains about mesh_rots_1 possibly being nullptr.
// While this is not the case, it's curious how the pistols subtract the
// frame_base from lara->*_arm.frame_num to access the mesh_rots, and the
// rifles do not.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

        M_DrawBodyPart(LM_LARM_R, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawBodyPart(LM_HAND_R, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawEquipment(LM_HAND_R, clip, true);

        if (lara->right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
            wsaved_matrix = *g_WMatrixPtr;
        }
        Matrix_Pop_I();

        Matrix_Push_I();
        M_DrawBodyPart(LM_UARM_L, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawBodyPart(LM_LARM_L, bone, mesh_rots_1, mesh_rots_2, clip);
        M_DrawBodyPart(LM_HAND_L, bone, mesh_rots_1, mesh_rots_2, clip);

#pragma GCC diagnostic pop

        if (lara->right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            *g_WMatrixPtr = wsaved_matrix;
            Gun_DrawFlash(gun_type, clip, false);
        }
        Matrix_Pop();
        break;
    }

    default:
        break;
    }

    if (m_CacheMatrices) {
        lara->mesh_pos_matrices_valid = true;
    }
    m_CacheMatrices = false;

    Matrix_Pop();
    Matrix_Pop();
    return true;
}

bool Lara_Draw(const ITEM *const item)
{
    const ITEM *const lara_item = Lara_GetItem();
    m_IsLara = item == lara_item;
    if (m_IsLara
        && (item->status == IS_INVISIBLE || (item->flags & IF_ONE_SHOT) != 0)) {
        return false;
    }

    const int32_t top = g_PhdTop;
    const int32_t left = g_PhdLeft;
    const int32_t right = g_PhdRight;
    const int32_t bottom = g_PhdBottom;

    g_PhdLeft = Viewport_GetMinX(VIEWPORT_GAME);
    g_PhdRight = Viewport_GetMaxX(VIEWPORT_GAME);
    g_PhdTop = Viewport_GetMinY(VIEWPORT_GAME);
    g_PhdBottom = Viewport_GetMaxY(VIEWPORT_GAME);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    ANIM_FRAME *frames[2];
    if (lara->hit_direction < 0) {
        int32_t rate;
        const int32_t frac = Item_GetFrames(lara_item, frames, &rate);
        if (frac != 0 && Lara_Pose_Get() == nullptr) {
            M_Draw_I(item, frames[0], frames[1], frac, rate);
            goto finish;
        }
    }

    const ANIM_FRAME *const hit_frame = Lara_GetHitFrame(item);
    const ANIM_FRAME *const frame =
        hit_frame == nullptr ? frames[0] : hit_frame;

    const OBJECT *const obj = Object_Get(item->object_id);
    const BOUNDS_16 *const shadow_bounds = Item_GetBoundsAccurate(item);
    if (!Lara_Vehicle_IsMounted()) {
        Output_DrawShadow(obj->shadow_size, shadow_bounds, item);
    }

    MATRIX saved_matrix = *g_MatrixPtr;
    MATRIX wsaved_matrix = *g_WMatrixPtr;

    Matrix_Push();
    Matrix_TranslateAbs32(item->interp.result.pos);
    Matrix_Rot16(item->interp.result.rot);
    const MATRIX item_matrix = *g_MatrixPtr;
    const MATRIX item_wmatrix = *g_WMatrixPtr;
    const CLIP clip = Output_CheckBoundsClip(&frame->bounds);
    if (clip == CLIP_NOT_VISIBLE && !m_IsLara) {
        m_CacheMatrices = false;
        Matrix_Pop();
        return false;
    }

    if (g_Config.debug.enable_debug_bounding_boxes) {
        Output_DrawCuboid(&frame->bounds);
    }

    m_CacheMatrices = m_IsLara;
    if (m_CacheMatrices) {
        lara->mesh_pos_matrices_valid = false;
    }

    Matrix_Push();
    Output_CalculateObjectLighting(item, &frame->bounds);

    const ANIM_BONE *const bone =
        m_IsLara ? Lara_Skin_GetBoneBase() : Object_GetBone(obj, 0);
    const LARA_POSE *const pose = Lara_Pose_Get();
    const XYZ_16 *mesh_rots = pose != nullptr ? pose->rots : frame->mesh_rots;

    Matrix_TranslateRel16(pose != nullptr ? pose->offset : frame->offset);
    Matrix_Rot16(mesh_rots[LM_HIPS]);
    M_CacheMatrix(LM_HIPS);
    M_DrawLaraMesh(item, LM_HIPS, clip, false);
    M_DrawEquipment(LM_HIPS, clip, false);

    Matrix_Push();
    M_DrawBodyPart(LM_THIGH_L, bone, mesh_rots, nullptr, clip);
    M_DrawEquipment(LM_THIGH_L, clip, false);
    M_DrawBodyPart(LM_CALF_L, bone, mesh_rots, nullptr, clip);
    M_DrawBodyPart(LM_FOOT_L, bone, mesh_rots, nullptr, clip);
    Matrix_Pop();

    Matrix_Push();
    M_DrawBodyPart(LM_THIGH_R, bone, mesh_rots, nullptr, clip);
    M_DrawEquipment(LM_THIGH_R, clip, false);
    M_DrawBodyPart(LM_CALF_R, bone, mesh_rots, nullptr, clip);
    M_DrawBodyPart(LM_FOOT_R, bone, mesh_rots, nullptr, clip);
    Matrix_Pop();

    Matrix_TranslateRel32(bone[LM_TORSO - 1].pos);
    if (Lara_IsM16Active() && pose == nullptr) {
        mesh_rots =
            lara->right_arm.frame_base[lara->right_arm.frame_num].mesh_rots;
    }

    Matrix_Rot16(mesh_rots[LM_TORSO]);
    Matrix_Rot16(lara->interp.result.torso_rot);
    M_CacheMatrix(LM_TORSO);
    M_DrawLaraMesh(item, LM_TORSO, clip, false);
    M_DrawEquipment(LM_TORSO, clip, false);

    Matrix_Push();
    Matrix_TranslateRel32(bone[LM_HEAD - 1].pos);
    Matrix_Rot16(mesh_rots[LM_HEAD]);
    Matrix_Rot16(lara->interp.result.head_rot);
    M_CacheMatrix(LM_HEAD);
    M_DrawLaraMesh(item, LM_HEAD, clip, false);
    M_DrawEquipment(LM_HEAD, clip, false);

    *g_MatrixPtr = saved_matrix;
    *g_WMatrixPtr = wsaved_matrix;
    if (m_IsLara) {
        Lara_Hair_Draw();
    }

    Matrix_Pop();

    LARA_GUN_TYPE gun_type = LGT_UNARMED;
    if (pose == nullptr
        && (lara->gun_status == LGS_READY || lara->gun_status == LGS_SPECIAL
            || lara->gun_status == LGS_DRAW
            || lara->gun_status == LGS_UNDRAW)) {
        gun_type = lara->gun_type;
    }

    switch (gun_type) {
    case LGT_UNARMED:
    case LGT_FLARE:
        Matrix_Push();
        M_DrawBodyPart(LM_UARM_R, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_LARM_R, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_HAND_R, bone, mesh_rots, nullptr, clip);
        M_DrawEquipment(LM_HAND_R, clip, false);
        Matrix_Pop();

        Matrix_Push();
        Matrix_TranslateRel32(bone[LM_UARM_L - 1].pos);
        if (lara->flare.control && pose == nullptr) {
            const ANIM *const anim = Anim_GetAnim(lara->left_arm.anim_num);
            mesh_rots =
                lara->left_arm
                    .frame_base[lara->left_arm.frame_num - anim->frame_base]
                    .mesh_rots;
        }

        Matrix_Rot16(mesh_rots[LM_UARM_L]);
        M_DrawLaraMesh(item, LM_UARM_L, clip, false);

        M_DrawBodyPart(LM_LARM_L, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_HAND_L, bone, mesh_rots, nullptr, clip);
        M_DrawEquipment(LM_HAND_L, clip, false);

        if (g_TRVersion < 3 && lara->gun_type == LGT_FLARE
            && lara->left_arm.flash_gun) {
            Gun_DrawFlash(LGT_FLARE, clip, false);
        }

        Matrix_Pop();
        break;

    case LGT_PISTOLS:
    case LGT_MAGNUMS:
    case LGT_AUTOS:
    case LGT_DESERT_EAGLE:
    case LGT_UZIS: {
        Matrix_Push();
        Matrix_TranslateRel32(bone[LM_UARM_R - 1].pos);
        g_MatrixPtr->_00 = item_matrix._00;
        g_MatrixPtr->_01 = item_matrix._01;
        g_MatrixPtr->_02 = item_matrix._02;
        g_MatrixPtr->_10 = item_matrix._10;
        g_MatrixPtr->_11 = item_matrix._11;
        g_MatrixPtr->_12 = item_matrix._12;
        g_MatrixPtr->_20 = item_matrix._20;
        g_MatrixPtr->_21 = item_matrix._21;
        g_MatrixPtr->_22 = item_matrix._22;
        g_WMatrixPtr->_00 = item_wmatrix._00;
        g_WMatrixPtr->_01 = item_wmatrix._01;
        g_WMatrixPtr->_02 = item_wmatrix._02;
        g_WMatrixPtr->_10 = item_wmatrix._10;
        g_WMatrixPtr->_11 = item_wmatrix._11;
        g_WMatrixPtr->_12 = item_wmatrix._12;
        g_WMatrixPtr->_20 = item_wmatrix._20;
        g_WMatrixPtr->_21 = item_wmatrix._21;
        g_WMatrixPtr->_22 = item_wmatrix._22;

        if (gun_type == LGT_DESERT_EAGLE) {
            Matrix_Rot16(lara->interp.result.torso_rot);
        } else {
            Matrix_Rot16(lara->right_arm.interp.result.rot);
        }
        if (pose == nullptr) {
            const ANIM *const anim = Anim_GetAnim(lara->right_arm.anim_num);
            mesh_rots =
                lara->right_arm
                    .frame_base[lara->right_arm.frame_num - anim->frame_base]
                    .mesh_rots;
        }
        Matrix_Rot16(mesh_rots[LM_UARM_R]);
        M_DrawLaraMesh(item, LM_UARM_R, clip, false);

        M_DrawBodyPart(LM_LARM_R, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_HAND_R, bone, mesh_rots, nullptr, clip);
        M_DrawEquipment(LM_HAND_R, clip, false);

        if (lara->right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
            wsaved_matrix = *g_WMatrixPtr;
        }
        Matrix_Pop();

        Matrix_Push();
        Matrix_TranslateRel32(bone[LM_UARM_L - 1].pos);
        g_MatrixPtr->_00 = item_matrix._00;
        g_MatrixPtr->_01 = item_matrix._01;
        g_MatrixPtr->_02 = item_matrix._02;
        g_MatrixPtr->_10 = item_matrix._10;
        g_MatrixPtr->_11 = item_matrix._11;
        g_MatrixPtr->_12 = item_matrix._12;
        g_MatrixPtr->_20 = item_matrix._20;
        g_MatrixPtr->_21 = item_matrix._21;
        g_MatrixPtr->_22 = item_matrix._22;
        g_WMatrixPtr->_00 = item_wmatrix._00;
        g_WMatrixPtr->_01 = item_wmatrix._01;
        g_WMatrixPtr->_02 = item_wmatrix._02;
        g_WMatrixPtr->_10 = item_wmatrix._10;
        g_WMatrixPtr->_11 = item_wmatrix._11;
        g_WMatrixPtr->_12 = item_wmatrix._12;
        g_WMatrixPtr->_20 = item_wmatrix._20;
        g_WMatrixPtr->_21 = item_wmatrix._21;
        g_WMatrixPtr->_22 = item_wmatrix._22;

        if (gun_type == LGT_DESERT_EAGLE) {
            Matrix_Rot16(lara->interp.result.torso_rot);
        } else {
            Matrix_Rot16(lara->left_arm.interp.result.rot);
        }
        if (pose == nullptr) {
            const ANIM *const anim = Anim_GetAnim(lara->left_arm.anim_num);
            mesh_rots =
                lara->left_arm
                    .frame_base[lara->left_arm.frame_num - anim->frame_base]
                    .mesh_rots;
        }
        Matrix_Rot16(mesh_rots[LM_UARM_L]);
        M_CacheMatrix(LM_UARM_L);
        M_DrawLaraMesh(item, LM_UARM_L, clip, false);

        M_DrawBodyPart(LM_LARM_L, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_HAND_L, bone, mesh_rots, nullptr, clip);
        M_DrawEquipment(LM_HAND_L, clip, false);

        if (lara->left_arm.flash_gun) {
            Gun_DrawFlash(gun_type, clip, false);
        }
        if (lara->right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            *g_WMatrixPtr = wsaved_matrix;
            Gun_DrawFlash(gun_type, clip, false);
        }

        Matrix_Pop();
        break;
    }

    case LGT_SHOTGUN:
    case LGT_M16:
    case LGT_MP5:
    case LGT_GRENADE:
    case LGT_ROCKET:
    case LGT_HARPOON: {
        Matrix_Push();
        Matrix_TranslateRel32(bone[LM_UARM_R - 1].pos);
        if (pose == nullptr) {
            mesh_rots =
                lara->right_arm.frame_base[lara->right_arm.frame_num].mesh_rots;
        }
        Matrix_Rot16(mesh_rots[LM_UARM_R]);
        M_CacheMatrix(LM_UARM_R);
        M_DrawLaraMesh(item, LM_UARM_R, clip, false);

        M_DrawBodyPart(LM_LARM_R, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_HAND_R, bone, mesh_rots, nullptr, clip);
        M_DrawEquipment(LM_HAND_R, clip, false);

        if (lara->right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
            wsaved_matrix = *g_WMatrixPtr;
        }
        Matrix_Pop();

        Matrix_Push();
        M_DrawBodyPart(LM_UARM_L, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_LARM_L, bone, mesh_rots, nullptr, clip);
        M_DrawBodyPart(LM_HAND_L, bone, mesh_rots, nullptr, clip);

        if (lara->right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            *g_WMatrixPtr = wsaved_matrix;
            Gun_DrawFlash(gun_type, clip, false);
        }

        Matrix_Pop();
        break;
    }

    default:
        break;
    }

    if (m_CacheMatrices) {
        lara->mesh_pos_matrices_valid = true;
    }
    m_CacheMatrices = false;

    Matrix_Pop();
    Matrix_Pop();

finish:
    if (m_IsLara && lara->electric != 0) {
        Lara_Electricity_Draw(0, lara_item);
        Lara_Electricity_Draw(1, lara_item);
    }

    g_PhdLeft = left;
    g_PhdRight = right;
    g_PhdTop = top;
    g_PhdBottom = bottom;
    return true;
}
