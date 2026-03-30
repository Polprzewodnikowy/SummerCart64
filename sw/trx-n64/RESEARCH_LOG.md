# Research Log — Techniques from Other N64 Projects

## lambertjamesd/n64graphicsdemo (libultra + F3DEX2)
Source: https://github.com/lambertjamesd/n64graphicsdemo

### Borrowable Techniques

1. **Pre-baked light falloff texture**
   - 64x64 I8 radial gradient baked to ROM
   - CPU projects UV coords per-vertex to sample falloff
   - RDP multiplies via TEXEL0 x SHADE combiner
   - Could replace our per-vertex CPU fog lerp (3 muls + 3 adds/vertex)
   - File: `realtimeshadows/src/models/point_light_rendered_tex.inc.h`

2. **CI8 palette-as-color-transform (toon shading)**
   - Render to I8 buffer, palette lookup transforms colors
   - Since we already use CI8 + TLUT, could use custom palette for
     lighting tiers — quantize distance to palette index, TLUT does grading
   - File: `palletecb/src/graphics/pallete_mapping.c`

3. **F3DEX2 performance benchmarks (reference data)**
   - gsDPLoadBlock: 0.00772 us/byte
   - gsSPMatrix MUL: 4.134 us
   - gsSPVertex (32 verts): ~12 us
   - Confirms our RDP has massive unused headroom
   - File: `performancetesting/README.md`

4. **Shadow map via offscreen render (64x64 CI8)**
   - Render object from light POV into tiny offscreen buffer
   - Project back as textured quad for ground shadow
   - Could replace our 16-point circle shadow with proper projected shadow
   - File: `realtimeshadows/src/scene/shadow_map.c`

### Not Applicable
- F3DEX2 microcode (libultra, not libdragon)
- Display list construction (Gfx[] arrays, gSP*/gDP* macros)
- RSP vertex T&L (F3DEX2-specific, no equivalent in rdpq)
- Matrix stack (RSP-side in F3DEX2)

### Key Insight
Confirms standard N64 path: RSP does ALL vertex transform + clipping.
CPU only builds matrices and per-vertex attributes. Our CPU-projection
bottleneck (40-60ms) is fundamentally wrong for N64 — need RSP vertex
processing via T3D, GL, or custom overlay.

---

## lambertjamesd/n64brew2023 — Megatextures (libultra + F3DEX2)
Source: https://github.com/lambertjamesd/n64brew2023

### Borrowable Techniques

1. **Tile cache with LRU eviction + pre-built display lists**
   - Each 32×32 texture tile gets a small pre-built DL that loads it into TMEM
   - LRU doubly-linked list + hash table for O(1) lookup by ROM address
   - Rate-limited: max 56 tile DMA requests per frame
   - Fallback to coarser LOD on cache miss (walks up mip chain)
   - **Directly applicable to our CI8 ROM cache** — same pattern:
     small cache, DMA from ROM on miss, keep pre-built surface_t per slot
   - File: `src/megatextures/megatexture_tilecache.c`

2. **Adaptive LOD bias (runtime quality slider)**
   - `gMtLodBias` auto-increases when too many tiles requested or DL overflows
   - Slowly reduces when spare capacity available
   - Prevents frame drops by degrading quality smoothly
   - **Could apply to our face/tri budgets**: auto-raise NEAR_FACE_BUDGET
     when RSP has headroom, auto-lower when CPU-bound
   - File: `src/megatextures/megatexture_renderer.c`

3. **Vertex sharing via careful ordering at build time**
   - Build tool reorders vertices so shared verts between adjacent tiles
     appear at start/end of each tile's vertex list
   - Runtime shares vertex buffer slots across adjacent geometry
   - **Relevant if we batch-submit room geometry** — sort faces so adjacent
     faces share vertex indices, reducing cache thrashing
   - File: `tools/export_level/megatexture.lua` (`determine_vertex_mapping`)

4. **Retroactive display list patching**
   - Write placeholder gSPVertex command, fill in triangles, then go back
     and patch the vertex count once known
   - Avoids needing to pre-count vertices before submission
   - **Pattern translatable to rspq**: could batch rdpq_triangle calls
     and retroactively fix up a flush point

