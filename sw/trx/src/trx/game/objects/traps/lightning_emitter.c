#include <trx/game/collision.h>
#include <trx/game/game.h>
#include <trx/game/lara.h>
#include <trx/game/matrix.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/viewport.h>

#define M_DAMAGE 400
#define M_STEPS 8
#define M_RND 64
#define M_SHOOTS 2

typedef struct {
    bool active;
    int32_t count;
    bool zapped;
    bool no_target;
    XYZ_32 target;
    int32_t start[M_SHOOTS];
    XYZ_32 end[M_SHOOTS];
    XYZ_32 main[M_STEPS];
    XYZ_32 wibble[M_STEPS];
    XYZ_32 shoot[M_SHOOTS][M_STEPS];
} M_PRIV;

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (Object_Get(item->object_id)->mesh_count > 1) {
        item->mesh_bits = 1;
        p->no_target = false;
    } else {
        p->no_target = true;
    }

    p->active = false;
    p->count = 1;
    p->zapped = false;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (!Item_IsTriggerActive(item)) {
        p->count = 1;
        p->active = false;
        p->zapped = false;

        if (Room_GetFlipStatus()) {
            Room_FlipMap();
        }

        Item_RemoveActive(item_num);
        item->status = IS_INACTIVE;
        return;
    }

    p->count--;
    if (p->count > 0) {
        return;
    }

    if (p->active) {
        p->active = false;
        p->count = 35 + (Random_GetControl() * 45) / 0x8000;
        p->zapped = false;
        if (Room_GetFlipStatus()) {
            Room_FlipMap();
        }
    } else {
        p->active = true;
        p->count = 20;

        for (int32_t i = 0; i < M_STEPS; i++) {
            p->wibble[i].x = 0;
            p->wibble[i].y = 0;
            p->wibble[i].z = 0;
        }

        const int32_t radius = p->no_target ? WALL_L : WALL_L * 5 / 2;
        if (Lara_IsNearItem(&item->pos, radius)) {
            const ITEM *const lara_item = Lara_GetItem();
            p->target.x = lara_item->pos.x;
            p->target.y = lara_item->pos.y;
            p->target.z = lara_item->pos.z;

            Lara_TakeDamage(M_DAMAGE, true);

            p->zapped = true;
        } else if (p->no_target) {
            const SECTOR *const sector =
                Room_GetSector(item->pos, &item->room_num);
            const int32_t h = Room_GetHeight(sector, item->pos);
            p->target.x = item->pos.x;
            p->target.y = h;
            p->target.z = item->pos.z;
            p->zapped = false;
        } else {
            p->target.x = 0;
            p->target.y = 0;
            p->target.z = 0;
            Collide_GetJointAbsPosition(
                item, &p->target, 1 + (Random_GetControl() * 5) / 0x7FFF);
            p->zapped = false;
        }

        for (int32_t i = 0; i < M_SHOOTS; i++) {
            p->start[i] = Random_GetControl() * (M_STEPS - 1) / 0x7FFF;
            p->end[i].x = p->target.x + (Random_GetControl() * WALL_L) / 0x7FFF;
            p->end[i].y = p->target.y;
            p->end[i].z = p->target.z + (Random_GetControl() * WALL_L) / 0x7FFF;

            for (int32_t j = 0; j < M_STEPS; j++) {
                p->shoot[i][j].x = 0;
                p->shoot[i][j].y = 0;
                p->shoot[i][j].z = 0;
            }
        }

        if (!Room_GetFlipStatus()) {
            Room_FlipMap();
        }
    }

    Sound_Effect(SFX_THUNDER, &item->pos, SPM_NORMAL);
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    const ITEM *const item = Item_Get(item_num);
    const M_PRIV *const p = item->priv;
    if (!p->zapped) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->hit_direction = 1 + (Random_GetControl() * 4) / (DEG_180 - 1);
    lara->hit_frame++;
    CLAMPG(lara->hit_frame, 34);
}

