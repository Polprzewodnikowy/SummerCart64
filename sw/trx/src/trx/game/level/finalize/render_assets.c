#include <trx/core/benchmark.h>
#include <trx/core/memory.h>
#include <trx/core/thread_pool.h>
#include <trx/core/utils.h>
#include <trx/game/level/finalize.h>
#include <trx/game/level/format/format.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>

#include <string.h>

static void M_FixTrapezoidRatios(FACE *const face, const XYZ_16 vertices[4])
{
    // This function attempts to correct texture coordinate ratios for a
    // quadrilateral so the GPU, which typically renders a quad as two
    // triangles, does not warp the texture disproportionately across the quad's
    // diagonal divisions. By comparing the 3D edge lengths (in world space)
    // with the corresponding 2D UV edge lengths, it computes a scale factor
    // that preserves the trapezoidal shape in texture space and allows the
    // shader to warp the texture uniformly across all four corners.
    //
    // In many software rasterization or older GPU pipelines, triangles can get
    // rendered using affine interpolation of texture coordinates, causing
    // visible warping when a four-sided polygon is split internally.
    // The original approach (coded by XProger) handled only rectangular UV
    // maps by simply scaling the edges; this updated version takes into
    // account the actual UV trapezoid to achieve a more uniform texture
    // projection.

    // 1) Gather the coordinate and texture information
    const OBJECT_TEXTURE *const tex =
        Output_GetObjectTexture(face->texture_idx);
    const TEXTURE_UV *uvs[4] = {
        &tex->uv[0],
        &tex->uv[1],
        &tex->uv[2],
        &tex->uv[3],
    };
    TEXTURE_ZW_F *zws[4] = {
        &face->texture_zw[0],
        &face->texture_zw[1],
        &face->texture_zw[2],
        &face->texture_zw[3],
    };
    XYZ_F c0, c1, c2, c3, *coords[4] = { &c0, &c1, &c2, &c3 };
    for (int32_t i = 0; i < 4; i++) {
        coords[i]->x = vertices[i].x;
        coords[i]->y = vertices[i].y;
        coords[i]->z = vertices[i].z;
    }

    // 2) Compute geometric edges
    //    a = c0-c1, b = c3-c2, c = c0-c3, d = c1-c2
    const XYZ_F a = XYZ_F_Subtract(c0, c1);
    const XYZ_F b = XYZ_F_Subtract(c3, c2);
    const XYZ_F c = XYZ_F_Subtract(c0, c3);
    const XYZ_F d = XYZ_F_Subtract(c1, c2);

    const float a_l = XYZ_F_Length(a);
    const float b_l = XYZ_F_Length(b);
    const float c_l = XYZ_F_Length(c);
    const float d_l = XYZ_F_Length(d);

    // 3) Compute dot-products in 3D to see which edges differ more
    const float ab = XYZ_F_DotProduct(a, b) / (a_l * b_l);
    const float cd = XYZ_F_DotProduct(c, d) / (c_l * d_l);

    // 4) Compute tx, ty in for orientation
    const float tx = ABS(uvs[0]->u - uvs[3]->u);
    const float ty = ABS(uvs[0]->v - uvs[3]->v);

    // 5) Measure the same edges in UV space so we know the "current" shape
    XYZ_F uv0 = { uvs[0]->u, uvs[0]->v, 0.0f };
    XYZ_F uv1 = { uvs[1]->u, uvs[1]->v, 0.0f };
    XYZ_F uv2 = { uvs[2]->u, uvs[2]->v, 0.0f };
    XYZ_F uv3 = { uvs[3]->u, uvs[3]->v, 0.0f };

    // au = uv0 - uv1, bu = uv3 - uv2, cu = uv0 - uv3, du = uv1 - uv2
    const XYZ_F au = XYZ_F_Subtract(uv0, uv1);
    const XYZ_F bu = XYZ_F_Subtract(uv3, uv2);
    const XYZ_F cu = XYZ_F_Subtract(uv0, uv3);
    const XYZ_F du = XYZ_F_Subtract(uv1, uv2);

    const float au_l = XYZ_F_Length(au);
    const float bu_l = XYZ_F_Length(bu);
    const float cu_l = XYZ_F_Length(cu);
    const float du_l = XYZ_F_Length(du);

    // We'll reuse the same ab/cd dot logic in UV if needed, but typically
    // we only need the lengths to find the ratio vs. geometry.

    // 6) Figure out the correction ratios per-corner, taking care of both
    //    geometry and UV mesh proportions
    if (ab > cd) {
        const int k = (tx > ty) ? 1 : 0; // pick axis
        if (a_l > b_l) {
            // geometry ratio = (b_l / a_l)
            // uv ratio       = (bu_l / au_l)  (avoid /0 check if needed)
            const float geom_ratio = (a_l > 1e-6f) ? (b_l / a_l) : 1.0f;
            const float uv_ratio = (au_l > 1e-6f) ? (bu_l / au_l) : 1.0f;
            const float fix = geom_ratio / uv_ratio; // final scale
            zws[2]->zw[k] = fix;
            zws[3]->zw[k] = fix;
        } else if (a_l < b_l) {
            const float geom_ratio = (b_l > 1e-6f) ? (a_l / b_l) : 1.0f;
            const float uv_ratio = (bu_l > 1e-6f) ? (au_l / bu_l) : 1.0f;
            const float fix = geom_ratio / uv_ratio;
            zws[0]->zw[k] = fix;
            zws[1]->zw[k] = fix;
        }
    } else if (ab < cd) {
        const int k = (tx > ty) ? 0 : 1; // pick axis
        if (c_l > d_l) {
            const float geom_ratio = (c_l > 1e-6f) ? (d_l / c_l) : 1.0f;
            const float uv_ratio = (cu_l > 1e-6f) ? (du_l / cu_l) : 1.0f;
            const float fix = geom_ratio / uv_ratio;
            zws[1]->zw[k] = fix;
            zws[2]->zw[k] = fix;
        } else if (c_l < d_l) {
            const float geom_ratio = (d_l > 1e-6f) ? (c_l / d_l) : 1.0f;
            const float uv_ratio = (du_l > 1e-6f) ? (cu_l / du_l) : 1.0f;
            const float fix = geom_ratio / uv_ratio;
            zws[0]->zw[k] = fix;
            zws[3]->zw[k] = fix;
        }
    }
}