5. **Preloading lowest LOD at init time**
   - Smallest mip levels (entire surface in a few tiles) loaded at startup
   - Guarantees there's always a visual fallback, never void
   - **We already do this for baked room tiles** — same philosophy

6. **No Z-buffer for distant geometry + painter's algorithm**
   - Megatexture surfaces use `G_RM_AA_OPA_SURF` (no Z test)
   - CPU sorts surfaces back-to-front via merge sort
   - **Matches our far pass** (no zbuf, back-to-front room sort)

### Not Applicable
- Megatexture UV-space culling (specific to flat planar surfaces)
- F3DEX2 vertex pipeline (libultra)
- Ear-clipping triangulation (we use pre-triangulated TR1 meshes)

### Megatextures Deep Dive — CI8 Cache Blueprint

**Tile cache struct** (`megatexture_tilecache.h`):
```c
struct MTTileCacheEntry {
    u16 newerTile, olderTile;  // doubly-linked LRU list
    u16 nextHashTile;          // hash chain collision list
    u16 hashTableIndex;        // which bucket (MT_NO_TILE_INDEX if not hashed)
    void* romAddress;          // ROM address = identity key
};
```

**Hash function**: `(LARGE_PRIME * (romAddr >> 11)) & mask`
- Right-shift by 11 drops always-zero bits (tiles are 2048-byte aligned)
- Hash table 2x overprovisioned (reduces collisions)

**Cache lookup** — O(1) average:
```c
Gfx* mtTileCacheSearch(cache, romAddress, hashIndex) {
    int idx = cache->hashTable[hashIndex];
    while (idx != MT_NO_TILE_INDEX) {
        if (entries[idx].romAddress == romAddress) {
            markMostRecent(idx);  // promote in LRU
            return &tileLoaders[idx * MT_GFX_SIZE];  // pre-built DL
        }
        idx = entries[idx].nextHashTile;
    }
    return NULL;  // miss
}
```

**On cache miss**: evict LRU entry, async DMA from ROM:
```c
dmaIoMesg->dramAddr = &tileData[entryIndex * MT_TILE_WORDS];
dmaIoMesg->devAddr  = (u32)romAddress;
dmaIoMesg->size     = 2048;  // 32×32 RGBA16
osEPiStartDma(piHandle, dmaIoMesg, OS_READ);
```
- 64-slot circular message queue for async DMA
- Max 56 DMA transfers per frame (`gMtMaxTileRequestsPerFrame`)
- All DMAs collected at frame end before RSP submission

**Eviction**: strict LRU with per-frame fence
- `oldestTileFromFrame[1]` = oldest tile used last frame
- Won't evict past this fence (prevents evicting active tiles)
- Two-frame rolling window rotated each frame

**Fallback on miss**: walks up mip chain (coarser LOD) looking for
any cached tile. Ultimate fallback: `gsSPEndDisplayList` (no-op).

**Pre-built display list per slot** (10 commands, built at init):
```
gDPPipeSync → gDPTileSync → gDPSetTextureImage(slot_buffer) →
gDPSetTile → gDPLoadSync → gDPLoadBlock(1024 texels) →
gDPPipeSync → gDPSetTile(render) → gDPSetTileSize → gSPEndDisplayList
```
Only last 2 commands patched on tile reassignment (shift + offset).

**Our CI8 adaptation**:
| Megatextures (RGBA16) | Our TR1 CI8 cache |
|---|---|
| 2048 entries × 2KB = 4MB | 128 entries × 1KB = 128KB |
| Hash key = ROM address | Hash key = texture index (0-896) |
| 10-cmd display list per slot | surface_t per slot (for rdpq_tex_upload) |
| Coarser LOD fallback | Use last-bound texture (no mipmaps in TR1) |
| osEPiStartDma (async) | dma_read (libdragon) |
| osInvalDCache at init | malloc_uncached (no cache issues) |

---

## lambertjamesd/n64brew2025 — "Junk Runner 64" (libdragon + T3D) ★★★
Source: https://github.com/lambertjamesd/n64brew2025

