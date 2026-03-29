#!/usr/bin/env python3
"""
TR1 Chunk Baker — Pre-bakes per-room textures with mip levels.

For each room in a TR1 .PHD level, this tool:
  1. Reads all face textures from texture pages
  2. Applies palette to get RGB colors
  3. Computes a representative baked texture per room by sampling
     the room's most-used textures with average lighting applied
  4. Generates 3 mip levels: 32×32, 16×16, 8×8 (all RGBA16)
  5. Outputs a binary file embedded in the ROM

At runtime, the far pass uploads one pre-baked mip tile per room
instead of switching between many per-face textures — eliminating
the upload bottleneck entirely.

Binary output format (.baked):
  Header:
    char[4]   magic = "BAKE"
    uint16_t  room_count
    uint16_t  mip_levels = 3
    uint16_t  mip0_dim = 32
    uint16_t  mip1_dim = 16
    uint16_t  mip2_dim = 8
    uint16_t  reserved = 0

  Per room (room_count entries):
    uint16_t[32*32]  mip0 pixels (RGBA16, 2048 bytes)
    uint16_t[16*16]  mip1 pixels (RGBA16, 512 bytes)
    uint16_t[8*8]    mip2 pixels (RGBA16, 128 bytes)

  Total per room: 2688 bytes
  For ~20 rooms: ~53 KB (fits easily in ROM)

Usage:
    python3 bake_chunks.py <level.PHD> <output.baked>

SPDX-License-Identifier: GPL-3.0-or-later
"""

import struct
import sys
import os
import math


# ---- TR1 .PHD Parser (minimal, for baking) ----

def read_u8(data, pos):
    return data[pos], pos + 1

def read_u16(data, pos):
    return struct.unpack_from('<H', data, pos)[0], pos + 2

def read_s16(data, pos):
    return struct.unpack_from('<h', data, pos)[0], pos + 2

def read_u32(data, pos):
    return struct.unpack_from('<I', data, pos)[0], pos + 4

def read_s32(data, pos):
    return struct.unpack_from('<i', data, pos)[0], pos + 4