static void M_PremultiplyTexturePage(void *userdata)
{
    const int32_t page = *(int32_t *)userdata;
    Output_LockTexturePage32(page);
    RGBA_8888 *ptr = Output_GetTexturePage32(page);
    const float inv255 = 1.0f / 255.0f;
    for (int32_t i = 0; i < TEXTURE_PAGE_SIZE; i++, ptr++) {
        ptr->r *= ptr->a * inv255;
        ptr->g *= ptr->a * inv255;
        ptr->b *= ptr->a * inv255;
    }
    Output_UnlockTexturePage32(page);
}

static void M_UpdateReflectivity(OBJECT_MESH *const mesh, FACE *const face)
{
    const OBJECT_TEXTURE *const texture =
        Output_GetObjectTexture(face->texture_idx);
    const bool reflective = texture->draw_type == DRAW_REFLECTIVE_OPAQUE
        || texture->draw_type == DRAW_REFLECTIVE_BLEND_ADD;
    face->enable_reflections |= reflective;
    mesh->enable_reflections |= reflective;
}

void Level_Finalize_LoadTextures(LEVEL_CONTEXT *const ctx)
{
    for (int32_t room_num = 0; room_num < Room_GetCount(); room_num++) {
        const ROOM *const room = Room_Get(room_num);
        for (int32_t j = 0; j < room->mesh.face3s.count; j++) {
            const FACE *const face = &room->mesh.face3s.data[j];
            OBJECT_TEXTURE *const texture =
                Output_GetObjectTexture(face->texture_idx);
            texture->uv_count = 3;
        }
    }

    for (int32_t i = 0; i < Object_GetMeshCount(); i++) {
        OBJECT_MESH *const mesh = Object_GetMesh(i);
        for (int32_t j = 0; j < mesh->tex_face3s.count; j++) {
            FACE *const face = &mesh->tex_face3s.data[j];
            OBJECT_TEXTURE *const texture =
                Output_GetObjectTexture(face->texture_idx);
            texture->uv_count = 3;
            M_UpdateReflectivity(mesh, face);
        }
    }

    for (int32_t room_num = 0; room_num < Room_GetCount(); room_num++) {
        ROOM *const room = Room_Get(room_num);
        for (int32_t j = 0; j < room->mesh.face4s.count; j++) {
            FACE *const face = &room->mesh.face4s.data[j];
            XYZ_16 vertices[4] = {
                room->mesh.vertices[face->vertices[0]].pos,
                room->mesh.vertices[face->vertices[1]].pos,
                room->mesh.vertices[face->vertices[2]].pos,
                room->mesh.vertices[face->vertices[3]].pos,
            };
            M_FixTrapezoidRatios(face, vertices);
        }
    }

    for (int32_t i = 0; i < Object_GetMeshCount(); i++) {
        OBJECT_MESH *const mesh = Object_GetMesh(i);
        for (int32_t j = 0; j < mesh->tex_face4s.count; j++) {
            FACE *const face = &mesh->tex_face4s.data[j];
            XYZ_16 vertices[4] = {
                mesh->vertices[face->vertices[0]],
                mesh->vertices[face->vertices[1]],
                mesh->vertices[face->vertices[2]],
                mesh->vertices[face->vertices[3]],
            };
            M_FixTrapezoidRatios(face, vertices);
            M_UpdateReflectivity(mesh, face);
        }
    }
}