**This is the most relevant project found so far.** Same SDK (libdragon),
uses T3D for RSP vertex processing. Directly applicable architecture.

### Borrowable Techniques

1. **T3D for RSP vertex T&L** ★★★ (THE key finding)
   - ALL vertices transformed on RSP via T3D microcode, zero CPU projection
   - `t3d_vert_load()` loads packed vertices → RSP transforms → `t3d_tri_draw()`
   - CPU only builds matrices (`t3d_mat4_to_fixed_3x4()`)
   - T3D handles clipping on RSP (no CPU Sutherland-Hodgman needed)
   - **This is the direct solution to our 40-60ms CPU bottleneck**
   - Files: `src/render/tmesh.c`, `src/render/render_batch.c`

2. **rspq_block pre-recording** ★★★
   - Entire mesh draw sequence (vert loads, matrix, tris, materials)
     recorded into `rspq_block_t` at LOAD TIME
   - At render time: single `rspq_block_run()` replays everything
   - Near-zero CPU overhead per draw call
   - Materials also pre-recorded into blocks
   - **We could pre-record room geometry as rspq_blocks**
   - Files: `src/render/tmesh.c:156-224`, `src/render/material.c:199-388`

3. **Build-time vertex cache optimization**
   - Sophisticated vertex batch algorithm maximizes RSP cache reuse
   - Tracks vertex lifetimes across batches
   - Max batch: 64 vertices (T3D RSP buffer size)
   - Greedy chunk ordering minimizes material switch cost
   - **Applicable if we adopt T3D — our room meshes need this**
   - File: `tools/mesh_export/entities/tiny3d_mesh_writer.py:152-314`

4. **Material delta transitions**
   - Build tool computes state diffs between materials
   - Only changed RDP state is emitted at transitions
   - Estimated costs: combine mode=0.395, blend mode=0.751
   - **Reduces near pass texture/mode switch overhead**
   - File: `tools/mesh_export/entities/material_delta.py`

5. **Camera-relative rendering for fixed-point**
   - Subtract camera position BEFORE converting to fixed-point
   - Prevents fixed-point overflow for large worlds (TR1 has big rooms)
   - `render_batch_relative_mtx()` in render_batch.c:57-61
   - **Critical if we adopt T3D — TR1 world coords are large**

6. **Frame memory bump allocator (double-buffered)**
   - 64 KB per-frame pool for matrices, poses, temporaries
   - `UncachedAddr()` for all RSP-visible writes (no cache coherency bugs)
   - **We need this for per-frame T3D matrix data**
   - File: `src/render/frame_alloc.h`

7. **4-direction front-to-back sort variants**
   - LOD meshes exported in 4 variants (+X, -X, +Z, -Z)
   - Triangles sorted front-to-back per direction for early-Z rejection
   - Correct variant selected at runtime by camera direction
   - **Advanced optimization for later — improves Z-buffer efficiency**
   - File: `tools/mesh_export/entities/overworld.py:187-192`

8. **Dithered Z-buffer clear**
   - Bayer-dithered depth clear (not flat 0xFFFF)
   - Creates depth offset that helps LOD transitions blend
   - **Could help our near/far pass Z-fighting at distance transitions**
   - File: `src/render/z_clear.c`

9. **Tile-based streaming with load/unload queue**
   - 4×4 window of loaded tiles, DMA on demand
   - Unload waits for `rspq_wait()` to ensure RSP is done
   - **Architecture for room-scoped geometry loading**
   - File: `src/overworld/overworld_load.c:20-71`

10. **Placeholder textures for animation swap**
    - `surface_make_placeholder_linear()` + `rdpq_set_lookup_address()`
    - Swap texture data without re-recording material block
    - **Useful for animated TR1 textures (water, lava)**
    - File: `src/render/material.c:159-166`

### Architecture Comparison

| Aspect | Our Renderer | Junk Runner 64 |
|--------|-------------|----------------|
| Vertex transform | CPU (40-60ms) | RSP via T3D (~1ms) |
| Triangle submit | rdpq_triangle | t3d_tri_draw (RSP) |
| Clipping | CPU S-H | T3D RSP microcode |
| Batching | Per-face | Pre-recorded rspq_blocks |
| RSP utilization | ~0% | Full pipeline |
| Material switches | Per-face upload | Delta transitions |

