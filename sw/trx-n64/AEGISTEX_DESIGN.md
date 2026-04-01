# AegisTex Design Document

**Status:** Design phase — no implementation yet
**Branch:** `claude/aegistex-design-proposal-TEumM`
**Scope:** Replaces the format-ladder approach in `IMPLEMENTATION_PLAN.md` M2/M3
         with a material compiler and adaptive reconstruction system.

---

## 1. What AegisTex Is

AegisTex is an **offline material compiler and runtime reconstruction system**.
It is not a texture compressor. A texture compressor asks "how do I make this
image smaller?" AegisTex asks "what is the cheapest parametric description that
reproduces this surface's appearance at a given viewing distance?"

The output is not a scaled-down copy of the source texture. It is a compact,
machine-readable reconstruction description: a coarse color field plus a
monochrome residual overlay. The renderer reconstructs appearance from this
description at runtime using the RDP's two-cycle combiner — no texture decoding,
no palette lookup, no RDRAM streaming of large texel arrays per frame.

This is spiritually close to id Software's MegaTexture pipeline, but much
cheaper: MegaTexture stores every unique texel once in a massive atlas and
streams pages on demand, requiring complex streaming hardware. AegisTex stores
parametric reconstruction data that is deterministic and tiny, requiring only
a single two-cycle RDP composite per surface. The two approaches solve the same
problem — high apparent detail at low runtime cost — but AegisTex trades
reconstruction fidelity (acceptable on suitable materials) for near-zero
runtime complexity.

### What it replaces

The existing pipeline (n64_textures.c) loads CI8/CI4/RGBA16 tiles from a
pre-converted texture page into TMEM per surface. Every bind is a full TMEM
eviction. A 64x64 CI8 tile uses all 4096 bytes of TMEM. With 96 near-pass
binds per frame, worst-case DMA is ~512 KB/frame.

AegisTex near-tier uses ~2560 bytes of TMEM (color field + IA4 overlay),
leaves 1536 bytes free, and allows adjacent surfaces sharing a material class
to keep the overlay resident and only reload the tiny color field (~128-512
bytes). The structural win is not just smaller data — it's that the data
separates color from structure, enabling partial TMEM reuse that the existing
pipeline cannot do.

### What it does not replace

- Tiers 4 and 5 of the existing distance ladder (XFAR/XXFAR) remain
  conventional CI4 and IA fallbacks. AegisTex only covers tiers 1-3.
- Fallback textures (signage, UI, painted props, strongly multicolor
  surfaces) use the existing pipeline. AegisTex classifies these
  automatically at build time.
- The existing `tmem_state_t` cache and bind path remain in place for
  fallback surfaces. AegisTex is a separate runtime path, not a
  modification of the existing one.

---

## 2. Design Goals

**Primary:** Simulate high-resolution RGBA32-like perceptual appearance on
compatible materials at the cheapest possible reconstruction cost.

**Secondary:** Enable a general-purpose material system usable across multiple
N64 games (TR1, potential Morrowind demake, etc.), not a TR1-specific hack.

**Non-goals:**
- Literal RGBA32 equivalence on every surface at every distance
- Per-texel fidelity at far viewing distances
- Replacing fallback textures for inherently multicolor or detail-rich surfaces

---

## 3. Where AegisTex Fits in the Distance Ladder

Existing 5-tier system (tr_chunk.h):

```
Tier 1: NEAR   < 4096 units      GL RGBA16           <- AegisTex Near
Tier 2: MID    4096-7168 units   RDPQ CI8            <- AegisTex Mid
Tier 3: FAR    7168-12288 units  RDPQ CI8            <- AegisTex Far
Tier 4: XFAR   12288-16384 units RDPQ CI4            (unchanged)
Tier 5: XXFAR  > 16384 units     RDPQ IA8/IA4        (unchanged)
```

AegisTex replaces tiers 1-3 for eligible materials. Tiers 4-5 are untouched.
Per-chunk distance thresholds (tr_chunk_t.near_dist2, etc.) are respected as-is;
AegisTex inherits the existing distance system rather than defining its own.

Distance thresholds are parameters, not AegisTex constants — the system must
work at the room scale of TR1 and the exterior scale of an open-world game
without recompilation.

