#include <trx/game/lara/hair.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/lara.h>
#include <trx/game/lara/pose.h>
#include <trx/game/matrix.h>
#include <trx/game/output/state.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

#define M_HAIR_SEGMENTS 6
#define M_HAIR_SPHERES 5
#define M_BONE_IDX(segment)                                                    \
    (segment == M_HAIR_SEGMENTS ? (segment - 2) : (segment - 1))

static bool m_IsFirstHair;
static SPHERE m_HairSpheres[M_HAIR_SPHERES];
static XYZ_32 m_HairVelocity[M_HAIR_SEGMENTS + 1];
static HAIR_SEGMENT m_HairSegments[M_HAIR_SEGMENTS + 1];

static void M_CalculateSpheres(const ANIM_FRAME *const frame)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const OBJECT *const lara_obj = Object_Get(O_LARA);

    const LARA_POSE *const pose = Lara_Pose_Get();
    const XYZ_16 *mesh_rots = pose != nullptr ? pose->rots : frame->mesh_rots;

    Matrix_TranslateRel16(pose != nullptr ? pose->offset : frame->offset);
    Matrix_Rot16(mesh_rots[LM_HIPS]);

    Matrix_Push();
    const OBJECT_MESH *mesh = Object_GetMesh(lara_obj->mesh_idx + LM_HIPS);
    Matrix_TranslateRel16(mesh->center);
    m_HairSpheres[0].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[0].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[0].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[0].r = mesh->radius;
    Matrix_Pop();

    const ANIM_BONE *bone = Object_GetBone(lara_obj, 0);
    Matrix_TranslateRel32(bone[LM_TORSO - 1].pos);
    if (Lara_IsM16Active() && pose == nullptr) {
        mesh_rots =
            lara->right_arm.frame_base[lara->right_arm.frame_num].mesh_rots;
    }

    Matrix_Rot16(mesh_rots[LM_TORSO]);
    Matrix_Rot16(lara->interp.result.torso_rot);
    Matrix_Push();
    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_TORSO);
    Matrix_TranslateRel16(mesh->center);
    m_HairSpheres[1].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[1].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[1].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[1].r = mesh->radius;
    Matrix_Pop();

    Matrix_Push();
    Matrix_TranslateRel32(bone[LM_UARM_R - 1].pos);
    Matrix_Rot16(mesh_rots[LM_UARM_R]);

    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_UARM_R);
    Matrix_TranslateRel16(mesh->center);
    m_HairSpheres[3].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[3].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[3].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[3].r = mesh->radius * 3 / 2;
    Matrix_Pop();

    Matrix_Push();
    Matrix_TranslateRel32(bone[LM_UARM_L - 1].pos);
    Matrix_Rot16(mesh_rots[LM_UARM_L]);
    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_UARM_L);
    Matrix_TranslateRel16(mesh->center);
    m_HairSpheres[4].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[4].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[4].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[4].r = mesh->radius * 3 / 2;
    Matrix_Pop();

    Matrix_TranslateRel32(bone[LM_HEAD - 1].pos);
    Matrix_Rot16(mesh_rots[LM_HEAD]);
    Matrix_Rot16(lara->interp.result.head_rot);

    Matrix_Push();
    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_HEAD);
    Matrix_TranslateRel16(mesh->center);
    m_HairSpheres[2].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[2].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[2].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[2].r = mesh->radius;
    Matrix_Pop();

    Matrix_TranslateRel32(Lara_Skin_GetBraidOffset());
}

