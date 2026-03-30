# ares-t3d Engineering Log

## Session: 2026-03-30

### Chunk 1: Tier 1 Telemetry (CONFIRMED)
**Objective**: Build machine-readable command parser with strict validation
**Files**: include/t3d_hle.h, src/t3d_hle.c
**Tests**: 24 + 25 + 12 = 61 assertions
**Result**: CONFIRMED

### Chunk 2: ares Hook Patch (PARTIALLY VALIDATED)
**Objective**: Apply T3D_HLE hooks to ares source, build with -DT3D_HLE
**Files**: patches/01-hle-hooks.patch, src/t3d_hle_hooks.h, src/t3d_hle_hooks.c
**Applied to**: /tmp/ares/ares/n64/{n64.hpp, rsp/dma.cpp, vi/vi.cpp, system/system.cpp, CMakeLists.txt}
**Build**: In progress (324/376 targets compiled)
**Result**: PARTIALLY VALIDATED — hooks applied, rdram.ram.size fix applied, awaiting link

Fixes applied:
- rdram.ram.size is a member var not a function (removed parentheses)
- Source files symlinked into ares/n64/t3d_hle/ to satisfy source_group ROOT check
- extern "C" wrapper for C headers included from C++ translation units

### Chunk 3: Command Reference (CONFIRMED)
**Files**: COMMAND_REFERENCE.md
**Result**: CONFIRMED — verified against t3d.c rspq_write() calls
**Mismatches found and fixed**: TRI_DRAW (1→2 words), DRAWFLAGS (1→2), PROJ_SET (2→1)

### Chunk 4: GL Renderer + Matrix (IMPLEMENTED BUT UNTESTED)
**Files**: gl_renderer.h/c, matrix_convert.h/c
**Tests**: test_matrix.c — 13 assertions pass
**Result**: IMPLEMENTED BUT UNTESTED in ares integration

### Chunk 5: RDP State Tracker (CONFIRMED)
**Files**: rdpq_state.h/c
**Tests**: test_rdpq_state.c — 25 assertions pass
**Result**: CONFIRMED standalone

### Chunk 6: Shadow Comparison (CONFIRMED)
**Files**: shadow_render.h/c
**Tests**: test_shadow.c — 14 assertions pass
**Result**: CONFIRMED standalone

### Chunk 7: Standalone Tools (CONFIRMED)
**Files**: tools/decode_buffer.c, tests/gen_fixture.c, scripts/validate.sh
**Result**: CONFIRMED

---

## Test Counts
| Suite | Assertions | Status |
|-------|-----------|--------|
| test_t3d_commands | 24 | PASS |
| test_vertex_extract | 25 | PASS |
| test_detection | 12 | PASS |
| test_matrix | 13 | PASS |
| test_rdpq_state | 25 | PASS |
| test_shadow | 14 | PASS |
| **Total** | **113** | **ALL PASS** |

---

## Commands to Run in the Morning

```bash
# 1. Run all standalone tests
cd tools/ares-t3d && bash scripts/validate.sh

# 2. Check ares T3D_HLE build (may need to finish/restart)
cd /tmp/ares && cmake --build build -j4 2>&1 | tail -5

# 3. If ares built, install and test with TR1 ROM
strip -o tools/emulator/bin/ares-t3d /tmp/ares/build/rundir/bin/ares

# 4. Boot TR1 with telemetry
Xvfb :99 -screen 0 640x480x24 +extension GLX &
DISPLAY=:99 tools/emulator/bin/ares-t3d \
    --setting "Audio/Driver=None" \
    --setting "General/HomebrewMode=true" \
    --system "Nintendo 64" --fullscreen \
    sw/trx-n64/build/trx-n64-full.z64 2>&1 | tee /tmp/t3d_telemetry.log &
sleep 90
kill %1 %2

# 5. Check telemetry output
grep "T3D_" /tmp/t3d_telemetry.log | head -50
grep "T3D_FRAME" /tmp/t3d_telemetry.log
grep "T3D_DETECT" /tmp/t3d_telemetry.log

# 6. Decode a captured buffer
tools/ares-t3d/build/decode_buffer <captured.bin> 6
```

## Manual Test Checklist
1. [ ] ares builds cleanly with -DT3D_HLE -DARES_T3D_HLE=ON
2. [ ] ares boots TR1 ROM without crash
3. [ ] T3D_INIT line appears in stderr at startup
4. [ ] T3D_DMA lines appear for RSP DMA transfers
5. [ ] T3D_DETECT lines show overlay candidate/confirmed
6. [ ] T3D_FRAME lines appear at vblank with non-zero counters
7. [ ] T3D_CMD lines show VERT_LOAD, TRI_DRAW, etc.
8. [ ] No T3D_ERR lines (or only expected ones)
9. [ ] Validate.sh passes
10. [ ] TR1 still renders correctly in ares (HLE is passive in Tier 1)

