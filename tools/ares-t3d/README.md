# ares-t3d: High-Level Emulation for libdragon/Tiny3D

A rendering acceleration layer for the [ares](https://ares-emu.net/) N64 emulator
that intercepts libdragon's rspq/Tiny3D command stream and renders natively on
the host GPU via OpenGL, bypassing both RSP emulation and parallel-rdp.

## Why

ares uses parallel-rdp (Vulkan compute shaders) for N64 RDP rendering. On
CPU-only servers (no GPU), this requires lavapipe (software Vulkan) which runs
at ~2 emulated fps. By recognizing T3D as a higher-level graphics language and
rendering it natively, we get near-instant emulation for libdragon homebrew.

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

## Three Tiers

### Tier 1: Telemetry
- Intercept and log all rspq/T3D commands without affecting rendering
- Validate command parsing against actual RSP execution
- Output structured logs for analysis

### Tier 2: Shadow Render
- Render via host OpenGL alongside normal emulated path
- Compare outputs frame-by-frame for correctness validation
- Pixel diff reports to catch translation bugs

### Tier 3: Full Replace
- Skip RSP/RDP emulation entirely for known T3D/rdpq paths
- Fall back to normal emulation for unknown overlays
- Full speed: renders at host GPU speed, not N64 speed

## Command Stream

rspq commands are 32-bit words: `[overlay_id(4)][cmd_id(4)][args(24)]`

T3D commands intercepted:
| CMD | ID | Description |
|-----|-----|-------------|
| TRI_DRAW | 0x0 | Draw triangle |
| SCREEN_SIZE | 0x1 | Viewport |
| MATRIX_STACK | 0x2 | Model matrix push/pop/set |
| SET_WORD | 0x3 | DMEM word (lights, ambient) |
| VERT_LOAD | 0x4 | Load vertices from RDRAM |
| LIGHT_SET | 0x5 | Configure light |
| DRAWFLAGS | 0x6 | Depth/shade/cull |
| PROJ_SET | 0x7 | Projection matrix |
| FOG_RANGE | 0x8 | Fog range |
| FOG_STATE | 0x9 | Fog enable |
| TRI_SYNC | 0xA | Flush triangles |
| TRI_STRIP | 0xB | Triangle strip |
| TRI_SEQ | 0xC | Sequential triangles |

## Building

```bash
# Apply patches to ares source
cd /tmp/ares && git apply /path/to/tools/ares-t3d/patches/*.patch

# Rebuild ares with T3D HLE
cmake --preset ubuntu-ci && cmake --build build -j$(nproc)
```

## Status

- [ ] Tier 1: Telemetry mode
- [ ] Tier 2: Shadow render
- [ ] Tier 3: Full replace
