# Implementation Plan — N64 Render Engine + AegisTex

## Project Goal

Build a **reusable, game-agnostic N64 rendering pipeline and texture system**
that can be packaged as a library and used across multiple PSX-era game ports
(TR1, TR2, TR3, and beyond). The first target is Tomb Raider 1 on SummerCart64.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────┐
│  Game Layer (per-port)                           │
│  .PHD parser, portal visibility, animation,      │
│  game logic, audio, camera                       │
├──────────────────────────────────────────────────┤
│  n64-render-core (reusable library)              │
│  T3D integration, material system, batch sorter, │
│  frame allocator, draw layers, matrix pool       │
├──────────────────────────────────────────────────┤
│  AegisTex (reusable library)                     │
│  Build tool (format chooser, packer)             │
│  Runtime (cache, stream, upload, evict)          │
├──────────────────────────────────────────────────┤
│  Platform (libdragon + T3D + RSPL overlays)      │
└──────────────────────────────────────────────────┘
```

The portability boundary:
- **Game layer** calls `render_*` and `aegistex_*` APIs
- **n64-render-core** calls T3D and rdpq
- **AegisTex** handles all texture economy (format, cache, stream, upload)
- Game never touches TMEM, T3D internals, or cache slots directly

---

## Deliverables

### D1: n64-render-core (runtime library)
- T3D lifecycle (init, frame start/end, viewport)
- Matrix pool (pre-allocated T3DMat4FP, triple-buffered ring)
- Frame bump allocator (64KB double-buffered, UncachedAddr)
- Batch sorter (material sort key, priority classes)
- Draw layer system (opaque / transparent / 2D, triple-buffered rspq_queue)
- rspq_block recording helpers (mesh + material)
- Camera-relative transform helper (subtract cam pos before fixed-point)
- Skeleton/bone pose builder (float hierarchy → T3DMat4FP array)
- Frustum culling (sphere, AABB, per-portal narrowing)

### D2: AegisTex (build tool + runtime library)

#### Build tool (`aegistex-pack`)
- Input: source textures (raw pixels or game-format tiles) + config
- Per-tile analysis: palette complexity, gradient smoothness, alpha usage
- Format ladder: CI4 → CI8 → RGBA16 (with IA4/IA8 for masks)
- Quality gate: perceptual error threshold per tier
- Output: `.atx` container with:
  - Header: magic `ATX1`, version, tile count, group count
  - Per-tile record (4 bytes): mode, flags, group_id
  - Tile payloads: format-native, DMA-aligned
  - Palette table: global + per-tile local palettes
  - Group table: stream group → tile list + ROM offset
  - TMEM batch table: groups of 2-4 tiles for alpha-to-tile upload
- ROM layout: group-ordered (not tile-index-ordered)
- Diagnostic report: format distribution, ROM size, peak cache estimate

#### Runtime library (`aegistex.h`)
- Cache: LRU + hash (megatexture blueprint), configurable slot count
- Scope API:
  ```c
  aegistex_scope_begin();
  aegistex_scope_set(AEGIS_HOT, group_id);
  aegistex_scope_set(AEGIS_WARM, group_id);
  aegistex_scope_end();  // triggers prefetch + eviction
  ```
- Bind API:
  ```c
  aegistex_bind(tile_id);              // single tile → TMEM TILE0
  aegistex_bind_group(batch_id);       // 2-4 tiles → TMEM for alpha-to-tile
  ```
- Residency classes: PINNED, HOT, WARM, OPPORTUNISTIC, COLD
- DMA budget: max N transfers per frame, async where possible
- Fallback: 1×1 solid color per group (always resident, never void)
- Palette management: global TLUT uploaded once per scope change,
  local CI4 palettes uploaded per-tile

### D3: TR1 game integration (first port)
- .PHD parser → room meshes → T3DVertPacked conversion
- Portal-based visibility with frustum narrowing
- Room → AegisTex stream group mapping
- Lara skeleton → T3D bone pose pipeline
- Items/static meshes → T3D blocks
- Baked room tiles for far pass (existing system)

---

## Implementation Order

### Milestone 1: T3D Integration (the 40ms → 4ms win)

**Goal**: Move vertex processing from CPU to RSP. Biggest single
performance gain available.

#### M1.1: Lara on T3D (partial integration, lowest risk)
- Install T3D, link into build
- At load time: convert Lara's 15 mesh vertex arrays to T3DVertPacked
  - position: int16 x,y,z (already int16 in TR1)
  - color: RGBA8 from pre-computed light value
  - UV: 10.5 fixed-point pixel coords from tex_uv_map
  - normal: pack (0,1,0) placeholder (TR1 has no per-vertex normals)
- Record rspq_block per Lara mesh:
  - t3d_vert_load + t3d_tri_draw + bone matrix commands via segment
- Per frame:
  - CPU builds bone pose (same push/pop code as now, output float matrices)
  - Convert to T3DMat4FP via t3d_mat4_to_fixed, write to UncachedAddr pool
  - t3d_segment_set(T3D_SEGMENT_SKELETON, pose_fp)
  - t3d_matrix_push(lara_world_mtx) → rspq_block_run(lara_block) → pop
- Keep rdpq for rooms (unchanged)
- **Expected**: Lara 32ms → ~4ms. Total frame ~20ms = ~25 FPS

#### M1.2: Near rooms on T3D
- At load time per room: convert room vertices to T3DVertPacked
  - position: room-relative int16 (offset by room origin at matrix level)
  - color: RGBA8 from vertex light + water tint (pre-baked at load)
  - UV: 10.5 fixed-point from tex_uv_map
- Record rspq_block per room:
  - Batch vertices into 70-vert loads (T3D cache limit)
  - t3d_tri_draw or t3d_tri_draw_strip for faces
  - Material transitions (texture uploads) embedded in block
- Per frame:
  - Build room matrix: room_origin − camera_pos, convert to fixed-point
  - For each visible near room: push matrix → block run → pop
- Camera-relative rendering: zero camera pos in view matrix,
  subtract from each room's model matrix
- **Expected**: Near rooms 24ms → ~4ms. Total frame ~12ms = ~30+ FPS

#### M1.3: Far pass on T3D (optional — already fast enough)
- Far pass is 9.6ms with baked tiles + vertex cache
- Could convert to T3D for consistency, but lower priority
- Keep rdpq far pass as fallback until near is proven

#### M1.4: Items + static meshes on T3D
- Same pattern as rooms: convert vertices, record blocks, play back
- Static meshes: per-mesh rspq_block with object matrix
- Items: same, but only first mesh (no bone tree for items)

### Milestone 2: AegisTex Build Tool

**Goal**: Offline texture compiler that picks optimal format per tile.

#### M2.1: Format chooser
- For each 32×32 tile from TR1 texture pages:
  - Extract unique colors, measure palette complexity
  - Try CI4 quantization, measure perceptual error (RMSE or SSIM)
  - If error < threshold → emit CI4 (544 bytes: 512 texel + 32 palette)
  - Else emit CI8 (1536 bytes: 1024 texel + 512 palette, or flag global palette)
  - Special case: RGBA16 for tiles that fail CI8 (rare in TR1)
  - IA4/IA8 for baked room tiles, fog fallbacks, masks
- Output format distribution report

#### M2.2: Group packer
- Analyze which tiles each room uses (from face texture references)
- Build stream groups: room_N → {tile_23, tile_45, tile_67, ...}
- Shared tiles (used by 3+ rooms) → common group
- Lara tiles → hero group (PINNED)
- Pack groups contiguously in ROM
- Build TMEM batch table: for each room, which sets of 2-4 tiles
  appear together most often → alpha-to-tile groups

#### M2.3: Container emitter
- Write `.atx` file with header, tile records, payloads, group table
- Generate `aegistex_offsets.h` (ROM offset + size, like baked_offsets.h)
- Integrate into Makefile build pipeline

### Milestone 3: AegisTex Runtime

**Goal**: Cache + stream + upload with scope-driven residency.

#### M3.1: Cache core
- LRU doubly-linked list + hash table (megatexture blueprint)
- 128 slots × max_tile_size = configurable RDRAM footprint
- surface_t per slot (for rdpq_tex_upload compatibility)
- Per-frame eviction fence (don't evict tiles used this frame)
- Fallback: 1×1 solid color always resident

#### M3.2: Scope + stream
- Game calls scope_begin / scope_set / scope_end each frame
- scope_end triggers: prefetch HOT groups, mark WARM, deprioritize rest
- DMA from ROM on cache miss, rate-limited (max N per frame)
- Group-aware: DMA entire group in one burst when possible

#### M3.3: TMEM upload integration
- aegistex_bind(tile_id): lookup cache → upload to TILE0
- aegistex_bind_group(batch_id): upload 2-4 tiles to TILE0-TILE3
  for T3D alpha-to-tile
- Palette management: global TLUT once per scope, local CI4 palettes per bind
- Works alongside T3D: bind textures between t3d_tri_sync() and next draw

### Milestone 4: Visibility Overhaul

**Goal**: Reduce visible geometry before rendering.

#### M4.1: Portal frustum narrowing
- For each room portal: construct 4 clipping planes from camera
  through portal corners (Portal64 pattern)
- Recursively visit adjacent rooms with narrowed frustum
- Only mark rooms visible if portal is on-screen
- **Expected**: ~3× fewer visible faces

#### M4.2: Pre-computed room non-visibility
- At build time: for each room pair, determine if they can ever
  be visible from each other (static analysis of portal connectivity)
- Store as u64 bitmask per room (TR1 has 38 rooms, fits in u64)
- Fast bitwise rejection before portal traversal

#### M4.3: Per-room face sorting by material
- At load time: sort each room's faces by texture index
- Groups faces with same texture → fewer texture switches
- With alpha-to-tile: sort by TMEM batch group instead
- Material sort key: (batch_group << 16 | texture_index)

### Milestone 5: Advanced Optimizations

**Goal**: Polish and squeeze remaining performance.

#### M5.1: Alpha-to-tile integration
- Build tool identifies top 4 textures per room
- Pack into TMEM simultaneously (CI4 at 512B each = 2KB for 4 tiles)
- Encode tile index in vertex alpha MSBs during T3DVertPacked conversion
- t3d_state_set_alpha_to_tile(true) per room
- **Expected**: texture switches drop from ~48/frame to ~12/frame

#### M5.2: Triangle strips
- Build tool converts room quads to strips at export time
- t3d_tri_draw_strip replaces individual t3d_tri_draw calls
- ~45% bandwidth savings on RSP triangle submission

#### M5.3: Adaptive budgets
- Monitor RSP stall time each frame
- If RSP stall > threshold: CPU has headroom, raise face/tri budgets
- If RSP stall ≈ 0: CPU-bound, lower budgets
- Smooth adaptation (megatexture LOD bias pattern)

#### M5.4: BCI compression + RSPL decode overlay (future)
- Write BCI decode overlay in RSPL (~50-100 lines)
- AegisTex stores BCI-compressed tiles in ROM (0.5 bytes/pixel)
- RSP decodes to CI8/RGBA16 in DMEM, DMAs to cache slot
- 4× ROM savings for texture data

---

## What Stays Game-Specific (per-port)

| Component | Why |
|-----------|-----|
| Level parser (.PHD, .TR2, etc.) | Each game has different format |
| Portal/visibility topology | TR1 has room portals, others may have BSP/octree |
| Animation system | TR1 bone format differs from TR2/3 |
| Game logic, camera, audio | Completely game-specific |
| Texture page layout | TR1: 256×256 CI8 pages; other games vary |
| Entity types, AI, triggers | Game-specific |

## What's Reusable (the library)

| Component | API |
|-----------|-----|
| T3D integration | `render_init()`, `render_frame_begin/end()` |
| Matrix pool | `render_matrix_alloc()`, `render_matrix_free()` |
| Frame allocator | `frame_malloc()`, `frame_pool_reset()` |
| Batch sorter | `render_batch_add()`, `render_batch_finish()` |
| Draw layers | `render_layer_begin()`, `render_layer_end()` |
| Skeleton pose | `render_build_pose()`, `render_bone_segment_set()` |
| Frustum culling | `render_frustum_test_sphere/aabb()` |
| AegisTex cache | `aegistex_init()`, `aegistex_bind()` |
| AegisTex scope | `aegistex_scope_begin/set/end()` |
| AegisTex build tool | `aegistex-pack` CLI |

---

## Priority Summary

| Priority | Milestone | Impact | Effort |
|----------|-----------|--------|--------|
| **P0** | M1.1: Lara on T3D | 32ms → 4ms | Days |
| **P0** | M1.2: Near rooms on T3D | 24ms → 4ms | ~1 week |
| **P1** | M4.1: Portal frustum narrowing | ~3× fewer faces | Days |
| **P1** | M2: AegisTex build tool | CI4/CI8 format optimization | ~1 week |
| **P2** | M3: AegisTex runtime | Cache + stream + upload | ~1 week |
| **P2** | M5.1: Alpha-to-tile | 48 → 12 tex switches/frame | Days |
| **P3** | M1.4: Items + SM on T3D | ~2ms savings | Days |
| **P3** | M4.3: Face sort by material | Fewer state changes | Hours |
| **P3** | M5.2: Triangle strips | ~45% RSP bandwidth | Days |
| **P4** | M5.3: Adaptive budgets | Smooth quality scaling | Hours |
| **P4** | M5.4: BCI + RSPL overlay | 4× ROM texture savings | ~1 week |
| **P4** | M4.2: Room non-visibility masks | Fast rejection | Hours |

---

## Success Metrics

| Metric | Current | After M1 | After M1-M4 | Endgame |
|--------|---------|----------|-------------|---------|
| FPS | 13 | ~30 | ~40+ | 50-60 |
| CPU frame time | 49ms | ~14ms | ~10ms | ~8ms |
| RSP utilization | ~0% | ~40% | ~60% | ~80% |
| RDRAM tex pressure | 897KB | 128KB cache | 128KB cache | 64KB cache |
| Tex switches/frame | ~48 | ~48 | ~12 | ~4 |
| Visible faces/frame | ~3000 | ~3000 | ~1000 | ~1000 |
| Heap free | ~89KB | ~89KB | ~800KB+ | ~1MB+ |

---

## Reference Sources (from research log)

| Technique | Source Repo | Milestone |
|-----------|-------------|-----------|
| T3D vertex T&L | Junk Runner / Spellcraft | M1 |
| rspq_block recording | Junk Runner / Spellcraft | M1 |
| Camera-relative transforms | Junk Runner / Spellcraft | M1 |
| Frame bump allocator | Junk Runner / Spellcraft | M1 |
| Skeleton segment addressing | Junk Runner (deep dive) | M1.1 |
| LRU tile cache + hash | n64brew2023 megatextures | M3 |
| Eviction fence + DMA budget | n64brew2023 megatextures | M3 |
| Portal frustum narrowing | Portal64 | M4.1 |
| Room non-visibility bitmask | Portal64 | M4.2 |
| Alpha-to-tile | tiny3d | M5.1 |
| Triangle strips | tiny3d | M5.2 |
| Adaptive LOD bias | n64brew2023 megatextures | M5.3 |
| BCI compression | Pyrite64 | M5.4 |
| RSPL overlay authoring | rspl | M5.4 |
| Format auto-detection | skelatool64 | M2 |
| RDP command cost model | skelatool64 | M4.3 |
| Render chunk ordering | skelatool64 | M4.3 |
| Ref-counted resource cache | Spellcraft | M3 |
| Draw layer queuing | Pyrite64 | D1 |
| Matrix pool allocator | Pyrite64 | D1 |