---

## 4. Effective Resolution Targets

Rather than expressing quality as "use an 8x8 grid," AegisTex expresses quality
as an **effective appearance target**: what resolution does the reconstruction
need to simulate at the intended viewing distance?

This makes the compiler objective outcome-driven. The compiler searches for the
cheapest color field + residual combination whose perceptual reconstruction error
falls within the target threshold, rather than starting from a fixed structure.

Resolution targets per quality variant:

| Variant    | Appearance Target  | Primary Use                        |
|------------|--------------------|------------------------------------|
| Quality+   | ~1024x1024 equiv.  | Near, abundant performance headroom|
| Quality    | ~512x512 equiv.    | Near default                       |
| Mid        | ~256x256 equiv.    | Mid default / Near performance fallback |
| Low        | ~128x128 equiv.    | Far default / Mid performance fallback  |
| Minimal    | ~64x64 equiv.      | Emergency fallback, any tier       |

These are appearance targets, not stored texture resolutions. A "1024x1024
equivalent" reconstruction may use a 32x32 color field in complex regions and
a coarser field in smooth regions (Phase 2), or a uniform fine grid (Phase 1).
What matters is that the perceptual error against the source falls within the
target threshold.

---

## 5. Material Blob Format

Each AegisTex material compiles to a single `.atm` (AegisTex Material) binary
blob stored in ROM. Debug JSON export is available during development but not
shipped.

### Blob layout

```
[Header]
  magic:            4 bytes  "ATM1"
  version:          1 byte
  flags:            1 byte   (see material flags below)
  source_w, h:      2x2 bytes  source texture dimensions
  uv_scale:         2x f32   UV scale factors
  uv_offset:        2x f32   UV placement offset
  near_dist_sq:     f32      distance threshold: near/mid boundary
  mid_dist_sq:      f32      distance threshold: mid/far boundary
  fit_score:        f32      0.0-1.0, 1.0 = perfect AegisTex fit
  material_class:   1 byte   (see material classes below)

[Overlay Table]
  overlay_count:    1 byte   number of unique overlays (1-4 typical)
  For each overlay:
    format:         1 byte   IA4 or IA8
    width, height:  2x u16
    data_offset:    u32      byte offset to overlay data in this blob

[Quality Variant Table]
  For each of 5 variants (Quality+, Quality, Mid, Low, Minimal):
    stored:         1 byte   0 = not present, 1 = present
    field_w, h:     2x u8    color field dimensions (cells)
    cell_format:    1 byte   RGBA5551 per cell, or indexed
    overlay_ref:    1 byte   index into overlay table
    data_offset:    u32      byte offset to color field data in this blob

[Color Field Data]  (one block per stored variant)
  cells:            field_w * field_h * 2 bytes  (RGBA5551 per cell)
  -- or --
  palette:          N * 2 bytes  (RGBA5551, N <= 16 for mid/far variants)
  indices:          field_w * field_h * 4 bits   (palette index per cell)

[Overlay Data]  (one block per unique overlay)
  IA4:  width * height / 2 bytes
  IA8:  width * height bytes
```

### Material flags

```
ATM_FLAG_FALLBACK       0x01  use traditional texture path, ignore AegisTex
ATM_FLAG_OVERLAY_SHARE  0x02  overlay is eligible for cross-material reuse
ATM_FLAG_FAR_AVERAGE    0x04  far tier collapses to single average color
ATM_FLAG_PHASE2_READY   0x08  reserved for non-uniform subdivision (Phase 2)
```

### Material classes

```
ATM_CLASS_STONE         0x01  cave walls, rock, masonry
ATM_CLASS_EARTH         0x02  dirt, mud, sand, gravel
ATM_CLASS_PLASTER       0x03  plaster, stucco, render
ATM_CLASS_WOOD          0x04  wood grain, timber, bark
ATM_CLASS_METAL         0x05  metal plates, Dwemer-style
ATM_CLASS_ORGANIC       0x06  moss, fabric, leather
ATM_CLASS_WATER         0x07  water, ice, glass
ATM_CLASS_FALLBACK      0xFF  signage, UI, painted props, multicolor
```