static void M_DrawBolts(const ITEM *const item)
{
    ANIM_FRAME *frmptr[2];
    int32_t rate;
    Item_GetFrames(item, frmptr, &rate);

    M_PRIV *const p = item->priv;
    if (!p->active) {
        return;
    }

    int32_t x1 = item->interp.result.pos.x + frmptr[0]->offset.x;
    int32_t y1 = item->interp.result.pos.y + frmptr[0]->offset.y;
    int32_t z1 = item->interp.result.pos.z + frmptr[0]->offset.z;

    int32_t x2 = p->target.x;
    int32_t y2 = p->target.y;
    int32_t z2 = p->target.z;

    int32_t dx = (x2 - x1) / M_STEPS;
    int32_t dy = (y2 - y1) / M_STEPS;
    int32_t dz = (z2 - z1) / M_STEPS;

    for (int32_t i = 0; i < M_STEPS; i++) {
        XYZ_32 *pos = &p->wibble[i];
        if (Game_IsPlaying()) {
            pos->x += (Random_GetDraw() - 0x4000) * M_RND / 0x8000;
            pos->y += (Random_GetDraw() - 0x4000) * M_RND / 0x8000;
            pos->z += (Random_GetDraw() - 0x4000) * M_RND / 0x8000;
        }
        if (i == M_STEPS - 1) {
            pos->y = 0;
        }

        x2 = x1 + dx + pos->x;
        y2 = y1 + dy + pos->y;
        z2 = z1 + dz + pos->z;

        if (i > 0) {
            Output_DrawLightningSegment((LIGHTNING_SEGMENT) {
                .from = { x1, y1 + p->wibble[i - 1].y, z1 },
                .to = { x2, y2, z2 },
                .thickness = Viewport_GetWidth(VIEWPORT_GAME) / 6 });
        } else {
            Output_DrawLightningSegment((LIGHTNING_SEGMENT) {
                .from = { x1, y1, z1 },
                .to = { x2, y2, z2 },
                .thickness = Viewport_GetWidth(VIEWPORT_GAME) / 6 });
        }

        x1 = x2;
        y1 += dy;
        z1 = z2;

        p->main[i].x = x2;
        p->main[i].y = y2;
        p->main[i].z = z2;
    }

    for (int32_t i = 0; i < M_SHOOTS; i++) {
        int32_t j = p->start[i];
        x1 = p->main[j].x;
        y1 = p->main[j].y;
        z1 = p->main[j].z;

        x2 = p->end[i].x;
        y2 = p->end[i].y;
        z2 = p->end[i].z;

        int32_t steps = M_STEPS - j;
        dx = (x2 - x1) / steps;
        dy = (y2 - y1) / steps;
        dz = (z2 - z1) / steps;

        for (int32_t k = 0; k < steps; k++) {
            XYZ_32 *pos = &p->shoot[i][k];
            if (Game_IsPlaying()) {
                pos->x += (Random_GetDraw() - 0x4000) * M_RND / 0x8000;
                pos->y += (Random_GetDraw() - 0x4000) * M_RND / 0x8000;
                pos->z += (Random_GetDraw() - 0x4000) * M_RND / 0x8000;
            }
            if (k == steps - 1) {
                pos->y = 0;
            }

            x2 = x1 + dx + pos->x;
            y2 = y1 + dy + pos->y;
            z2 = z1 + dz + pos->z;

            if (k > 0) {
                Output_DrawLightningSegment((LIGHTNING_SEGMENT) {
                    .from = { x1, y1 + p->shoot[i][k - 1].y, z1 },
                    .to = { x2, y2, z2 },
                    .thickness = Viewport_GetWidth(VIEWPORT_GAME) / 16 });
            } else {
                Output_DrawLightningSegment((LIGHTNING_SEGMENT) {
                    .from = { x1, y1, z1 },
                    .to = { x2, y2, z2 },
                    .thickness = Viewport_GetWidth(VIEWPORT_GAME) / 16 });
            }

            x1 = x2;
            y1 += dy;
            z1 = z2;
        }
    }
}

static bool M_Draw(const ITEM *const item)
{
    const OBJECT *const obj = Object_Get(O_LIGHTNING_EMITTER);
    ANIM_FRAME *frmptr[2];
    int32_t rate;
    Item_GetFrames(item, frmptr, &rate);

    Matrix_Push();
    Matrix_TranslateAbs32(item->interp.result.pos);
    Matrix_Rot16(item->interp.result.rot);
    const CLIP clip = Output_CheckBoundsClip(&frmptr[0]->bounds);
    if (clip == CLIP_NOT_VISIBLE) {
        Matrix_Pop();
        return false;
    }

    Output_CalculateObjectLighting(item, &frmptr[0]->bounds);

    Matrix_TranslateRel16(frmptr[0]->offset);
    Object_DrawMesh(obj->mesh_idx, clip, false);
    Matrix_Pop();

    M_DrawBolts(item);
    return true;
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->collision_func = M_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_flags = true;
}

REGISTER_OBJECT(O_LIGHTNING_EMITTER, M_Setup)
