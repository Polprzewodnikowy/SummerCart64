# ares-t3d Implementation Plan

## Architecture

```
N64 CPU emulation (ares)
        |
   [rspq command buffer in RDRAM]
        |
   [Overlay ID detector]
        |
   T3D overlay? ──yes──> T3D HLE Renderer (host OpenGL)
        |                     |
   rdpq overlay? ─yes──> rdpq HLE State Translator
        |                     |
   Unknown ──────────> Normal RSP/RDP emulation (fallback)
```

## Hook Points in ares

| # | File | Location | Purpose |
|---|------|----------|---------|
| 1 | `n64/rsp/dma.cpp` | `dmaTransferStep()` after DMA loop | Detect ucode (IMEM) + intercept commands (DMEM) |
| 2 | `n64/rdp/render.cpp` | `RDP::render()` entry | Intercept RDP commands from rdpq |
| 3 | `n64/vi/vi.cpp` | `VI::main()` at vblank | Frame boundary, GL flush |
| 4 | `n64/vi/vi.cpp` | `VI::refresh()` entry | Composite HLE framebuffer to RDRAM (Tier 3) |
| 5 | `n64/system/system.cpp` | After system load | Initialize HLE state |
| 6 | `n64/rsp/io.cpp` | SP_STATUS write | Track RSP halt/unhalt for rspq detection |

All hooks guarded by `#if defined(T3D_HLE)`.

## T3D Overlay Detection

1. **IMEM fingerprint**: Hash first 128 bytes of RSP ucode DMA → match rspq engine signature
2. **Overlay table**: Monitor DMEM writes to overlay descriptor region (0x00-0x80), read ucode RDRAM pointers, hash → match T3D
3. **Fallback**: Pattern-match command stream — SCREEN_SIZE + VERT_LOAD + TRI_DRAW = T3D

## Vertex Data (VERT_LOAD)

```
Word 0: [overlay_id:4][0x4:4][count:8][dest_offset:16]
Word 1: [rdram_address:32]
```

T3DVertPacked (32 bytes per 2 verts, big-endian in RDRAM):
- posA[3] (s16), normA (u16), posB[3] (s16), normB (u16)
- uvA[2] (s16), rgbaA[4] (u8), uvB[2] (s16), rgbaB[4] (u8)

## Matrix Data (PROJ_SET, MATRIX_STACK)

T3DMat4FP: 64 bytes — first 32 = integer parts (s16 x16), next 32 = fraction parts (u16 x16).
Convert: `float = (float)intPart + (float)fracPart / 65536.0f`

## RDP State → GL Mapping

| RDP Command | GL Equivalent |
|------------|---------------|
| Set_Other_Modes (0x2F) | glEnable(GL_DEPTH_TEST), glDepthMask, glEnable(GL_BLEND) |
| Set_Combine_Mode (0x3C) | Fragment shader uniform (combiner mode) |
| Set_Scissor (0x2D) | glScissor() |
| Set_Fog_Color (0x38) | Fog color uniform |
| Set_Prim_Color (0x3A) | Prim color uniform |
| Set_Color_Image (0x3F) | Target framebuffer RDRAM address |
| Load_Block/Tile (0x33/34) | Read texture from RDRAM → GL texture |
| Sync_Full (0x29) | Flush GL rendering |

Common combiner shortcuts:
- `(SHADE, 0, TEXTURE, 0)` → `fragColor = vertColor * texColor`
- `(SHADE, 0, 1, 0)` → `fragColor = vertColor`
- `(PRIM, 0, SHADE, 0)` → `fragColor = primColor * vertColor`

## Framebuffer Composite (Tier 3)

1. `glReadPixels()` → host RGBA8888 buffer
2. Convert to N64 RGBA5551: `(r>>3)<<11 | (g>>3)<<6 | (b>>3)<<1 | (a>>7)`
3. Write to `rdram.ram.data` at VI's `io.dramAddress`
4. Normal VI scanout picks up our pixels

## File Structure

```
tools/ares-t3d/
  include/
    t3d_hle.h              Main API
    rspq_parser.h          rspq command stream walker
    t3d_commands.h         T3D command decoders
    rdpq_state.h           rdpq render state tracker
    gl_renderer.h          Host OpenGL renderer
    rdram_reader.h         Endian-correct RDRAM accessor
    overlay_detector.h     Ucode/overlay fingerprinting
    framebuffer_composite.h  GL→RDRAM compositor
  src/
    t3d_hle.c              Entry point, tier dispatch
    rspq_parser.c          Command buffer walking
    t3d_commands.c         All 13 T3D command handlers
    rdpq_state.c           RDP state machine
    gl_renderer.c          EGL/OSMesa context + draw
    rdram_reader.c         Typed RDRAM reads
    overlay_detector.c     Fingerprinting
    framebuffer_composite.c  Pixel format conversion + writeback
    telemetry.c            Structured JSON logging
  shaders/
    basic.vert             MVP + vertex color + UV
    basic.frag             Combiner approximation + fog
  patches/
    01-hle-hooks.patch     Hook points in ares
    02-hle-build.patch     CMake integration
  tests/
    test_rspq_parser.c
    test_t3d_commands.c
```

## Schedule

### Week 1 — Tier 1 (Telemetry)
- Day 1: Headers + stubs + ares patches, verify T3D_HLE build
- Day 2: rdram_reader + overlay_detector (IMEM fingerprint)
- Day 3: rspq_parser (overlay 0: JUMP, CALL, RET)
- Day 4: t3d_commands (all 13 decoders)
- Day 5: telemetry + integration test with T3D ROM

### Week 2 — Tier 2 (Shadow Render)
- Day 6-7: gl_renderer (EGL/OSMesa context, shaders, FBO)
- Day 8: Vertex upload from VERT_LOAD, first triangles
- Day 9: rdpq_state (combiner, scissor, depth from RDP commands)
- Day 10: Shadow comparison, diff images

### Week 3-4 — Tier 3 (Full Replace)
- Day 11-12: framebuffer_composite (GL→RDRAM writeback)
- Day 13: Short-circuit RSP/RDP for known overlays
- Day 14-15: Texture loading (TMEM→GL textures)
- Day 16-17: Fog, lighting in shader
- Day 18-20: Mixed-mode fallback, performance tuning, regression tests

## Key Risks

| Risk | Mitigation |
|------|-----------|
| rspq format varies between libdragon versions | Fingerprint multiple versions; command API is stable |
| EGL/OSMesa fails in headless env | Test Day 6; fallback to pure SW rasterizer |
| RDRAM cached/uncached aliases | Always mask: `addr & 0x1FFFFFFF` then `& (rdramSize-1)` |
| rdpq state tracking misses commands | Start with solid color, add texture/combiner incrementally |
| Performance still slow | GL on llvmpipe is still 100x faster than parallel-rdp on lavapipe |