Material class drives overlay sharing: surfaces in the same class are candidates
for sharing overlays. The build pipeline clusters overlays by perceptual
similarity within a class and deduplicates.

---

## 6. Build Pipeline

### Overview

```
source texture (any resolution)
        |
        v
[Fallback Classifier]
  - compute worst-case near-tier reconstruction error
  - if error > threshold at finest subdivision: mark ATM_FLAG_FALLBACK
  - otherwise: proceed to reconstruction
        |
        v
[Color Field Generator]  (per variant)
  - Phase 1: uniform grid at ratio matching effective resolution target
  - compute one RGBA5551 value per cell (perceptual average of source region)
  - measure per-cell Delta E vs source region
        |
        v
[Residual Extractor]  (per variant)
  - subtract reconstructed color field from source (luma-weighted)
  - scale residual to [0, 1] range
  - encode as IA4 (default) or IA8 (promoted by rule, see below)
  - for far variants: apply residual threshold (weak residual zeroed out)
        |
        v
[Perceptual Error Check]
  - compare reconstruction (color field + residual) against source
  - use Delta E in CIELAB space, weighted by luma (not RGB MSE)
  - if error > target threshold: increase subdivision and retry
  - if error still fails at maximum subdivision: flag for review
        |
        v
[Overlay Dedup]
  - compare new overlay against existing overlays in same material class
  - if perceptual similarity > dedup threshold: assign shared overlay ref
  - otherwise: store new overlay, add to table
        |
        v
[Blob Emitter]
  - write .atm binary
  - emit diagnostic: fit_score, variant sizes, overlay shared/unique
  - emit fallback flag if classified as fallback
```

### Residual extraction (exact definition)

```
1. Generate color field reconstruction R (color cells upsampled to source dims)
2. Convert source S and reconstruction R to YUV (or CIELAB)
3. Compute per-pixel residual: delta = S.luma - R.luma
4. Scale: residual_normalized = (delta + 1.0) / 2.0  [maps [-1,1] to [0,1]]
5. Encode intensity channel: IA.I = residual_normalized
6. Alpha channel: IA.A = 1.0 (full blend) where |delta| > noise_floor, else 0
7. Quantize to IA4 (4-bit I, 4-bit A) or IA8 (8-bit I, 8-bit A)
```

The overlay encodes *everything the color field failed to reconstruct* — not
decorative detail, not just edges. It is an error correction layer. The
intensity channel corrects luminance error; the alpha channel gates where
correction is applied.

### IA8 promotion rule