def parse_phd_for_baking(data):
    """Parse a TR1 .PHD file and extract what we need for baking:
    rooms, texture pages, object textures, palette."""

    pos = 0
    version, pos = read_u32(data, pos)
    if version != 0x00000020:
        print(f"  Warning: unexpected version 0x{version:08X}")

    # Texture pages (256×256 CI8, 65536 bytes each)
    num_tex_pages, pos = read_u32(data, pos)
    tex_pages = []
    for i in range(num_tex_pages):
        page = data[pos:pos + 65536]
        tex_pages.append(page)
        pos += 65536

    # Skip: unused1
    unused1, pos = read_u32(data, pos)

    # Rooms
    num_rooms, pos = read_u16(data, pos)
    rooms = []

    for ri in range(num_rooms):
        room = {}
        # Room info
        room['x'], pos = read_s32(data, pos)
        room['z'], pos = read_s32(data, pos)
        y_bottom, pos = read_s32(data, pos)
        y_top, pos = read_s32(data, pos)

        # Room data size
        room_data_size, pos = read_u32(data, pos)
        room_data_end = pos + room_data_size * 2  # size is in 16-bit words

        # Vertices
        num_verts, pos = read_u16(data, pos)
        verts = []
        for vi in range(num_verts):
            vx, pos = read_s16(data, pos)
            vy, pos = read_s16(data, pos)
            vz, pos = read_s16(data, pos)
            vlight, pos = read_s16(data, pos)
            verts.append((vx, vy, vz, vlight))
        room['vertices'] = verts

        # Quads
        num_quads, pos = read_u16(data, pos)
        quads = []
        for qi in range(num_quads):
            v0, pos = read_u16(data, pos)
            v1, pos = read_u16(data, pos)
            v2, pos = read_u16(data, pos)
            v3, pos = read_u16(data, pos)
            tex, pos = read_u16(data, pos)
            quads.append((v0, v1, v2, v3, tex))
        room['quads'] = quads

        # Triangles
        num_tris, pos = read_u16(data, pos)
        tris = []
        for ti in range(num_tris):
            v0, pos = read_u16(data, pos)
            v1, pos = read_u16(data, pos)
            v2, pos = read_u16(data, pos)
            tex, pos = read_u16(data, pos)
            tris.append((v0, v1, v2, tex))
        room['tris'] = tris

        # Sprites
        num_sprites, pos = read_u16(data, pos)
        pos += num_sprites * 4  # skip sprite data

        # Jump to end of room data
        pos = room_data_end

        # Portals
        num_portals, pos = read_u16(data, pos)
        pos += num_portals * 32  # skip portal data

        # Sectors
        sectors_z, pos = read_u16(data, pos)
        sectors_x, pos = read_u16(data, pos)
        room['sectors_x'] = sectors_x
        room['sectors_z'] = sectors_z
        pos += sectors_x * sectors_z * 8  # skip sector data

        # Ambient light
        ambient, pos = read_s16(data, pos)
        room['ambient'] = ambient

        # Lights
        num_lights, pos = read_u16(data, pos)
        pos += num_lights * 18  # skip light data (TR1: 18 bytes each)

        # Static meshes
        num_statics, pos = read_u16(data, pos)
        pos += num_statics * 18  # skip static mesh data

        # Alternate room + flags
        alt_room, pos = read_s16(data, pos)
        flags, pos = read_u16(data, pos)
        room['flags'] = flags

        rooms.append(room)

    # Skip floor data
    floor_data_size, pos = read_u32(data, pos)
    pos += floor_data_size * 2

    # Skip mesh data blob
    mesh_data_size, pos = read_u32(data, pos)
    pos += mesh_data_size * 2

    # Skip mesh pointers
    num_mesh_ptrs, pos = read_u32(data, pos)
    pos += num_mesh_ptrs * 4

    # Skip animations
    num_anims, pos = read_u32(data, pos)
    pos += num_anims * 32

    # Skip anim changes
    num_changes, pos = read_u32(data, pos)
    pos += num_changes * 6

    # Skip anim ranges
    num_ranges, pos = read_u32(data, pos)
    pos += num_ranges * 8

    # Skip anim commands
    num_cmds, pos = read_u32(data, pos)
    pos += num_cmds * 2

    # Skip anim bones (mesh tree)
    num_bones, pos = read_u32(data, pos)
    pos += num_bones * 4

    # Skip anim frames
    num_frames, pos = read_u32(data, pos)
    pos += num_frames * 2

    # Skip objects
    num_objects, pos = read_u32(data, pos)
    pos += num_objects * 18

    # Skip static objects
    num_statics2, pos = read_u32(data, pos)
    pos += num_statics2 * 32

    # Object textures
    num_obj_tex, pos = read_u32(data, pos)
    obj_textures = []
    for i in range(num_obj_tex):
        draw_type, pos = read_u16(data, pos)
        tex_page, pos = read_u16(data, pos)
        uvs = []
        for k in range(4):
            u, pos = read_u16(data, pos)
            v, pos = read_u16(data, pos)
            uvs.append((u >> 8, v >> 8))  # 8.8 fixed → integer pixel
        obj_textures.append({
            'page': tex_page,
            'uvs': uvs,
        })

    # Skip sprite textures + sequences
    num_sprite_tex, pos = read_u32(data, pos)
    pos += num_sprite_tex * 16
    num_sprite_seq, pos = read_u32(data, pos)
    pos += num_sprite_seq * 8

    # Skip cameras, sound sources, boxes, overlaps, zones
    num_cameras, pos = read_u32(data, pos)
    pos += num_cameras * 16
    num_sound_src, pos = read_u32(data, pos)
    pos += num_sound_src * 16
    num_boxes, pos = read_u32(data, pos)
    pos += num_boxes * 20
    num_overlaps, pos = read_u32(data, pos)
    pos += num_overlaps * 2
    # Zones: 6 × num_boxes × 2 bytes
    pos += 6 * num_boxes * 2

    # Skip animated textures
    num_anim_tex, pos = read_u32(data, pos)
    pos += num_anim_tex * 2

    # Skip items
    num_items, pos = read_u32(data, pos)
    pos += num_items * 22

    # Light map (8192 bytes)
    pos += 8192

    # Palette (256 × 3 bytes RGB, 6-bit per channel)
    palette = []
    for i in range(256):
        r = data[pos]; pos += 1
        g = data[pos]; pos += 1
        b = data[pos]; pos += 1
        palette.append((r, g, b))  # 0-63 range

    return {
        'rooms': rooms,
        'tex_pages': tex_pages,
        'obj_textures': obj_textures,
        'palette': palette,
    }


