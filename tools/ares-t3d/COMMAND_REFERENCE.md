# T3D Command Reference (from source)

Verified against `/tmp/tiny3d/src/t3d/t3d.c` and `t3d.h`.

## TR1 Usage Priority

The current TR1 game uses **7 of 13** commands (updated from live telemetry).

| Priority | Cmd | ID | Words | TR1 | Emitter | Render Semantic |
|----------|-----|-----|-------|-----|---------|----------------|
| **P0** | TRI_DRAW | 0x0 | 2 | YES | `t3d_tri_draw()` t3d.c:385 | Draw indexed triangle |
| **P0** | VERT_LOAD | 0x4 | 3 | YES | `t3d_vert_load()` t3d.c:151 | Upload vertex buffer |
| **P0** | MATRIX_STACK | 0x2 | 2 | YES | `t3d_matrix_set/push/pop()` t3d.c:93 | Model transform |
| **P0** | TRI_SYNC | 0xA | 1 | YES | `t3d_tri_sync()` t3d.h:413 | Flush draw batch |
| **P1** | DRAWFLAGS | 0x6 | 2 | YES | `t3d_state_set_drawflags()` t3d.c:303 | Render state |
| **P1** | LIGHT_SET | 0x5 | 4 | YES | `t3d_light_set_ambient()` t3d.c:190 | Lighting |
| **P1** | FOG_RANGE | 0x8 | 2 | **YES** | `t3d_fog_set_range()` t3d.c:442 | **Fog params (confirmed live)** |
| P2 | SCREEN_SIZE | 0x1 | 5 | no | `t3d_viewport_attach()` t3d.c:518 | Viewport setup |
| P2 | PROJ_SET | 0x7 | 1 | no | `t3d_matrix_set_proj()` t3d.c:134 | Projection matrix |
| P2 | SET_WORD | 0x3 | 2 | no | `t3d_dmem_set_u32()` t3d.c:100 | DMEM state |
| P3 | FOG_STATE | 0x9 | 1 | no | `t3d_fog_set_enabled()` t3d.h:543 | Fog toggle |
| P3 | TRI_STRIP | 0xB | 2 | no | `t3d_tri_draw_strip()` t3d.c:425 | Triangle strip |
| P3 | TRI_SEQ | 0xC | 2 | no | `t3d_tri_draw_unindexed()` t3d.c:404 | Sequential tris |

### Live telemetry data (from TR1 Caves, frame 88)
```
cmds=7408 t3d=112 rspq=4060 other=3236
verts=560 tris=32 drawcalls=6 mtx_ops=3
overlay=1 (runtime-assigned)
```

## Detailed Command Encoding

### 0x0 TRI_DRAW (2 words)
```
W0: [ovl:4][0x0:4][v0_dmem_addr:24]
W1: [v1_dmem_addr:16][v2_dmem_addr:16]
```
- Addresses are DMEM offsets into T3D vertex output buffer
- Emitted via `rdpq_write(-1, ...)` (async, with AUTOSYNC)
- **Render**: Draw triangle {v0, v1, v2}

### 0x1 SCREEN_SIZE (5 words)
```
W0: [ovl:4][0x1:4][guardBandScale:4][invScreenSize:20]
W1: [screenOffset_x:15][0][screenOffset_y:16]
W2: [screenScale_x:16][screenScale_y:16]
W3: [depthScale:16][normWScale:16]
```
- **Render**: Set viewport (glViewport equivalent)

### 0x2 MATRIX_STACK (2 words)
```
W0: [ovl:4][0x2:4][stackAdvance:8][flags:16]
    flags bit 0: doMultiply (multiply with parent)
    flags bit 1: onlyStackMove (push_pos, no matrix load)
W1: [rdram_addr:32] — pointer to T3DMat4FP (64 bytes)
    NULL if pop or push_pos
```
- T3DMat4FP: 32 bytes integer (s16 x16) + 32 bytes fraction (u16 x16)
- **Render**: Push/pop/set model matrix (glPushMatrix equivalent)