void Level_Finalize_LoadTexturePages(LEVEL_CONTEXT *const ctx)
{
    BENCHMARK benchmark = Benchmark_Start();
    const LEVEL_FORMAT_LOADER *const loader = ctx->loader;
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    const int32_t num_pages = info->textures.page_count;
    Output_InitialiseTexturePages(num_pages, loader->game_version >= 2);

    for (int32_t i = 0; i < num_pages; i++) {
        if (loader->game_version >= 2) {
            uint8_t *const target_8 = Output_GetTexturePage8(i);
            const uint8_t *const source_8 =
                &info->textures.pages_8[i * TEXTURE_PAGE_SIZE];
            memcpy(target_8, source_8, TEXTURE_PAGE_SIZE * sizeof(uint8_t));
        }

        const RGBA_8888 *const source_32 =
            &info->textures.pages_32[i * TEXTURE_PAGE_SIZE];
        RGBA_8888 *const target_32 = Output_GetTexturePage32(i);
        memcpy(target_32, source_32, TEXTURE_PAGE_SIZE * sizeof(RGBA_8888));
    }
    Benchmark_End(&benchmark, "copied texture data");

    {
        int32_t *pages = Memory_Alloc(num_pages * sizeof(int32_t));
        THREAD_POOL *const pool = ThreadPool_Create(-1);
        for (int32_t i = 0; i < num_pages; i++) {
            pages[i] = i;
        }
        for (int32_t i = 0; i < num_pages; i++) {
            ThreadPool_AddJob(pool, M_PremultiplyTexturePage, &pages[i]);
        }
        ThreadPool_Wait(pool);
        ThreadPool_Destroy(pool);
        Memory_Free(pages);
    }
    Benchmark_End(&benchmark, "premultiplied alpha");

    Memory_FreePointer(&info->textures.pages_8);
    Memory_FreePointer(&info->textures.pages_32);
    Benchmark_End(&benchmark, nullptr);
}

void Level_Finalize_LoadPalettes(LEVEL_CONTEXT *const ctx)
{
    LEVEL_CONTEXT_INFO *const info = &ctx->info;
    Output_InitialisePalettes(
        info->palette.size, info->palette.data_24, info->palette.data_32);
    Memory_FreePointer(&info->palette.data_24);
    Memory_FreePointer(&info->palette.data_32);
}