Promote from IA4 to IA8 for a given variant when:
- The residual intensity histogram uses > 10 distinct levels across > 20% of
  texels (IA4's 16 levels are visibly insufficient), **and**
- The variant is near-tier (mid/far use IA4 regardless of histogram), **and**
- The resulting blob still fits within the configured ROM budget per material

**TMEM constraint note:** IA8 at 64x64 = 4096 bytes, which fills all of TMEM.
Combined with any color field, it exceeds 4096 bytes and cannot be loaded in
a single two-cycle composite. Options if IA8 is needed:
- Reduce overlay to 32x32 IA8 (1024 bytes) — fits with 16x16 field (2048 B total)
- Keep IA4 at full resolution — preferred for the clean single-pass path

The default and recommended path is IA4 at 64x64. IA8 is available but forces
a smaller overlay footprint.

### Cell color computation

For each cell (region of source texture):
```
1. Extract all source pixels in the cell's region
2. Convert to CIELAB
3. Compute perceptual centroid (average in Lab space, not RGB)
4. Convert centroid back to RGBA5551
5. Store as cell color
```

RGBA average in RGB space introduces hue errors on high-saturation sources.
Lab centroid is more expensive but produces correct results for the materials
AegisTex targets.

### Fallback classifier threshold

If the finest-subdivision near-tier reconstruction exceeds **Delta E 6.0** on
average across the surface (approximately twice the just-noticeable difference
for side-by-side comparison at typical viewing distance), the texture is marked
as ATM_FLAG_FALLBACK and the traditional pipeline handles it.

This threshold is a build configuration parameter, not hardcoded.

### Phase 1 subdivision ratios (uniform grid)

Color field dimensions are expressed as ratios of source texture dimensions:

| Effective Target | Ratio    | 64x64 source | 256x256 source |
|------------------|----------|--------------|----------------|
| ~1024x1024       | 1/2      | 32x32 cells  | 128x128 cells  |
| ~512x512         | 1/4      | 16x16 cells  | 64x64 cells    |
| ~256x256         | 1/8      | 8x8 cells    | 32x32 cells    |
| ~128x128         | 1/16     | 4x4 cells    | 16x16 cells    |
| ~64x64 equiv.    | 1/32     | 2x2 cells    | 8x8 cells      |

Fixed ratios are used in Phase 1. Phase 2 replaces uniform grids with
quadtree subdivision that concentrates cells at color boundaries.

---

## 7. Runtime Path

### Separation from existing pipeline

AegisTex surfaces use a completely separate runtime path from `n64_tex_bind`.
The existing `tmem_state_t` and bind path remain untouched and handle fallback
surfaces. The two paths never interleave on the same surface.

Decision point per surface at draw time:
```
if material->flags & ATM_FLAG_FALLBACK:
    n64_tex_bind(...)                       // existing path
else:
    aegistex_bind(material, variant, buf)   // new path
```

### TMEM layout for two-cycle composite

AegisTex occupies two TMEM tiles simultaneously:

```
TMEM 0x000: color field texture  (tile 0)
TMEM 0x100: IA4/IA8 overlay      (tile 1)
Palette at 0x800 is NOT used — no CI format, no LOAD_TLUT
```

Default near-tier sizing (16x16 field + 64x64 IA4 overlay):
```
Color field:  16 * 16 * 2 bytes = 512 bytes   → TMEM 0x000-0x0FF
IA4 overlay:  64 * 64 / 2 bytes = 2048 bytes  → TMEM 0x100-0x4FF
Total:        2560 of 4096 bytes used (63%)
Headroom:     1536 bytes free
```

No LOAD_TLUT, no palette change tracking. Two commands fewer per bind vs.
current pipeline on page changes.

### Two-cycle combiner configuration

The RDP two-cycle mode composites color field and residual overlay in one pass:

```
Cycle 1: output = TEXEL0 (color field sample — reconstructed base color)
Cycle 2: output = COMBINED + TEXEL1.intensity * TEXEL1.alpha
                  (adds IA residual correction to base color)
```

The overlay's intensity channel encodes the residual offset (signed, centered
at 0.5). The alpha channel gates correction: zero alpha leaves the color field
unchanged in smooth regions; full alpha applies the full residual correction.

Single RDP pass. No overdraw. No extra triangle submission.

### Overlay residency and sharing

The key runtime optimization: when adjacent surfaces share a material class,
the overlay is often already loaded in tile 1. The bind function checks overlay
identity before reloading:

```
aegistex_bind(material, variant, buf):
    field   = material->variants[variant].color_field
    overlay = material->variants[variant].overlay_ref

    // Always reload color field (tiny: 8-512 bytes)
    load_tmem_tile0(buf, field)

    // Only reload overlay if it changed
    if overlay.id != tmem_state.tile1_overlay_id:
        load_tmem_tile1(buf, overlay)
        tmem_state.tile1_overlay_id = overlay.id

    set_combine_two_cycle(buf)
    set_tile_sizes(buf, field.w, field.h, overlay.w, overlay.h)
```

In a TR1 cave room where all walls share `ATM_CLASS_STONE`, the overlay stays
resident across the entire room's draw. Only the color field (128-512 bytes)
reloads per surface. Compare to the current pipeline: every surface is a full
4096-byte TMEM eviction plus a potential 512-byte palette reload.

### Tier and variant selection

Per surface per frame:
```
1. Read tr_chunk_t.dist_sq (already computed this frame)
2. Select distance tier: near / mid / far
3. Read quality governor offset for that band: 0, -1, or -2
4. Compute variant index: base_variant + offset, clamped to [0, NUM_VARIANTS-1]
5. aegistex_bind(material, variant_index, buf)
```

No new distance computation. The governor offset is a single integer read per
band, applied uniformly to all surfaces in that band.