### Key Insight
**T3D is the answer.** This project proves that libdragon + T3D achieves
full RSP vertex processing with the same SDK we're already using. The
migration path: convert TR1 room meshes to T3DVertPacked format, build
matrices per room, let T3D handle transform+clip+project on RSP.

Our CPU would go from spending 40ms on vertex math to spending ~1ms
building matrices. The RSP does the rest.

### T3D Deep Dive — Migration Blueprint

**Init** (3 lines):
```c
rdpq_init();
t3d_init((T3DInitParams){});
```

**Vertex format (T3DVertPacked = 32 bytes per 2 vertices)**:
```
Offset 0:  Pos A (3× int16 big-endian, ×128 fixed-point scale)
Offset 6:  Normal A (packed 16-bit: 5-6-5 bit fields)
Offset 8:  Pos B (3× int16)
Offset 14: Normal B (packed 16-bit)
Offset 16: Color A (RGBA8888)
Offset 20: Color B (RGBA8888)
Offset 24: UV A (2× uint16, 10.5 fixed point)
Offset 28: UV B (2× uint16)
```
Vertices live in cached RDRAM, flushed once via data_cache_hit_writeback.

**Per-frame render sequence**:
1. `t3d_screen_clear_depth()` + `rdpq_set_color_image/z_image`
2. `t3d_frame_start()`
3. `t3d_light_set_ambient()` / `t3d_light_set_count()`
4. Build viewport: `t3d_viewport_create()` + camera matrices
5. `t3d_viewport_attach(viewport)`
6. For each object: build float matrix → subtract camera pos →
   `t3d_mat4_to_fixed_3x4()` → `t3d_matrix_push()` → `rspq_block_run(mesh)`
7. `t3d_matrix_pop()`
8. `rdpq_detach_show()`

**rspq_block recording** (at load time):
```c
rspq_block_begin();
  t3d_vert_load(vertices, offset, count);  // DMA verts to RSP
  t3d_tri_draw(i0, i1, i2);               // triangle from RSP cache
  t3d_tri_sync();                          // flush tri pipe
  material_apply(transition_mat);          // mid-mesh tex change
  t3d_matrix_push(&armature[bone_idx]);    // bone via segment
block = rspq_block_end();
// Replay: rspq_block_run(block);  // near-zero CPU cost
```

