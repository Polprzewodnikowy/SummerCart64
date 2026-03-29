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

---

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

## Future Repos to Investigate
- HailToDodongo/tiny3d (T3D) — libdragon RSP vertex T&L overlay ★ PRIORITY
- libdragon GL internals — RSP overlay for OpenGL vertex processing
- crashoveride95/n64homebrew — community rendering techniques
