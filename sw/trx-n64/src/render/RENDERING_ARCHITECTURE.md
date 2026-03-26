# TRX-N64 Rendering Architecture

## Goal: 100% Faithful Reproduction

Every vertex position, UV coordinate, texture pixel, face, room, light value,
and animation frame must match the original TR1 data exactly. No approximations.

## The TMEM Problem

The N64 RDP has 4 KB of texture memory (TMEM). TR1 textures are 256×256 8-bit
palettized pages. A single page is 64 KB — 16× larger than TMEM.

Traditional approach: split faces, tile textures into 64×64 chunks, load each
chunk into TMEM separately. This requires modifying geometry (adding split
points) and can break UV continuity on faces that span tile boundaries.

**For a faithful port, we cannot modify the original geometry.**

## Solution: Hybrid Pipeline

Two complementary approaches, used where each is strongest:

### 1. Deferred UV Pipeline (HailToDodongo approach)

For textured faces that need pixel-perfect UV accuracy:

```
Pass 1 (RSP/RDP):
  - Transform vertices (RSP microcode)
  - Rasterize triangles to off-screen UV buffer
  - Each pixel stores: 24-bit texture address (page + UV offset)
  - Shading values written to separate shade buffer

Pass 2 (CPU, raw MIPS assembly):
  - Read UV buffer pixel by pixel
  - Perform texel lookup from TR1 texture pages in RDRAM
  - Combine with shade buffer
  - Write final color to framebuffer

Pass 3 (RDP):
  - Composite any overlay/UI elements
```

**Advantages:**
- No TMEM tiling required — textures of any size work
- No face splitting — original geometry preserved exactly
- UV generation works correctly (no tile boundary artifacts)
- CPU texel lookup is cache-friendly (sequential pixel walk)

**Cost:**
- Requires custom RSP microcode (write UVs instead of colors)
- CPU texel lookup is the bottleneck (~3-5ms per frame)
- Needs raw MIPS assembly optimization for the CPU pass
- Simple texture compression format for bandwidth

**Implementation status:** Architecture defined, requires custom ucode development.

### 2. Megatexture / Mipmap LOD Pipeline (James Lambert approach)

For rooms with many distant surfaces where detail isn't needed:

```
For each texture page:
  - Generate mipmap chain (256→128→64→32→16→8)
  - LOD 5 (8×8) and LOD 4 (16×16) always resident in RDRAM
  - Higher LODs streamed from SD card on demand

Per face:
  - Compute screen-space texel density
  - Select LOD: lod = floor(0.5 * log2(screen_pixel_area))
  - If selected LOD not loaded, use next-available lower LOD
  - Load the mip tile into TMEM (guaranteed to fit at lower LODs)

Per frame:
  - Budget: N tile loads from SD card per frame
  - If budget exceeded, skip new tiles and use lower LOD fallback
  - Frustum-cull tiles: only load tiles for visible faces
```

**Advantages:**
- Works within standard RDP pipeline (no custom ucode needed)
- Natural LOD reduces TMEM pressure for distant geometry
- SD card streaming budget prevents frame spikes
- Lowest 2 LODs always available = no popping at distance

**Cost:**
- Requires mipmap generation at asset pack time
- LOD boundaries visible if not carefully placed (use dividing planes)
- Not suitable for close-up faces (need full resolution)

**Implementation status:** Mipmap generation ready to add to tr1_pack.py.

### 3. Combined Strategy

```
For each visible face:
  if (face is close to camera AND texture tile > 64×64):
    → Deferred UV pipeline (pixel-perfect, any texture size)
  elif (face is far from camera):
    → Megatexture LOD pipeline (mip-mapped, fits TMEM)
  elif (texture tile fits in TMEM as-is):
    → Direct CI8 TMEM load (simplest path, already implemented)
```

The direct CI8 path handles the common case (most TR1 texture tiles are
≤64×64 and fit in TMEM without any tiling).

### Z-Buffer Optimization

Following Lambert's observation: disabling the Z-buffer saves bandwidth.
TR1's room-based portal rendering naturally provides back-to-front ordering:

- Rooms are drawn far-to-near (reverse portal traversal order)
- Within each room, faces can be depth-sorted by average vertex Z
- Saves 48 bits written + 16 bits read per pixel (75% bandwidth reduction)
- Trade-off: some overdraw on overlapping faces within a room

For TR1, where rooms have relatively few overlapping faces, this is a net win.

## Current Implementation (Milestone 3)

The initial implementation uses the **Direct CI8 TMEM Load** path:
- TR1 8-bit palette indices stored byte-for-byte (no quantization)
- 256-color RGBA5551 palette loaded once per page change
- Per-face: compute UV bounding box, load sub-rectangle into TMEM
- Textured triangles with exact UV coordinates from original data

This is correct and faithful for all faces with tiles ≤64×64 pixels.
The deferred UV pipeline will be added for the remaining large-tile faces.

## James Lambert Techniques (Full Detail)

### Mipmap LOD Selection
LOD selection: `lod = floor(0.5 * log2(screen_pixel_area))`
For angled faces: 3 samples (closest/farthest/middle texel to camera),
compute LOD boundary, draw each LOD with dividing plane for clean seam.

### Tile Streaming Budget
Per frame N tile loads from SD. Most tiles reused frame-to-frame.
When budget exceeded: skip, use highest-detail available fallback.
Lowest 2 LODs always resident, never evicted — guaranteed fallback.

### Frustum Tile Culling
Per face: test against frustum. If partial intersection, compute
overlap in texture image space, only load/draw intersecting tiles.

### Z-Buffer Disable
Saves 48 bits written + 16 read per pixel (80→16 bits per pixel).
Requires back-to-front sort. Some overdraw but net bandwidth win.

### Two-Pass Far/Near
Pass 1: far objects scaled ~100×, low detail. Pass 2: near, full.
Avoids Z-fighting at distance. Layer high over low.

### Terrain Rendering Order
1. Skybox. 2. Lowest detail tiles (frustum culled). 3. If close,
replace with higher detail (recursive). Back-to-front, Z off.
Bake terrain chunks: geometry + texture + vertex lighting in one.
4 chunks → 1 lower-detail chunk for distance.

### Fog Optimization
Distant terrain doesn't use vertex colors (baked). Free the combiner:
- RGB fog interpolation (3-channel fog color over texture)
- Top-to-bottom fog gradient via freed vertex color channel

## References

- HailToDodongo's tiny3d: https://github.com/HailToDodongo/tiny3d
- James Lambert's portal64 rendering techniques
- TR1 texture format: TRX src/trx/game/level/sections/textures.c