**Skeleton/bone animation for Lara**:
- CPU builds float world matrices per bone (`armature_build_pose`)
- Converts to fixed-point: `t3d_mat4_to_fixed()`
- Writes to UncachedAddr frame-pool memory
- Sets segment: `t3d_segment_set(T3D_SEGMENT_SKELETON, pose_fp)`
- Block references bones via segment-relative addressing
- Bone attachments (e.g., guns in Lara's hands): chain of 3 matrices
  pushed to T3D stack: object → bone → local offset

**Camera-relative rendering** (precision trick):
- Camera position ZEROED in view matrix
- Camera pos subtracted from each object's model matrix translation
- Prevents fixed-point overflow for large TR1 world coordinates

**Memory pattern**:
- Vertex data: cached RDRAM, writeback once at load
- Transform matrices: per-frame bump allocator (64KB double-buffered)
- All RSP-visible writes via UncachedAddr()

**What T3D does (RSP)**: vertex transform, clipping, lighting, fog,
triangle rasterization setup, matrix stack, segment addressing.

**What CPU still does**: frustum culling, bone hierarchy, float→fixed
conversion, render sorting, animation, camera math, material tracking.

---

---

## lambertjamesd/spellcraft — RPG (libdragon + T3D) ★★
Source: https://github.com/lambertjamesd/spellcraft

Same author as n64brew2025. Confirms T3D patterns with additional techniques.

### Borrowable Techniques (beyond what n64brew2025 already showed)

1. **Sprite/resource reference-counted cache**
   - Materials, sprites, tmeshes use ref-counted caches
   - Shared resources across tiles/rooms loaded once
   - **Applicable to our CI8 texture cache and baked tile management**
   - File: `src/resource/resource_cache.h`

2. **2D frustum footprint scan for tile visibility**
   - Projects frustum NDC corners back to world XZ
   - Builds convex hull, rasterizes row-by-row to find visible tiles
   - **Much cheaper than per-face frustum tests** — could use for room
     visibility before doing per-portal checks
   - File: `src/overworld/overworld_render.c`

3. **Seek-based tile loading from persistent file handle**
   - Overworld file stays open, tiles loaded via fseek() to offsets
   - No per-tile file open/close overhead
   - **Pattern for ROM-streaming room data: keep file handle open,
     seek to room offset, DMA on demand**
   - File: `src/overworld/overworld_load.c`

4. **Unload queue with rspq_wait() safety**
   - Defer freeing tile memory until after `rspq_wait()` confirms
     RSP is done with it
   - **We need this for any runtime texture/geometry eviction**
   - File: `src/overworld/overworld_load.c` (UNLOAD_QUEUE_SIZE=4)

5. **Indoor room management: show/hide up to 4 rooms**
   - `scene_show_room()` / `scene_hide_room()`, MAX_LOADED_ROOM=4
   - **Similar to our ring-1 near rooms (current + connected)**
   - File: `src/scene/scene.c`

### Confirms from n64brew2025
- T3D vertex pipeline (t3d_vert_load → t3d_tri_draw)
- rspq_block pre-recording for mesh + material
- Camera-relative transforms (subtract cam pos from model matrix)
- Frame bump allocator (64KB double-buffered, UncachedAddr)
- Build-time vertex optimization (adjacency batching, lifetime tracking)
- Material sort-key batching (priority, material ptr, type)

### Key Insight
Every Lambert project using libdragon uses T3D for vertex processing.
Three projects (n64graphicsdemo, n64brew2023, n64brew2025/spellcraft)
span libultra→libdragon migration. The libdragon ones ALL use T3D.
This is the established pattern for 3D on modern libdragon.

---

## mwpenny/portal64-still-alive — Portal on N64 (libultra + F3DEX2) ★★
Source: https://github.com/mwpenny/portal64-still-alive

### Borrowable Techniques

1. **Doorway frustum narrowing** ★★★ (directly maps to TR1 portals)
   - For each visible doorway/portal: construct 4 clipping planes from
     camera position through portal corners → tighter frustum
   - Recursively visit adjacent rooms with narrowed frustum
   - Only renders geometry that's actually visible THROUGH the portal
   - **Our current approach**: mark all connected rooms visible regardless.
     This technique would reduce visible geometry significantly.
   - File: `src/levels/static_render.c:119-166`, `src/scene/camera.c:8-52`

2. **Pre-computed room non-visibility bitmask**
   - `nonVisibleRooms` u64 bitmask per room, baked at build time
   - Fast bitwise rejection: rooms that can NEVER be seen from a given
     room are masked out before portal traversal
   - **TR1 has similar room data** — could generate equivalent masks
   - File: `tools/level_scripts/room_export.lua`

3. **4-bit twoTone textures** ★★ (massive TMEM savings)
   - Store wall textures as 4-bit intensity (grayscale)
   - Use color combiner to lerp between two colors via the texture:
     `PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT` (cycle 1)
     `COMBINED × SHADE` (cycle 2)
   - Result: full-color textured walls at 4bpp instead of 16bpp
   - A 32×32 4-bit tile = 512 bytes TMEM (vs 2 KB for RGBA16, 1 KB for CI8)
   - **Could halve our TMEM usage for room textures**
   - File: `assets/materials/static.skm.yaml`

4. **Per-room BVH with skip pointers (build-time)**
   - Axis-aligned BVH built at build time, serialized as flat array
   - `siblingOffset` skip pointers for efficient frustum traversal
   - O(log n) per-room geometry culling vs our O(n) face loop
   - File: `tools/level_scripts/static_export.lua:257-317`

5. **Baked vertex lighting from Blender**
   - Python script bakes ambient volumes + point light contributions
     directly into vertex colors at build time
   - Runtime: zero lighting cost — just `SHADE` in combiner
   - **TR1 has per-vertex light values** — we already use them, but
     could pre-bake fog+lighting into vertex colors at load time
   - File: `tools/blender/bake_lighting.py`

6. **Material sort key: (material << 23 | distance)**
   - Simple integer sort key groups by material first, distance second
   - Minimizes state changes during rendering
   - **Applicable to our near pass** — sort faces by texture index
     to reduce CI8 upload frequency
   - File: `src/graphics/render_scene.c`

7. **Z-buffer partitioning for nested views (no stencil)**
   - Splits Z range [0, G_MAXZ] proportionally across portal views
   - Each view gets its own depth range, preventing Z-fighting
   - **Could use for our near/far pass Z-fighting at distance transitions**
   - File: `src/scene/render_plan.c:430-491`

8. **ROM-loaded dynamic assets with pointer fixup**
   - Models loaded on-demand from ROM via `romCopy()`
   - Display list pointers patched at load time for VTX/SETTIMG/DL addresses
   - **Pattern for ROM-streaming TR1 mesh data**
   - File: `src/scene/dynamic_asset_loader.c`

### Not Applicable
- F3DEX2 microcode (libultra)
- Shadow volume rendering (too expensive for TR1 scene complexity)
- Recursive portal rendering (TR1 portals are one-way doorways, not see-through)

### Key Insight
The doorway frustum narrowing is the single most impactful visibility
technique we're missing. Our current "all connected rooms visible"
approach renders ~3x more geometry than needed. Constructing per-portal
frustums and only recursing through visible portals would dramatically
reduce face count before the vertex cache even runs.

---

## HailToDodongo/tiny3d — T3D Library Internals ★★★
Source: https://github.com/HailToDodongo/tiny3d

### T3DVertPacked Struct (32 bytes = 2 vertices, 16 bytes/vert)
```c
typedef struct {
  int16_t posA[3];    // 0x00: position (integer part of s16.16)
  uint16_t normA;     // 0x06: packed 5.6.5 normal
  int16_t posB[3];    // 0x08: position B
  uint16_t normB;     // 0x0E: normal B
  uint32_t rgbaA;     // 0x10: RGBA8888 color A
  uint32_t rgbaB;     // 0x14: RGBA8888 color B
  int16_t stA[2];     // 0x18: UV A (10.5 fixed-point PIXEL coords)
  int16_t stB[2];     // 0x1C: UV B
} __attribute__((aligned(8))) T3DVertPacked;
```
UVs are pixel coords × 32 (10.5 fixed point). NOT normalized 0-1.

### Key Specs
- **Vertex cache**: 70 transformed vertices in RSP DMEM (36 bytes each)
- **Clipping**: Sutherland-Hodgman on RSP, lazy-loaded overlay, 5 planes
- **Guard band**: default 2× screen (most offscreen tris skip clipping)
- **Perf reference**: ~8.8ms RSP for 3642 verts / ~2326 tris

### Borrowable Techniques

1. **Alpha-to-tile for per-triangle texture selection** ★★★
   - `t3d_state_set_alpha_to_tile(true)` — 3 MSBs of vertex alpha
     select which TMEM tile (0-7) to use for that triangle
   - Pack up to 8 textures into TMEM simultaneously
   - Select per-triangle with ZERO texture switch overhead
   - "Completely free and adds no extra time"
   - **For TR1**: pack 2-4 most-used room textures into TMEM at once,
     encode tile index in vertex alpha. Eliminates most CI8 uploads
     mid-frame. With CI8 at 1KB/tile, 4 tiles = 4KB = full TMEM.
   - File: `src/t3d/t3d.h:568-585`

2. **Minimal init + rdpq interop pattern**
   ```c
   rdpq_init();
   t3d_init((T3DInitParams){});
   // Vertices MUST be malloc_uncached (or data_cache_hit_writeback)
   // rdpq_tex_upload works alongside t3d_tri_draw
   // MUST call t3d_tri_sync() before any rdpq state changes
   ```

3. **Optimal matrix stack for many objects at same level**
   ```c
   t3d_matrix_push_pos(1);       // advance once
   for each object:
     t3d_matrix_set(mat, true);  // multiply with parent
     t3d_vert_load / t3d_tri_draw
   t3d_matrix_pop(1);            // pop once
   ```
   **For TR1 rooms**: push room matrix once, draw all room geometry,
   pop. For Lara: push per-bone via segment addressing.

4. **Triangle strips save ~45% bandwidth**
   - `t3d_tri_draw_strip(indexBuff, count)` vs individual tri_draw
   - TR1 quads naturally form strips (each quad = 2 tris sharing an edge)
   - Build tool converts to strip format: `t3d_indexbuffer_convert()`
   - File: `docs/modelOpt.md`

5. **Vertex FX: spherical UV and UV offset on RSP**
   - `T3D_VERTEX_FX_SPHERICAL_UV` — environment mapping
   - `T3D_VERTEX_FX_UV_OFFSET` — UV scrolling (water/lava animation)
   - Both computed on RSP, zero CPU cost
   - File: `src/t3d/t3d.h:587-604`

6. **Model BVH for frustum culling**
   - `t3d_model_bvh_query_frustum()` — test model against frustum
   - Built at export time with `--bvh` flag
   - Returns which chunks are visible
   - File: `src/t3d/t3dmodel.h`

7. **Dynamic vertex replacement via segment**
   - `t3d_model_make_object_vert_placeholder(model, obj, segId)`
   - At runtime: `t3d_segment_set(segId, newVertices)`
   - Swap vertex data without re-recording rspq_block
   - **For TR1**: could swap between room vertex sets for animated rooms

### TR1 Migration Specifics
- Each TR1 room vertex (int16 x,y,z + int16 light) maps to T3DVertPacked
  with position = x,y,z and rgbaA = light-derived vertex color
- UV from tex_uv_map → 10.5 fixed-point pixel coords
- 70-vert RSP cache fits ~2 TR1 room batches (35 verts each)
- With alpha-to-tile: pack 4 CI8 textures (4KB TMEM total), select
  per-triangle via vertex alpha. Reduces texture switches from ~48/frame
  to ~12/frame (one switch per 4-texture group change)

---

## HailToDodongo/pyrite64 — Game Engine (libdragon + T3D) ★★
Source: https://github.com/HailToDodongo/pyrite64

Full N64 game engine + visual editor by the T3D author.

### Borrowable Techniques

1. **BigTex deferred UV rendering** ★★★ (breakthrough for large textures)
   - Pass 1: RDP renders UV coords as colors into RGBA32 buffer
     (special UV identity texture + combiner outputs interpolated UV)
     Material index encoded in prim color R channel
   - Pass 2: CPU+RSP reads UV buffer, resolves actual texels from
     large textures in RAM using material+UV lookup
   - Bypasses 4KB TMEM limit entirely — renders 256×256 textures
   - Custom BCI format (4-color palette per 4×4 block, like S3TC/DXT1)
   - RSP ucode for decode + hand-optimized MIPS assembly for CPU path
   - CPU and RSP interleaved (every 4th slice RSP, rest CPU)
   - **For TR1**: Could render rooms with full 256×256 texture pages
     without any TMEM switching. Major quality + perf upgrade.
   - Files: `n64/engine/src/renderer/pipelineBigTex.cpp`,
     `n64/engine/src/renderer/bigtex/applyTexture.S`

2. **Draw layer command queuing system**
   - Objects record draw commands into separate `rspq_queue_t` buffers
   - Layers played back in order — enables sorting without re-sorting
   - Triple-buffered (3 queue sets)
   - **For TR1**: record opaque room geo to layer 0, water/transparent
     to layer 1, HUD to 2D layer
   - File: `n64/engine/src/renderer/drawLayer.cpp`

3. **Matrix pool allocator (576 pre-allocated, triple-buffered)**
   - Bitmap allocator for T3DMat4FP in uncached BSS
   - Scans for contiguous free bits, fallback to malloc_uncached
   - `RingMat4FP` triple-buffers to avoid CPU/GPU conflicts
   - **For TR1 Lara**: pre-allocate bone matrices from pool
   - File: `n64/engine/src/lib/matrixManager.cpp`

4. **BCI texture compression (4-color per 4×4 block)**
   - K-means clustering for 4-color palette per block
   - 2-bit indices per pixel = 0.5 bytes/pixel (vs 2B for RGBA16)
   - Build tool: `src/build/tools/bci.cpp`
   - **TR1 textures could be BCI-compressed in ROM** — 4× smaller
     than RGBA16, decoded at runtime by RSP or CPU

5. **Depth buffer via sbrk_top (no heap fragmentation)**
   - Allocates z-buffer at TOP of heap via sbrk
   - Regular heap grows up, z-buffer sits at ceiling
   - File: `n64/engine/src/lib/memory.cpp`

6. **Object single-allocation packing**
   - Pre-scan component list, single memalign for object + all components
   - Zero fragmentation for ECS-style objects
   - File: `n64/engine/src/scene/sceneLoader.cpp`

### Not Applicable
- Visual editor (ImGui desktop tool, not runtime)
- Node-graph scripting
- Audio manager details

---

## HailToDodongo/rspl — RSP Language Compiler ★★
Source: https://github.com/HailToDodongo/rspl

C-like language that transpiles to RSP MIPS assembly. T3D's entire
RSP microcode is written in RSPL. Web IDE available for instant
transpilation.

### Relevance
- Write custom RSP overlays without raw assembly
- ~50-100 lines RSPL for a BCI texture decode overlay
- Handles register allocation, vector swizzle, DMA built-ins,
  instruction scheduling automatically
- Output is standard libdragon rspq overlay (.S file)
- Learning curve: ~2 days language, ~1-2 weeks RSP architecture

### Potential Uses for Our Project
1. **AegisTex BCI decode overlay**: DMA compressed tile from ROM →
   DMEM → expand 4-color palette → DMA to RDRAM. RSP 8-lane SIMD
   processes 8 texels/cycle.
2. **Custom vertex effects**: TR1's per-vertex light values aren't
   standard normals — could extend T3D with custom light processing.
3. **Deferred UV resolve**: if we adopt BigTex approach later.

### Not Needed Immediately
T3D handles vertex work. CI8 tiles work without compression.
RSPL is the path to custom RSP work when ready for AegisTex
BCI decode or other specialized processing.

---

## lambertjamesd/skelatool64 — Mesh-to-N64 Converter (libultra) ★
Source: https://github.com/lambertjamesd/skelatool64

Build tool used by Portal64. Converts 3D models → C display lists.
Targets libultra only (not libdragon/T3D). Can't read TR1 .PHD.

### Borrowable Algorithms (reference, not code)

1. **Branch-and-bound render chunk ordering**
   - Finds optimal mesh draw order to minimize state transition cost
   - Cost model includes per-RDP-command timing estimates (microseconds)
   - Greedy fallback when search space too large
   - **Reference for AegisTex material sort optimization**
   - File: `src/RenderChunkOrder.cpp`

2. **RDP command cost model**
   - Individual timing for each mode change (combine, blend, tile, etc.)
   - Used as weights for the ordering optimizer
   - **Directly applicable to our material delta system**
   - File: `src/materials/MaterialTransitionTiming.h`

3. **Texture format auto-detection**
   - Analyzes pixel content to choose CI4/CI8/RGBA16/I/IA
   - Palette generation via nearest-neighbor RGB distance
   - Two-tone grayscale via least-squares fitting
   - **Reference for AegisTex offline format chooser**
   - File: `src/materials/TextureDefinition.cpp:565`

### Not Applicable
- Output format (libultra Gfx/Vtx, not T3D)
- Input parser (Assimp, can't read .PHD)
- No texture atlas/page support

### Lambert's libdragon fork
Source: https://github.com/lambertjamesd/libdragon
**Not useful.** PR staging area, 341 commits behind upstream, only 4
minor commits (gamma correction, CI4 auto-detect, compiler warning).
All Lambert's real innovations are in his game repos.

---

## Future Repos to Investigate
- libdragon GL internals — RSP overlay for OpenGL vertex processing