---

## 8. Dynamic Quality Governor

### Motivation

The governor selects among prebaked quality variants at runtime based on
measured performance pressure. It enables the system to degrade gracefully
under load without geometry changes, without popping, and with per-band
precision — near-band pressure does not affect mid or far quality.

### Band controllers

Three independent band controllers: near, mid, far. Each tracks:

```c
typedef struct {
    uint32_t smoothed_cost_us;  // EMA of this band's frame cost
    uint32_t budget_us;         // allocated budget for this band
    int8_t   quality_offset;    // 0 = default, -1 = cheaper, -2 = cheapest
    uint8_t  degrade_cooldown;  // frames until next downgrade allowed
    uint8_t  restore_cooldown;  // frames until next upgrade allowed
} aegistex_band_t;
```

Default budget split: 50% near / 30% mid / 20% far of total AegisTex frame
budget. Tunable per-project.

### Pressure source detection

AegisTex quality reduction helps only when pressure is RDP/TMEM-sourced.
CPU/geometry pressure is not relieved by changing material quality.

```
if tmem_state.loads_this_frame > (TEX_BIND_BUDGET_NEAR * 0.8):
    pressure = PRESSURE_TMEM_DMA      // texture thrash → AegisTex helps
elif rdp_stall_us > rdp_stall_threshold:
    pressure = PRESSURE_RDP_FILL      // fill rate → two-cycle overhead
else:
    pressure = PRESSURE_CPU_GEOMETRY  // vertex/transform → no benefit
```

When pressure is CPU/geometry, the governor suppresses quality changes.
Degrading AegisTex quality would not help and would degrade visuals for free.

`loads_this_frame` is already tracked in `tmem_state_t`. RDP stall is measured
via rdpq's timing facilities (RDP_SYNC_FULL timestamp delta).

### Per-band update algorithm

```
update_band(band, measured_cost_us, pressure_source):
    if pressure_source == PRESSURE_CPU_GEOMETRY:
        return  // governor inactive, no change

    // Exponential moving average (alpha=0.1 ≈ 16-frame window)
    band.smoothed_cost = band.smoothed_cost * 0.9 + measured_cost_us * 0.1

    overspend = band.smoothed_cost - band.budget_us
    headroom  = band.budget_us - band.smoothed_cost

    if overspend > 0 and band.degrade_cooldown == 0:
        band.quality_offset = max(band.quality_offset - 1, -2)
        band.degrade_cooldown = 3   // fast: may degrade again in 3 frames
        band.restore_cooldown = 30  // reset restore timer on any degrade

    elif headroom > (band.budget_us * 0.3) and band.restore_cooldown == 0:
        band.quality_offset = min(band.quality_offset + 1, 0)
        band.restore_cooldown = 30  // slow: wait 30 frames before next restore
        band.degrade_cooldown = 0

    if band.degrade_cooldown > 0: band.degrade_cooldown--
    if band.restore_cooldown > 0: band.restore_cooldown--
```

Key properties:
- **Asymmetric:** degrade in 3 frames of sustained overspend; restore after
  30 frames of 30% headroom. Fast to protect, slow to improve.
- **30% headroom threshold:** prevents immediately re-degrading after restoring.
- **Per-band independence:** a heavy near room does not touch mid or far quality.
- **Band-level granularity:** all surfaces in a band use the same offset.
  No per-surface oscillation.

### ROM storage for quality variants

All variants are precomputed at build time. Runtime never computes reconstruction
data — it selects among prebaked assets.

ROM cost per 64x64 source texture (worst case, no overlay sharing):
```
Quality+ (32x32 field + 64x64 IA4):  2048 + 2048 = 4096 B
Quality  (16x16 field + 64x64 IA4):   512 + 2048 = 2560 B
Mid      (8x8  field + 64x64 IA4):    128 + 2048 = 2176 B
Low      (4x4  field + 32x32 IA4):     32 + 512  =  544 B
Minimal  (2x2  field, no overlay):      8         =    8 B
                               Total:           ~9384 B
```

Five quality variants for ~2.3x the ROM of a single CI8 tile, covering all
distance and performance scenarios. With overlay sharing across materials in
the same class, real-world cost is significantly lower.