static void M_CalculateSpheres_I(
    const ANIM_FRAME *const frame_1, const ANIM_FRAME *const frame_2,
    const int32_t frac, const int32_t rate)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const OBJECT *const lara_obj = Object_Get(O_LARA);

    const XYZ_16 *mesh_rots_1 = frame_1->mesh_rots;
    const XYZ_16 *mesh_rots_2 = frame_2->mesh_rots;
    Matrix_InitInterpolate(frac, rate);
    Matrix_TranslateRel16_ID(frame_1->offset, frame_2->offset);
    Matrix_Rot16_ID(mesh_rots_1[LM_HIPS], mesh_rots_2[LM_HIPS]);

    Matrix_Push_I();
    const OBJECT_MESH *mesh = Object_GetMesh(lara_obj->mesh_idx + LM_HIPS);
    Matrix_TranslateRel16_I(mesh->center);
    Matrix_Interpolate();
    m_HairSpheres[0].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[0].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[0].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[0].r = mesh->radius;
    Matrix_Pop_I();

    const ANIM_BONE *bone = Object_GetBone(lara_obj, 0);
    Matrix_TranslateRel32_I(bone[LM_TORSO - 1].pos);
    if (Lara_IsM16Active()) {
        mesh_rots_1 =
            lara->right_arm.frame_base[lara->right_arm.frame_num].mesh_rots;
        mesh_rots_2 = mesh_rots_1;
    }

    Matrix_Rot16_ID(mesh_rots_1[LM_TORSO], mesh_rots_2[LM_TORSO]);
    Matrix_Rot16_I(lara->interp.result.torso_rot);

    Matrix_Push_I();
    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_TORSO);
    Matrix_TranslateRel16_I(mesh->center);
    Matrix_Interpolate();
    m_HairSpheres[1].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[1].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[1].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[1].r = mesh->radius;
    Matrix_Pop_I();

    Matrix_Push_I();
    Matrix_TranslateRel32_I(bone[LM_UARM_R - 1].pos);
    Matrix_Rot16_ID(mesh_rots_1[LM_UARM_R], mesh_rots_2[LM_UARM_R]);

    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_UARM_R);
    Matrix_TranslateRel16_I(mesh->center);
    Matrix_Interpolate();
    m_HairSpheres[3].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[3].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[3].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[3].r = mesh->radius * 3 / 2;
    Matrix_Pop_I();

    Matrix_Push_I();
    Matrix_TranslateRel32_I(bone[LM_UARM_L - 1].pos);
    Matrix_Rot16_ID(mesh_rots_1[LM_UARM_L], mesh_rots_2[LM_UARM_L]);

    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_UARM_L);
    Matrix_TranslateRel16_I(mesh->center);
    Matrix_Interpolate();
    m_HairSpheres[4].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[4].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[4].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[4].r = mesh->radius * 3 / 2;
    Matrix_Pop_I();

    Matrix_TranslateRel32_I(bone[LM_HEAD - 1].pos);
    Matrix_Rot16_ID(mesh_rots_1[LM_HEAD], mesh_rots_2[LM_HEAD]);
    Matrix_Rot16_I(lara->interp.result.head_rot);

    Matrix_Push_I();
    mesh = Object_GetMesh(lara_obj->mesh_idx + LM_HEAD);
    Matrix_TranslateRel16_I(mesh->center);
    Matrix_Interpolate();
    m_HairSpheres[2].pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
    m_HairSpheres[2].pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
    m_HairSpheres[2].pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;
    m_HairSpheres[2].r = mesh->radius;
    Matrix_Pop_I();

    Matrix_TranslateRel32_I(Lara_Skin_GetBraidOffset());
    Matrix_Interpolate();
}

void Lara_Hair_Initialise(void)
{
    const ANIM_BONE *const bones = Lara_Skin_GetBraidBoneBase();
    if (bones == nullptr) {
        return;
    }

    m_IsFirstHair = true;
    m_HairSegments[0].rot.x = -DEG_90;
    m_HairSegments[0].rot.y = 0;

    for (int32_t i = 1; i <= M_HAIR_SEGMENTS; i++) {
        const ANIM_BONE *const bone = &bones[M_BONE_IDX(i)];
        m_HairSegments[i].pos = bone->pos;
        m_HairSegments[i].rot.x = -DEG_90;
        m_HairSegments[i].rot.y = 0;
        m_HairSegments[i].rot.z = 0;
        m_HairVelocity[i - 1] = (XYZ_32) {};
    }
}