### Chunk 8: Live ares Integration Test (CONFIRMED)
**Objective**: Boot TR1 in ares-t3d, verify telemetry flows
**Binary**: tools/emulator/bin/ares-t3d (16MB, stripped)
**ROM**: sw/trx-n64/build/trx-n64-full.z64
**Result**: CONFIRMED — T3D overlay detected, 2.2M commands parsed

Live output:
- `T3D_INIT mode=0 tier=telemetry` — fires at startup
- `T3D_HOOK event=system_init rdram_size=8388608` — 8MB RDRAM
- `T3D_DMA target=IMEM dram=0x00802000 len=4096` — rspq ucode load
- `T3D_DMA target=DMEM dram=0x000527B0 len=504 cmds=16` — command buffer
- `T3D_DETECT event=confirmed overlay=1 confidence=3` — auto-detected
- `T3D_CMD TRI_DRAW v0_addr=0x204080 v1_addr=0x0102 v2_addr=0x0408`
- `T3D_CMD MATRIX_STACK addr=0x00005714 flags=0x025101 multiply=1 push=0`
- 27 T3D_FRAME lines in 90 seconds (lavapipe speed)
- 2,240,024 T3D_CMD/DMA/DETECT lines total

Known issue: T3D_FRAME counters show cmds=0 — vblank fires before
DMA hook processes commands for that frame. Commands ARE parsed
(proven by 2.2M lines). Fix: use DMA-based frame boundaries.

## Remaining Risks
1. ares link may fail on remaining C++ issues
2. DMA hook filter may miss command buffers (pbus < 0x200 heuristic)
3. rspq commands in RDRAM may span multiple DMA transfers
4. T3D overlay ID changes per run — detection must be runtime
5. ISViewer printf not captured by ares terminal output
6. lavapipe still needed for rendering (T3D_HLE doesn't replace rendering yet)

## Architecture Summary
```
tools/ares-t3d/
├── include/
│   ├── t3d_hle.h          Core parser + state (CONFIRMED)
│   ├── t3d_hle_hooks.h    ares entry points (PARTIALLY VALIDATED)
│   ├── gl_renderer.h      OSMesa GL renderer (IMPLEMENTED)
│   ├── matrix_convert.h   Fixed-point conversion (CONFIRMED)
│   ├── rdpq_state.h       RDP state tracker (CONFIRMED)
│   └── shadow_render.h    Frame comparison (CONFIRMED)
├── src/
│   ├── t3d_hle.c          Command parser + telemetry
│   ├── t3d_hle_hooks.c    ares singleton + DMA observer
│   ├── gl_renderer.c      OSMesa + GLSL + VBO + composite
│   ├── matrix_convert.c   T3DMat4FP → float[16]
│   ├── rdpq_state.c       RDP command → GL state
│   └── shadow_render.c    Pixel + structural diff
├── tests/ (6 suites, 113 assertions)
├── tools/decode_buffer.c  Standalone command decoder
├── scripts/validate.sh    One-command validation
├── COMMAND_REFERENCE.md   Verified against T3D source
├── IMPLEMENTATION_PLAN.md 3-week plan
└── ENGINEERING_LOG.md     This file
```

### Chunk 9: Parser Convergence (CONFIRMED)
**Objective**: Eliminate false positives without killing valid activity
**Versions tested**: v1→v2→v3→v4

| Metric | v1 | v2 | v3 | v4 |
|--------|-----|-----|-----|-----|
| Warnings | 104,167 | 4,386 | 124 | **0** |
| cmd_id=13 | 60,413 | 1,065 | 0 | 0 |
| VERT_LOAD overflow | 13,664 | 118 | 124 | **0** |
| Detection | YES | YES | YES | YES |
| Valid T3D activity | YES | YES | YES | YES |

Root causes found and fixed:
1. v2: Unknown overlay commands have unknown sizes → stop scanning
2. v3: Unknown cmd_id (>=13) from misaligned scan → stop scanning  
3. v4: Implausible VERT_LOAD (size>1120, addr<0x1000) → stop scanning

Architecture insight: rspq multiplexes overlays in same DMA buffer.
Parser must identify trustworthy T3D segments, not assume entire buffers.