---

## 9. Integration with Existing Systems

**tr_chunk.h distance thresholds:** No changes. AegisTex reads
`tr_chunk_t.dist_sq` and existing distance constants directly.

**tex_budget_t bind budgets:** Existing TEX_BIND_BUDGET_NEAR/FAR remain for
fallback surfaces. AegisTex binds are tracked separately.

**tmem_state_t:** AegisTex extends the existing state struct with a
`tile1_overlay_id` field for overlay residency tracking. No other changes.

**tr_cluster_t:** No changes. Clustering and AegisTex operate at different
levels of the geometry hierarchy.

**T3D:** AegisTex bind calls insert between t3d_tri_sync() and the next draw,
same placement as current texture binds. Two-cycle combiner mode is set before
the surface's draw commands and reset to one-cycle afterwards if needed.

---

## 10. Phased Rollout

### Phase 1 — Core pipeline

Target materials: cave walls, stone, dirt, plaster (TR1 caves).

Deliverables:
- `aegistex-pack`: uniform grid compiler, residual extractor, fallback classifier
- `.atm` format: all 5 quality variants, overlay table, metadata
- Runtime bind path: two-tile TMEM, two-cycle combiner, overlay residency check
- Quality governor: per-band EMA, asymmetric cooldown, pressure detection
- Integration with existing distance tiers and tmem_state_t

Phase 1 success criteria (all required before Phase 2):
- Near-tier reconstruction visually indistinguishable from CI8/RGBA16 on
  stone/cave materials at near-tier viewing distances
- Overlay residency hit rate > 60% in a representative TR1 cave room
- Two-cycle fill overhead < 15% of total frame time in representative scenes
- No visible quality oscillation in a 60-second gameplay sequence

### Phase 2 — Refinements (post Phase 1 success)

- Quadtree / non-uniform subdivision
- Cross-material overlay deduplication
- Palette-indexed color fields for mid/far (further ROM reduction)
- Expansion to wood, metal, organic material classes
- Per-project budget profiling tool

---

## 11. Success Metrics

**Build-time (reported by aegistex-pack):**
- AegisTex-eligible rate (% of source textures passing fallback classifier)
- Average fit score across eligible materials
- Overlay deduplication ratio (unique overlays / total references)
- ROM cost: AegisTex total vs. equivalent CI8 coverage

**Runtime (debug build, per-frame):**
- TMEM load count: AegisTex binds vs. fallback binds
- Overlay residency hit rate (% of binds that skipped overlay reload)
- Per-band quality offset values (near/mid/far)
- Per-band smoothed cost vs. budget
- Pressure source classification each frame (TMEM / RDP / CPU)

**Visual (manual, per Phase 1 completion):**
- Near-tier: side-by-side vs. original CI8/RGBA16 on stone/cave surfaces at
  near-tier view distance — target: indistinguishable
- Far-tier: no visible structure loss vs. current CI4 at far-tier boundary
- Governor: no visible oscillation in 60-second varied-framerate sequence

**Performance targets (Phase 1, typical TR1 cave room):**
- TMEM DMA per frame: < 250 KB (current worst case: ~512 KB)
- Overlay residency hit rate: > 60%
- Two-cycle fill overhead: < 15% of total frame time

---

## 12. What AegisTex Is Not

**Not mipmaps.** Mipmaps store degraded copies of texel data at smaller
resolutions. AegisTex stores parametric reconstruction data — the minimum
information needed to reproduce perceptual appearance at a distance. A mipmap
at 1/4 resolution is the texture scaled down. An AegisTex mid-tier
representation may be structurally richer than a 1/4 mipmap because the overlay
preserves high-frequency structure from the full-resolution source while the
color field provides accurate spatial color at every cell boundary.

**Not a streaming system.** Data is resident in ROM and loaded to TMEM
synchronously at bind time. No background threads, no prefetch queue, no async
paging. All decisions are synchronous within a single frame's command buffer.

**Not universal.** Surfaces with inherently complex color — signage, murals,
stained glass, UI elements — require literal texel storage. The fallback path
is permanent and intentional.