void Lara_Hair_Control(const bool in_cutscene)
{
    if (!Lara_Hair_IsActive()) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();

    const ANIM_FRAME *frame_1;
    const ANIM_FRAME *frame_2;
    int32_t frac;
    int32_t rate;
    const ANIM_FRAME *const hit_frame = Lara_GetHitFrame(lara_item);
    if (!in_cutscene && hit_frame != nullptr) {
        frame_1 = hit_frame;
        frac = 0;
    } else {
        ANIM_FRAME *frmptr[2];
        frac = Item_GetFrames(lara_item, frmptr, &rate);
        frame_1 = frmptr[0];
        frame_2 = frmptr[1];
    }

    Matrix_PushUnit();
    Matrix_TranslateSet32(lara_item->pos);
    Matrix_Rot16(lara_item->rot);

    if (frac == 0 || Lara_Pose_Get() != nullptr) {
        M_CalculateSpheres(frame_1);
    } else {
        M_CalculateSpheres_I(frame_1, frame_2, frac, rate);
    }

    const XYZ_32 pos = {
        .x = g_MatrixPtr->_03 >> W2V_SHIFT,
        .y = g_MatrixPtr->_13 >> W2V_SHIFT,
        .z = g_MatrixPtr->_23 >> W2V_SHIFT,
    };
    Matrix_Pop();

    const ANIM_BONE *const bones = Lara_Skin_GetBraidBoneBase();

    HAIR_SEGMENT *const fs = &m_HairSegments[0];
    fs->pos = pos;

    if (m_IsFirstHair) {
        m_IsFirstHair = false;
        for (int32_t i = 1; i <= M_HAIR_SEGMENTS; i++) {
            const ANIM_BONE *const bone = &bones[M_BONE_IDX(i)];
            const HAIR_SEGMENT *const ps = &m_HairSegments[i - 1];
            HAIR_SEGMENT *const s = &m_HairSegments[i];

            Matrix_PushUnit();
            Matrix_TranslateSet32(ps->pos);
            Matrix_RotY(ps->rot.y);
            Matrix_RotX(ps->rot.x);
            Matrix_TranslateRel32(bone->pos);

            s->pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
            s->pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
            s->pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;

            Matrix_Pop();
        }
        return;
    }

    int16_t room_num = lara_item->room_num;
    int32_t water_height;
    if (in_cutscene) {
        water_height = NO_HEIGHT;
    } else {
        water_height = Room_GetWaterHeight(
            (XYZ_32) {
                lara_item->pos.x
                    + (frame_1->bounds.min.x + frame_1->bounds.max.x) / 2,
                lara_item->pos.y
                    + (frame_1->bounds.max.y + frame_1->bounds.min.y) / 2,
                lara_item->pos.z
                    + (frame_1->bounds.max.z + frame_1->bounds.min.z) / 2,
            },
            room_num);
    }

    const SECTOR *const sector = Room_GetSector(fs->pos, &room_num);
    int32_t height = Room_GetHeight(sector, fs->pos);
    if (height < fs->pos.y) {
        height = lara_item->floor;
    }

    const XZ_32 smoke_wind = Sparks_GetSmokeWind();
    const int32_t hair_wind_z = Sparks_GetHairWindZ();

    for (int32_t i = 1; i <= M_HAIR_SEGMENTS; i++) {
        HAIR_SEGMENT *const ps = &m_HairSegments[i - 1];
        HAIR_SEGMENT *const s = &m_HairSegments[i];

        m_HairVelocity[0] = s->pos;

        s->pos.x += m_HairVelocity[i].x * 3 / 4;
        s->pos.y += m_HairVelocity[i].y * 3 / 4;
        s->pos.z += m_HairVelocity[i].z * 3 / 4;

        if (g_TRVersion == 3) {
            if (lara_info->water_status == LWS_ABOVE_WATER
                && Room_Get(room_num)->flags.wind) {
                s->pos.x += smoke_wind.x;
                s->pos.z += smoke_wind.z;
            }

            if (water_height == NO_HEIGHT || s->pos.y < water_height) {
                s->pos.y += 10;
                if (water_height != NO_HEIGHT && s->pos.y > water_height) {
                    s->pos.y = water_height;
                }
            }

            if (s->pos.y > height) {
                s->pos.x = m_HairVelocity[0].x;
                if (s->pos.y - height <= STEP_L) {
                    s->pos.y = height;
                }
                s->pos.z = m_HairVelocity[0].z;
            }
        } else {
            switch (lara_info->water_status) {
            case LWS_ABOVE_WATER:
                s->pos.y += 10;
                if (water_height != NO_HEIGHT && s->pos.y > water_height) {
                    s->pos.y = water_height;
                } else if (s->pos.y > height) {
                    s->pos.y = height;
                } else {
                    s->pos.z += hair_wind_z;
                }
                break;

            case LWS_UNDERWATER:
            case LWS_SURFACE:
            case LWS_WADE:
                CLAMP(s->pos.y, water_height, height);
                break;

            default:
                break;
            }
        }

        for (int32_t j = 0; j < M_HAIR_SPHERES; j++) {
            const SPHERE *const sphere = &m_HairSpheres[j];
            const int32_t dx = s->pos.x - sphere->pos.x;
            const int32_t dy = s->pos.y - sphere->pos.y;
            const int32_t dz = s->pos.z - sphere->pos.z;
            int32_t dist = SQUARE(dz) + SQUARE(dy) + SQUARE(dx);
            if (dist < SQUARE(sphere->r)) {
                dist = Math_Sqrt(dist);
                CLAMPL(dist, 1);
                s->pos.x = sphere->pos.x + sphere->r * dx / dist;
                s->pos.y = sphere->pos.y + sphere->r * dy / dist;
                s->pos.z = sphere->pos.z + sphere->r * dz / dist;
            }
        }

        const int32_t dx = s->pos.x - ps->pos.x;
        const int32_t dz = s->pos.z - ps->pos.z;
        const int32_t distance = Math_Sqrt(SQUARE(dx) + SQUARE(dz));
        ps->rot.y = Math_Atan(dz, dx);
        ps->rot.x = -Math_Atan(distance, s->pos.y - ps->pos.y);

        Matrix_PushUnit();
        Matrix_TranslateSet32(ps->pos);
        Matrix_RotY(ps->rot.y);
        Matrix_RotX(ps->rot.x);

        const ANIM_BONE *const bone = &bones[M_BONE_IDX(i)];
        Matrix_TranslateRel32(bone->pos);

        s->pos.x = g_MatrixPtr->_03 >> W2V_SHIFT;
        s->pos.y = g_MatrixPtr->_13 >> W2V_SHIFT;
        s->pos.z = g_MatrixPtr->_23 >> W2V_SHIFT;

        m_HairVelocity[i].x = s->pos.x - m_HairVelocity[0].x;
        m_HairVelocity[i].y = s->pos.y - m_HairVelocity[0].y;
        m_HairVelocity[i].z = s->pos.z - m_HairVelocity[0].z;

        Matrix_Pop();
    }
}

