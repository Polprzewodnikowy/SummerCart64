# Morning Validation Checklist

## Quick Validation (2 minutes)
```bash
# 1. Run all standalone tests (113 assertions)
cd tools/ares-t3d && bash scripts/validate.sh

# 2. Decode the live sample
cd build && ./decode_buffer ../tests/fixtures/tr1_lara_sample.bin 6
```

## Live ares Integration Test (5 minutes)
```bash
# 3. Boot TR1 in ares-t3d with telemetry (needs Xvfb + lavapipe)
Xvfb :99 -screen 0 640x480x24 +extension GLX &
DISPLAY=:99 tools/emulator/bin/ares-t3d \
    --setting "Audio/Driver=None" \
    --setting "General/HomebrewMode=true" \
    --system "Nintendo 64" --fullscreen \
    sw/trx-n64/build/trx-n64-full.z64 2>/tmp/t3d_test.log &
sleep 90

# 4. Check telemetry
grep "^T3D_DETECT" /tmp/t3d_test.log    # Should show overlay confirmed
grep "^T3D_CMD" /tmp/t3d_test.log | wc -l  # Should be >100K
grep "^T3D_FRAME" /tmp/t3d_test.log | head -3

# 5. Kill processes
kill %1 %2
```

## What to Verify
- [ ] `T3D_INIT mode=0 tier=telemetry` appears
- [ ] `T3D_DETECT event=confirmed overlay=1 confidence=3` appears
- [ ] T3D_CMD lines show TRI_DRAW, MATRIX_STACK, FOG_RANGE
- [ ] No crashes, game still renders caves correctly
- [ ] `scripts/validate.sh` shows ALL PASSED (113 tests)

## Status Summary

### CONFIRMED (exercised and validated)
- Tier 1 command parser: all 13 commands, strict validation, machine-readable
- ares hook integration: DMA observer fires, command buffers parsed
- T3D overlay auto-detection: confirmed as ID=1 via 3-signal confidence
- Live telemetry: 2.2M command lines from TR1 in 90 seconds
- Standalone tests: 113 assertions across 6 suites
- Fixture tools: decode_buffer, gen_fixture, validate.sh

### IMPLEMENTED BUT UNTESTED (in ares path)
- GL renderer (OSMesa context, GLSL shaders, VBO, composite)
- Matrix conversion (T3DMat4FP → float[16])
- RDP state tracker (color/scissor/depth/combine mode)
- Shadow comparison (structural + pixel diff)

### NOT YET IMPLEMENTED
- Wire GL renderer into ares hook path
- Tier 3 replace mode (skip RSP/RDP)
- Texture loading from N64 TMEM
- rdpq overlay detection
- Frame counter timing fix (vblank vs DMA ordering)

## Known Issues
1. T3D_FRAME counters show cmds=0 — timing issue where vblank
   fires before DMA processes commands. Commands ARE parsed.
2. FOG_RANGE is used by TR1 but wasn't in original "used" list
3. TRI_DRAW addresses are DMEM offsets, not vertex indices —
   need RSP vertex buffer offset to convert
4. Some DMA buffers with pbus >= 0x200 may be missed by filter
5. IMEM fingerprinting not implemented (using command pattern match)

## File Inventory
| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| include/t3d_hle.h | 180 | Core types + API | CONFIRMED |
| include/t3d_hle_hooks.h | 35 | ares entry points | CONFIRMED |
| include/gl_renderer.h | 75 | GL renderer API | IMPLEMENTED |
| include/matrix_convert.h | 20 | Matrix API | CONFIRMED |
| include/rdpq_state.h | 65 | RDP state API | CONFIRMED |
| include/shadow_render.h | 55 | Comparison API | CONFIRMED |
| src/t3d_hle.c | 430 | Command parser | CONFIRMED |
| src/t3d_hle_hooks.c | 140 | ares singleton | CONFIRMED |
| src/gl_renderer.c | 250 | OSMesa + shaders | IMPLEMENTED |
| src/matrix_convert.c | 55 | Fixed-point conv | CONFIRMED |
| src/rdpq_state.c | 115 | RDP state machine | CONFIRMED |
| src/shadow_render.c | 50 | Pixel comparison | CONFIRMED |
| tests/ (6 suites) | 460 | 113 assertions | ALL PASS |
| tools/decode_buffer.c | 90 | CLI decoder | CONFIRMED |
| COMMAND_REFERENCE.md | 170 | T3D command spec | CONFIRMED |
| ENGINEERING_LOG.md | 130 | Session log | Current |
