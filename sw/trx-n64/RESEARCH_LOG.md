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

## Future Repos to Investigate
- HailToDodongo/tiny3d (T3D) — libdragon RSP vertex T&L overlay
- libdragon GL internals — RSP overlay for OpenGL vertex processing
- crashoveride95/n64homebrew — community rendering techniques