void Lara_Hair_Draw(void)
{
    if (!Lara_Hair_IsActive()) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t mesh_idx = Lara_Skin_GetBraidMeshIdx();

    for (int32_t i = 0; i < M_HAIR_SEGMENTS; i++) {
        const HAIR_SEGMENT *const s = &m_HairSegments[i];
        Matrix_Push();
        Matrix_TranslateAbs32(s->interp.result.pos);
        Matrix_RotY(s->interp.result.rot.y);
        Matrix_RotX(s->interp.result.rot.x);

        Output_PushTintOverride(Lara_GetMeshTint((GAME_VECTOR) {
            .pos = s->interp.result.pos, .room_num = lara_item->room_num }));
        Object_DrawMesh(mesh_idx + i, CLIP_FULLY_VISIBLE, false);
        Output_PopTintOverride();
        Matrix_Pop();
    }
}

bool Lara_Hair_IsActive(void)
{
    return g_Config.visuals.enable_braid && Object_Get(O_LARA)->loaded
        && Lara_Skin_IsBraidSupported();
}

int32_t Lara_Hair_GetSegmentCount(void)
{
    return M_HAIR_SEGMENTS;
}

HAIR_SEGMENT *Lara_Hair_GetSegment(const int32_t n)
{
    return &m_HairSegments[n];
}