### 0x3 SET_WORD (2 words)
```
W0: [ovl:4][0x3:4][dmem_addr:24]
    bit 15 of addr: 1=16-bit write, 0=32-bit write
W1: [value:32]
```
- Low-level DMEM state write
- **Render**: Internal state update

### 0x4 VERT_LOAD (3 words)
```
W0: [ovl:4][0x4:4][input_byte_size:24]
W1: [rdram_addr:32] — pointer to T3DVertPacked array
W2: [dest_dmem_offset:16][src_dmem_offset:16]
```
- T3DVertPacked is 32 bytes per 2 vertices
- num_verts = input_byte_size / 16
- Max 70 vertices per load
- **Render**: Upload vertex buffer (glBufferData equivalent)

### 0x5 LIGHT_SET (4 words)
```
W0: [ovl:4][0x5:4][dmem_addr:24]
W1: [r:8][g:8][b:8][a:8]
W2: [direction_x:16][direction_y:16] or [pos_x:16][pos_y:16]
W3: [direction_z:16][0:16] or [pos_z:16][size:16]
```
- Ambient: addr points to ambient color slot
- Directional: addr points to light N slot, W2/W3 = direction
- Point: addr points to light N slot, W2/W3 = position + size
- **Render**: Set light uniform (glLight equivalent)

### 0x6 DRAWFLAGS (2 words)
```
W0: [ovl:4][0x6:4][cull_mask:24]
W1: [rdp_command_word:32]
```
- cull_mask encodes front/back face culling state
- rdp_command_word sets depth/shade/texture modes
- **Render**: glEnable(GL_DEPTH_TEST), glCullFace, etc.

### 0x7 PROJ_SET (1 word)
```
W0: [ovl:4][0x7:4][rdram_addr:24]
```
- 24-bit physical address of T3DMat4FP projection matrix
- **Render**: Set projection matrix (glMatrixMode(GL_PROJECTION))

### 0x8 FOG_RANGE (2 words)
```
W0: [ovl:4][0x8:4][fog_mul:24] — s16 fog scale factor
W1: [fog_offset:32] — s15.16 fixed-point offset
```
- **Render**: glFog(GL_FOG_START/END)

### 0x9 FOG_STATE (1 word)
```
W0: [ovl:4][0x9:4][flag:24]
    flag=0x0B: enable fog
    flag=0x0C: disable fog
```
- **Render**: glEnable/Disable(GL_FOG)

### 0xA TRI_SYNC (1 word)
```
W0: [ovl:4][0xA:4][0:24]
```
- Flush pending triangle buffer before overlay switch
- **Render**: glFlush / draw call boundary

### 0xB TRI_STRIP (2 words)
```
W0: [ovl:4][0xB:4][rdram_addr:24] — index buffer in RDRAM
W1: [dmem_addr:16][byte_count:16]
    dmem_addr bit 15: set=sync after draw
```
- **Render**: glDrawElements(GL_TRIANGLE_STRIP)

### 0xC TRI_SEQ (2 words)
```
W0: [ovl:4][0xC:4][base_vertex_dmem:24]
W1: [end_vertex_dmem:24][stride:8]
    stride=0: triangle mode (3 verts per tri)
    stride>0: quad mode
```
- **Render**: glDrawArrays(GL_TRIANGLES)

## Mismatches Found

| Item | Parser had | Actual (from source) | Fixed |
|------|-----------|---------------------|-------|
| TRI_DRAW size | 1 word | 2 words | YES |
| DRAWFLAGS size | 1 word | 2 words | YES |
| PROJ_SET size | 2 words | 1 word | YES |
| TRI_DRAW vertex format | raw indices | DMEM addresses | YES |
| DRAWFLAGS content | flag bits | cull_mask + RDP cmd | YES |
