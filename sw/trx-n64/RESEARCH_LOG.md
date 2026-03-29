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

## Future Repos to Investigate
- HailToDodongo/tiny3d (T3D) — libdragon RSP vertex T&L overlay
- libdragon GL internals — RSP overlay for OpenGL vertex processing
- crashoveride95/n64homebrew — community rendering techniques