# ---- Texture Sampling ----

def sample_texture_pixel(tex_pages, palette, obj_tex, u, v):
    """Sample a pixel from a TR1 object texture at (u,v) in texel space."""
    page_idx = obj_tex['page']
    if page_idx >= len(tex_pages):
        return (128, 128, 128)  # gray fallback

    page = tex_pages[page_idx]
    uvs = obj_tex['uvs']

    # Compute UV bounding box
    u0 = min(uv[0] for uv in uvs)
    v0 = min(uv[1] for uv in uvs)
    u1 = max(uv[0] for uv in uvs)
    v1 = max(uv[1] for uv in uvs)

    # Map (u,v) in [0,1] to page pixel coords
    px = int(u0 + u * (u1 - u0 + 1))
    py = int(v0 + v * (v1 - v0 + 1))
    if px < 0: px = 0
    if px > 255: px = 255
    if py < 0: py = 0
    if py > 255: py = 255

    ci = page[py * 256 + px]
    r, g, b = palette[ci]
    # Convert from 6-bit (0-63) to 8-bit (0-255)
    return (r * 255 // 63, g * 255 // 63, b * 255 // 63)


def rgb_to_rgba16(r, g, b):
    """Convert 8-bit RGB to N64 RGBA16 (5-5-5-1)."""
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g5 << 6) | (b5 << 1) | 1


# ---- Room Baking ----

