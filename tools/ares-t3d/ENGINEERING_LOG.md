# ares-t3d Engineering Log

## Session: 2026-03-30

### Chunk 1: Tier 1 Telemetry (CONFIRMED)
**Objective**: Build machine-readable command parser with strict validation
**Files**: include/t3d_hle.h, src/t3d_hle.c
**Result**: CONFIRMED — 61 unit tests pass
**Artifacts**: build/test_t3d_commands, build/test_vertex_extract, build/test_detection

Key decisions:
- Output format: `T3D_CMD <name> key=value`, `T3D_FRAME counters`, `T3D_DETECT events`
- Multi-signal overlay detection (3+ signals to confirm)
- All RDRAM reads bounds-checked with 0x1FFFFFFF physical mask

### Chunk 2: ares Hook Patch (PARTIALLY VALIDATED)
**Objective**: Apply T3D_HLE hooks to ares source, build with -DT3D_HLE
**Files**: patches/01-hle-hooks.patch, src/t3d_hle_hooks.h, src/t3d_hle_hooks.c
**Applied to**: /tmp/ares/ares/n64/{n64.hpp, rsp/dma.cpp, vi/vi.cpp, system/system.cpp, CMakeLists.txt}
**Build status**: In progress (rdram.ram.size → fixed from size() to .size)
**Result**: PARTIALLY VALIDATED — hooks compile but not yet tested against live ROM

Hook locations:
1. n64.hpp:34 — include t3d_hle_hooks.h with extern "C"
2. rsp/dma.cpp:42 — after DMA read loop, call t3d_hle_on_rsp_dma()
3. vi/vi.cpp:93 — after screen->frame(), call t3d_hle_on_vblank()
4. system/system.cpp:149 — after initDebugHooks(), call t3d_hle_system_init()
5. n64/CMakeLists.txt:33 — option(ARES_T3D_HLE), symlinked sources

Known fix: rdram.ram.size is a member variable, not a function (no parentheses)

### Chunk 3: Command Reference (CONFIRMED)
**Objective**: Verify all 13 T3D command formats against real source
**Files**: COMMAND_REFERENCE.md
**Result**: CONFIRMED — cross-referenced with t3d.c rspq_write() calls

Found and fixed:
- TRI_DRAW: 2 words (was 1) — v0 in args, v1|v2 packed in word 1
- DRAWFLAGS: 2 words (was 1) — cull_mask in args, RDP cmd in word 1
- PROJ_SET: 1 word (was 2) — addr in 24-bit args field
- TRI_DRAW uses DMEM addresses, not raw vertex indices

### Chunk 4: GL Renderer + Matrix Conversion (IMPLEMENTED BUT UNTESTED)
**Objective**: OSMesa headless GL context with shader pipeline
**Files**: include/gl_renderer.h, src/gl_renderer.c, include/matrix_convert.h, src/matrix_convert.c
**Tests**: test_matrix.c — 13 assertions pass
**Result**: IMPLEMENTED BUT UNTESTED (GL context not exercised in integration)

Matrix conversion verified:
- Identity matrix roundtrip: correct
- Fractional values (2.5, -0.75): correct to 0.01
- Row-major → column-major transpose: correct
- Out-of-bounds: returns false

GL renderer has:
- OSMesa context (320x240 RGBA8)
- GLSL 1.20 shaders (vertex color + linear fog)
- VBO upload from T3DHLEVertex
- Draw state (depth, cull, fog)
- RGBA5551 composite to RDRAM

### Chunk 5: Standalone Tools (CONFIRMED)
**Files**: tools/decode_buffer.c, tests/gen_fixture.c, scripts/validate.sh
**Fixtures**: tests/fixtures/tr1_lara_sample.bin
**Result**: CONFIRMED — decode_buffer correctly decodes fixture, validate.sh passes

---

## Manual Tests for Morning

1. **Verify ares T3D_HLE build**: `cd /tmp/ares && cmake --build build -j4`
2. **Run validate.sh**: `cd tools/ares-t3d && bash scripts/validate.sh`
3. **Boot TR1 in patched ares**: Look for T3D_FRAME lines in stderr
4. **Decode live buffer**: Capture DMA buffer from ares log, run through decode_buffer
5. **Check detection confidence**: Verify T3D_DETECT shows overlay confirmed

## Remaining Risks

1. ares build may have more compilation issues (C++ vs C linkage, nall types)
2. DMA hook filter (pbus < 0x200 && length <= 512) may miss command buffers
3. rspq command buffer layout may differ from assumptions
4. T3D overlay ID is assigned at runtime — may be different across runs
5. OSMesa GL context not tested in ares integration path yet