def bake_room(room, tex_pages, obj_textures, palette, tile_dim=32):
    """Bake a room's appearance into a single tile_dim × tile_dim RGBA16 texture.

    Strategy: collect color samples from all faces in the room,
    weighted by face area and lighting. Build a representative
    texture that captures the room's overall look.

    For each cell in the output tile, sample from the room's
    most common textures with average lighting applied.
    """
    # Collect all face texture references and average colors
    face_colors = []
    tex_usage = {}  # tex_idx → count

    all_faces = []
    for q in room['quads']:
        v0, v1, v2, v3, tex = q
        all_faces.append((tex, [v0, v1, v2, v3]))
    for t in room['tris']:
        v0, v1, v2, tex = t
        all_faces.append((tex, [v0, v1, v2]))

    if not all_faces:
        # Empty room — return gray tile
        pixel = rgb_to_rgba16(100, 100, 100)
        return [pixel] * (tile_dim * tile_dim)

    # Sample colors from each face
    for tex_val, vert_indices in all_faces:
        is_flat = (tex_val & 0x8000) != 0
        tex_idx = tex_val & 0x7FFF

        # Compute average vertex lighting for this face
        avg_light = 0
        valid_verts = 0
        for vi in vert_indices:
            if vi < len(room['vertices']):
                vlight = room['vertices'][vi][3]
                bright = 1.0 - (vlight / 8191.0)
                bright = max(0.0, min(1.0, bright))
                bright = 0.4 + bright * 0.6
                avg_light += bright
                valid_verts += 1
        avg_light = avg_light / max(valid_verts, 1)

        if is_flat:
            # Flat colored — use palette
            ci = tex_idx & 0xFF
            r, g, b = palette[ci]
            r = int(r * 255 / 63 * avg_light)
            g = int(g * 255 / 63 * avg_light)
            b = int(b * 255 / 63 * avg_light)
            face_colors.append((r, g, b))
        elif tex_idx < len(obj_textures):
            # Textured — sample center of texture
            r, g, b = sample_texture_pixel(
                tex_pages, palette, obj_textures[tex_idx], 0.5, 0.5)
            r = int(r * avg_light)
            g = int(g * avg_light)
            b = int(b * avg_light)
            face_colors.append((r, g, b))

            # Track texture usage
            tex_usage[tex_idx] = tex_usage.get(tex_idx, 0) + 1

    if not face_colors:
        pixel = rgb_to_rgba16(80, 80, 80)
        return [pixel] * (tile_dim * tile_dim)

    # Find the top N most-used textures
    top_textures = sorted(tex_usage.keys(),
                          key=lambda k: tex_usage[k], reverse=True)[:4]

    # Build the baked tile: grid of sampled textures
    pixels = []
    is_water = (room.get('flags', 0) & 1) != 0

    # Average lighting for the room
    room_bright = 1.0 - (room.get('ambient', 4096) / 8191.0)
    room_bright = max(0.2, min(1.0, room_bright))
    room_bright = 0.4 + room_bright * 0.6

    if top_textures:
        # Fill tile with samples from top textures
        num_top = len(top_textures)
        for y in range(tile_dim):
            for x in range(tile_dim):
                # Which texture to sample from
                tx = (x * num_top) // tile_dim
                tex_idx = top_textures[min(tx, num_top - 1)]
                obj_tex = obj_textures[tex_idx]

                # Sample position within the texture tile
                su = (x % (tile_dim // num_top)) / max(tile_dim // num_top, 1)
                sv = y / tile_dim

                r, g, b = sample_texture_pixel(
                    tex_pages, palette, obj_tex, su, sv)

                # Apply room lighting
                r = int(r * room_bright)
                g = int(g * room_bright)
                b = int(b * room_bright)

                # Water tint
                if is_water:
                    r = int(r * 0.4)
                    g = int(g * 0.6)

                r = max(0, min(255, r))
                g = max(0, min(255, g))
                b = max(0, min(255, b))

                pixels.append(rgb_to_rgba16(r, g, b))
    else:
        # No textured faces — use average flat color
        avg_r = sum(c[0] for c in face_colors) // len(face_colors)
        avg_g = sum(c[1] for c in face_colors) // len(face_colors)
        avg_b = sum(c[2] for c in face_colors) // len(face_colors)
        pixel = rgb_to_rgba16(avg_r, avg_g, avg_b)
        pixels = [pixel] * (tile_dim * tile_dim)

    return pixels


def box_filter_2x(pixels, src_dim):
    """2×2 box filter downsample: src_dim → src_dim/2."""
    dst_dim = src_dim // 2
    out = []
    for y in range(dst_dim):
        for x in range(dst_dim):
            sx, sy = x * 2, y * 2
            p00 = pixels[sy * src_dim + sx]
            p10 = pixels[sy * src_dim + sx + 1]
            p01 = pixels[(sy + 1) * src_dim + sx]
            p11 = pixels[(sy + 1) * src_dim + sx + 1]
            # Average RGBA16 channels
            ar = ((p00 >> 11) + (p10 >> 11) + (p01 >> 11) + (p11 >> 11) + 2) >> 2
            ag = (((p00 >> 6) & 0x1F) + ((p10 >> 6) & 0x1F) +
                  ((p01 >> 6) & 0x1F) + ((p11 >> 6) & 0x1F) + 2) >> 2
            ab = (((p00 >> 1) & 0x1F) + ((p10 >> 1) & 0x1F) +
                  ((p01 >> 1) & 0x1F) + ((p11 >> 1) & 0x1F) + 2) >> 2
            aa = (p00 | p10 | p01 | p11) & 1
            out.append((ar << 11) | (ag << 6) | (ab << 1) | aa)
    return out


def quantize_to_ci8(rgba16_pixels, dim):
    """Convert RGBA16 pixels to CI8: 256-color palette + index array.
    Returns (palette_rgba16[256], indices[dim*dim])."""
    # Extract unique RGB5 values, build palette
    colors = {}
    for px in rgba16_pixels:
        r5 = (px >> 11) & 0x1F
        g5 = (px >> 6) & 0x1F
        b5 = (px >> 1) & 0x1F
        key = (r5, g5, b5)
        colors[key] = colors.get(key, 0) + 1

    # Sort by frequency, take top 256
    sorted_colors = sorted(colors.keys(), key=lambda k: colors[k], reverse=True)[:256]
    palette_map = {c: i for i, c in enumerate(sorted_colors)}

    # Build palette in RGBA16 format
    palette = [0] * 256
    for i, (r5, g5, b5) in enumerate(sorted_colors):
        palette[i] = (r5 << 11) | (g5 << 6) | (b5 << 1) | 1

    # Map pixels to nearest palette entry
    indices = []
    for px in rgba16_pixels:
        r5 = (px >> 11) & 0x1F
        g5 = (px >> 6) & 0x1F
        b5 = (px >> 1) & 0x1F
        key = (r5, g5, b5)
        if key in palette_map:
            indices.append(palette_map[key])
        else:
            # Find nearest color
            best_idx = 0
            best_dist = 999999
            for ci, (pr, pg, pb) in enumerate(sorted_colors):
                d = (r5-pr)**2 + (g5-pg)**2 + (b5-pb)**2
                if d < best_dist:
                    best_dist = d
                    best_idx = ci
            indices.append(best_idx)

    return palette, indices


def quantize_to_ci4(rgba16_pixels, dim):
    """Convert RGBA16 pixels to CI4: 16-color palette + nibble-packed array.
    Returns (palette_rgba16[16], packed_bytes[dim*dim/2])."""
    # Median-cut-lite: collect colors, reduce to 16
    colors = {}
    for px in rgba16_pixels:
        r5 = (px >> 11) & 0x1F
        g5 = (px >> 6) & 0x1F
        b5 = (px >> 1) & 0x1F
        key = (r5, g5, b5)
        colors[key] = colors.get(key, 0) + 1

    sorted_colors = sorted(colors.keys(), key=lambda k: colors[k], reverse=True)[:16]
    palette_map = {c: i for i, c in enumerate(sorted_colors)}

    palette = [0] * 16
    for i, (r5, g5, b5) in enumerate(sorted_colors):
        palette[i] = (r5 << 11) | (g5 << 6) | (b5 << 1) | 1

    # Map pixels to nearest palette entry
    indices = []
    for px in rgba16_pixels:
        r5 = (px >> 11) & 0x1F
        g5 = (px >> 6) & 0x1F
        b5 = (px >> 1) & 0x1F
        key = (r5, g5, b5)
        if key in palette_map:
            indices.append(palette_map[key])
        else:
            best_idx = 0
            best_dist = 999999
            for ci, (pr, pg, pb) in enumerate(sorted_colors):
                d = (r5-pr)**2 + (g5-pg)**2 + (b5-pb)**2
                if d < best_dist:
                    best_dist = d
                    best_idx = ci
            indices.append(best_idx)

    # Pack into nibbles (2 pixels per byte, high nibble first)
    packed = []
    for i in range(0, len(indices), 2):
        hi = indices[i] & 0xF
        lo = indices[i+1] & 0xF if i+1 < len(indices) else 0
        packed.append((hi << 4) | lo)

    return palette, packed


def bake_level(phd_path, output_path):
    """Bake all rooms in a TR1 .PHD level file.

    Output format (BAK2):
      Header: 16 bytes
      Per room:
        mip0: 32×32 RGBA16 (2048 bytes)
        mip1_pal: 256 × RGBA16 palette (512 bytes)
        mip1_idx: 16×16 CI8 indices (256 bytes)
        mip2_pal: 16 × RGBA16 palette (32 bytes)
        mip2_idx: 8×8 CI4 packed nibbles (32 bytes)
      Total per room: 2048 + 512 + 256 + 32 + 32 = 2880 bytes
    """
    print(f"Baking chunks: {phd_path}")

    with open(phd_path, 'rb') as f:
        data = f.read()

    level = parse_phd_for_baking(data)
    rooms = level['rooms']
    print(f"  Rooms: {len(rooms)}")
    print(f"  Texture pages: {len(level['tex_pages'])}")
    print(f"  Object textures: {len(level['obj_textures'])}")

    MIP0_DIM = 32
    MIP1_DIM = 16
    MIP2_DIM = 8

    baked_rooms = []
    for ri, room in enumerate(rooms):
        face_count = len(room['quads']) + len(room['tris'])
        # Generate RGBA16 at all levels first
        mip0_rgba = bake_room(room, level['tex_pages'],
                              level['obj_textures'], level['palette'],
                              tile_dim=MIP0_DIM)
        mip1_rgba = box_filter_2x(mip0_rgba, MIP0_DIM)
        mip2_rgba = box_filter_2x(mip1_rgba, MIP1_DIM)

        # Quantize mip1 to CI8, mip2 to CI4
        mip1_pal, mip1_idx = quantize_to_ci8(mip1_rgba, MIP1_DIM)
        mip2_pal, mip2_ci4 = quantize_to_ci4(mip2_rgba, MIP2_DIM)

        baked_rooms.append((mip0_rgba, mip1_pal, mip1_idx, mip2_pal, mip2_ci4))
        if ri % 5 == 0:
            print(f"  Room {ri}/{len(rooms)} ({face_count} faces)")

    # Write binary output
    with open(output_path, 'wb') as f:
        # Header: BAK2 magic + room count + mip info
        f.write(b'BAK2')
        f.write(struct.pack('<HHHHHH',
                            len(rooms),
                            3,           # mip_levels
                            MIP0_DIM,
                            MIP1_DIM,
                            MIP2_DIM,
                            0))

        # Per-room data
        for ri, (mip0, m1_pal, m1_idx, m2_pal, m2_ci4) in enumerate(baked_rooms):
            # mip0: 32×32 RGBA16
            for px in mip0:
                f.write(struct.pack('<H', px))
            # mip1: CI8 palette (256 entries) + indices
            for px in m1_pal:
                f.write(struct.pack('<H', px))
            f.write(bytes(m1_idx))
            # mip2: CI4 palette (16 entries) + packed nibbles
            for px in m2_pal:
                f.write(struct.pack('<H', px))
            f.write(bytes(m2_ci4))

    per_room = (MIP0_DIM*MIP0_DIM*2 + 256*2 + MIP1_DIM*MIP1_DIM +
                16*2 + MIP2_DIM*MIP2_DIM//2)
    total_bytes = 16 + len(rooms) * per_room
    print(f"  Output: {output_path} ({total_bytes} bytes, "
          f"{total_bytes/1024:.1f} KB)")
    print(f"  Per room: mip0={MIP0_DIM}² RGBA16 + "
          f"mip1={MIP1_DIM}² CI8 + mip2={MIP2_DIM}² CI4 = "
          f"{per_room} bytes")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <level.PHD> <output.baked>")
        print(f"  Bakes per-room textures with mip levels for N64 far pass.")
        sys.exit(1)

    bake_level(sys.argv[1], sys.argv[2])
