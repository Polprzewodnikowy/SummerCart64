#!/usr/bin/env python3
"""
AegisTex Pack — Offline texture compiler for N64 rendering pipeline.

For each object texture in a TR1 .PHD level:
  1. Extracts the 32×32 tile from texture pages (raw CI8 palette indices)
  2. Analyzes palette complexity (unique color count)
  3. Chooses format: CI4 if <= 16 unique colors, else CI8
  4. For CI4: finds best 16-color subset, remaps indices, packs nibbles
  5. Outputs unified .atx container with both formats

Binary output format (.atx):
  Header (32 bytes):
    char[4]   magic = "ATX1"
    uint16_t  tile_count
    uint16_t  tile_dim = 32
    uint16_t  ci8_count        (tiles stored as CI8)
    uint16_t  ci4_count        (tiles stored as CI4)
    uint8_t[16] reserved

  Per-tile record (4 bytes each, tile_count entries):
    uint8_t   mode             (0=CI8, 1=CI4)
    uint8_t   palette_size     (CI4: 16, CI8: 0 = uses global)
    uint16_t  payload_offset   (offset into payload section, in 16-byte units)

  CI8 payload per tile (1024 bytes):
    uint8_t[32*32] raw palette indices

  CI4 payload per tile (544 bytes):
    uint16_t[16]  local 16-color RGBA16 palette (32 bytes)
    uint8_t[32*32/2] packed nibble indices (512 bytes)

Usage:
    python3 aegistex_pack.py <level.PHD> <palette_source.PHD> <output.atx>
"""

import struct
import sys
import os
import math
from collections import Counter


def read_u16(data, pos):
    return struct.unpack_from('<H', data, pos)[0], pos + 2

def read_u32(data, pos):
    return struct.unpack_from('<I', data, pos)[0], pos + 4

def read_s32(data, pos):
    return struct.unpack_from('<i', data, pos)[0], pos + 4


def parse_phd(data):
    """Parse .PHD to extract texture pages, object textures, and palette."""
    pos = 0
    version, pos = read_u32(data, pos)

    # Texture pages (256x256 CI8)
    num_pages, pos = read_u32(data, pos)
    tex_pages = []
    for i in range(num_pages):
        tex_pages.append(data[pos:pos + 65536])
        pos += 65536

    # Skip to palette (need to parse through the whole file)
    # Skip unused1
    pos += 4

    # Skip rooms
    num_rooms, pos = read_u16(data, pos)
    for ri in range(num_rooms):
        pos += 4 + 4 + 4 + 4  # x, z, y_bottom, y_top
        room_data_size, pos = read_u32(data, pos)
        pos += room_data_size * 2
        num_portals, pos = read_u16(data, pos)
        pos += num_portals * 32
        num_z, pos = read_u16(data, pos)
        num_x, pos = read_u16(data, pos)
        pos += num_z * num_x * 8
        ambient, pos = read_u16(data, pos)
        num_lights, pos = read_u16(data, pos)
        pos += num_lights * 18
        num_sm, pos = read_u16(data, pos)
        pos += num_sm * 18
        pos += 2 + 2  # alt room + flags

    # Floor data
    floor_size, pos = read_u32(data, pos)
    pos += floor_size * 2

    # Mesh data
    mesh_data_size, pos = read_u32(data, pos)
    pos += mesh_data_size * 2

    # Mesh pointers
    num_mesh_ptrs, pos = read_u32(data, pos)
    pos += num_mesh_ptrs * 4

    # Animations + changes + ranges + commands + bones + frames
    num_anims, pos = read_u32(data, pos)
    pos += num_anims * 32
    num_changes, pos = read_u32(data, pos)
    pos += num_changes * 6
    num_ranges, pos = read_u32(data, pos)
    pos += num_ranges * 8
    num_cmds, pos = read_u32(data, pos)
    pos += num_cmds * 2
    num_tree, pos = read_u32(data, pos)
    pos += num_tree * 4
    num_frames, pos = read_u32(data, pos)
    pos += num_frames * 2

    # Objects + static objects
    num_objects, pos = read_u32(data, pos)
    pos += num_objects * 18
    num_static, pos = read_u32(data, pos)
    pos += num_static * 32

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
            uvs.append((u >> 8, v >> 8))
        obj_textures.append({'page': tex_page, 'uvs': uvs})

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
    pos += 6 * num_boxes * 2  # zones

    # Skip animated textures
    num_anim_tex, pos = read_u32(data, pos)
    pos += num_anim_tex * 2

    # Skip items
    num_items, pos = read_u32(data, pos)
    pos += num_items * 22

    # Light map (8192 bytes)
    pos += 8192

    # Palette (256 x 3 bytes RGB, 6-bit per channel)
    palette = []
    for i in range(256):
        r = data[pos]; pos += 1
        g = data[pos]; pos += 1
        b = data[pos]; pos += 1
        palette.append((r, g, b))

    return tex_pages, obj_textures, palette


def palette_to_rgba16(palette):
    """Convert 6-bit RGB palette to RGBA16 array."""
    rgba16 = []
    for r6, g6, b6 in palette:
        r5 = r6 >> 1
        g5 = g6 >> 1
        b5 = b6 >> 1
        rgba16.append((r5 << 11) | (g5 << 6) | (b5 << 1) | 1)
    rgba16[0] = 0  # index 0 = transparent
    return rgba16


def extract_tile(tex_pages, otex, tile_dim):
    """Extract a tile_dim x tile_dim CI8 tile from texture pages."""
    us = [uv[0] for uv in otex['uvs']]
    vs = [uv[1] for uv in otex['uvs']]
    u0, u1 = min(us), max(us)
    v0, v1 = min(vs), max(vs)
    tw = max(u1 - u0 + 1, 1)
    th = max(v1 - v0 + 1, 1)

    tile = bytearray(tile_dim * tile_dim)
    if otex['page'] < len(tex_pages):
        page = tex_pages[otex['page']]
        for y in range(tile_dim):
            sy = v0 + y * th // tile_dim
            if sy > 255: sy = 255
            for x in range(tile_dim):
                sx = u0 + x * tw // tile_dim
                if sx > 255: sx = 255
                tile[y * tile_dim + x] = page[sy * 256 + sx]
    return tile


def quantize_ci4(ci8_tile, global_palette_rgba16, tile_dim):
    """Quantize a CI8 tile to CI4 (16 colors).

    Returns (local_palette_rgba16[16], packed_nibbles[tile_dim*tile_dim/2], error).
    Uses the 16 most frequent palette indices from the tile.
    """
    # Count frequency of each palette index
    freq = Counter(ci8_tile)
    top16 = [idx for idx, _ in freq.most_common(16)]

    # Build local palette (RGBA16)
    local_pal = [0] * 16
    for i, ci8_idx in enumerate(top16):
        local_pal[i] = global_palette_rgba16[ci8_idx]

    # Build reverse map: global palette index → local CI4 index
    remap = {}
    for i, ci8_idx in enumerate(top16):
        remap[ci8_idx] = i

    # Remap tile indices, finding nearest for unmapped colors
    ci4_indices = []
    total_error = 0
    for ci8_idx in ci8_tile:
        if ci8_idx in remap:
            ci4_indices.append(remap[ci8_idx])
        else:
            # Find nearest color in local palette
            src = global_palette_rgba16[ci8_idx]
            sr = (src >> 11) & 0x1F
            sg = (src >> 6) & 0x1F
            sb = (src >> 1) & 0x1F
            best_ci4 = 0
            best_dist = 999999
            for ci4, pal_rgba in enumerate(local_pal):
                pr = (pal_rgba >> 11) & 0x1F
                pg = (pal_rgba >> 6) & 0x1F
                pb = (pal_rgba >> 1) & 0x1F
                d = (sr - pr) ** 2 + (sg - pg) ** 2 + (sb - pb) ** 2
                if d < best_dist:
                    best_dist = d
                    best_ci4 = ci4
            ci4_indices.append(best_ci4)
            total_error += best_dist

    # Pack into nibbles (2 pixels per byte)
    packed = bytearray(tile_dim * tile_dim // 2)
    for i in range(0, len(ci4_indices), 2):
        hi = ci4_indices[i] & 0xF
        lo = ci4_indices[i + 1] & 0xF if i + 1 < len(ci4_indices) else 0
        packed[i // 2] = (hi << 4) | lo

    # Perceptual error: RMSE in RGB5 space
    num_pixels = tile_dim * tile_dim
    rmse = math.sqrt(total_error / max(num_pixels, 1))

    return local_pal, packed, rmse


def aegistex_pack(phd_path, output_path):
    with open(phd_path, 'rb') as f:
        data = f.read()

    tex_pages, obj_textures, palette = parse_phd(data)
    global_pal_rgba16 = palette_to_rgba16(palette)
    num_pages = len(tex_pages)
    num_tiles = len(obj_textures)
    tile_dim = 32

    print(f"  Texture pages: {num_pages}")
    print(f"  Object textures: {num_tiles}")
    print(f"  Palette: {len(palette)} entries")

    # Quality gate threshold: max RMSE for CI4 acceptance
    CI4_QUALITY_THRESHOLD = 3.5  # RGB5 units, ~4 means visible banding

    # Process each tile
    ci8_tiles = []
    ci4_tiles = []  # (local_pal, packed_nibbles)
    tile_records = []  # (mode, palette_size)
    ci8_count = 0
    ci4_count = 0

    for i, otex in enumerate(obj_textures):
        ci8_tile = extract_tile(tex_pages, otex, tile_dim)
        ci8_tiles.append(ci8_tile)

        # Count unique palette indices
        unique = len(set(ci8_tile))

        if unique <= 16:
            # Fits CI4 exactly — no quality loss
            local_pal, packed, rmse = quantize_ci4(ci8_tile, global_pal_rgba16, tile_dim)
            ci4_tiles.append((local_pal, packed))
            tile_records.append((1, 16))  # mode=CI4
            ci4_count += 1
        else:
            # Try CI4 quantization, check quality
            local_pal, packed, rmse = quantize_ci4(ci8_tile, global_pal_rgba16, tile_dim)
            if rmse <= CI4_QUALITY_THRESHOLD:
                ci4_tiles.append((local_pal, packed))
                tile_records.append((1, 16))  # mode=CI4
                ci4_count += 1
            else:
                ci4_tiles.append(None)  # placeholder
                tile_records.append((0, 0))  # mode=CI8
                ci8_count += 1

    print(f"  Format selection: {ci8_count} CI8, {ci4_count} CI4")
    print(f"  CI4 acceptance rate: {ci4_count * 100 // max(num_tiles, 1)}%")

    # Build output
    # Header: 32 bytes
    header = struct.pack('<4sHHHH16s',
                         b'ATX1', num_tiles, tile_dim,
                         ci8_count, ci4_count,
                         b'\x00' * 16)

    # Per-tile records: 4 bytes each
    # Compute payload offsets
    records_data = bytearray()
    payload_data = bytearray()
    for i in range(num_tiles):
        mode, pal_size = tile_records[i]
        # Offset in 16-byte units from start of payload
        offset_16 = len(payload_data) // 16
        records_data += struct.pack('<BBH', mode, pal_size, offset_16)

        if mode == 0:
            # CI8: 1024 bytes
            payload_data += ci8_tiles[i]
        else:
            # CI4: 32 bytes palette + 512 bytes packed = 544 bytes
            local_pal, packed = ci4_tiles[i]
            for px in local_pal:
                payload_data += struct.pack('<H', px)
            payload_data += packed

        # Align to 16 bytes for DMA
        while len(payload_data) % 16:
            payload_data += b'\x00'

    # Write
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(records_data)
        f.write(payload_data)

    total = len(header) + len(records_data) + len(payload_data)
    ci8_bytes = ci8_count * 1024
    ci4_bytes = ci4_count * 544
    savings = (num_tiles * 1024) - (ci8_bytes + ci4_bytes)

    print(f"  Output: {output_path} ({total} bytes, {total / 1024:.1f} KB)")
    print(f"  CI8: {ci8_count} tiles × 1024B = {ci8_bytes / 1024:.1f} KB")
    print(f"  CI4: {ci4_count} tiles × 544B = {ci4_bytes / 1024:.1f} KB")
    print(f"  Savings vs all-CI8: {savings / 1024:.1f} KB ({savings * 100 // max(num_tiles * 1024, 1)}%)")

    # Also output the legacy .ci8tiles for backward compat
    ci8_output = output_path.replace('.atx', '.ci8tiles')
    ci8_header = struct.pack('<4sHH8s',
                             b'CI8T', num_tiles, tile_dim, b'\x00' * 8)
    with open(ci8_output, 'wb') as f:
        f.write(ci8_header)
        for tile in ci8_tiles:
            f.write(tile)
    print(f"  Legacy CI8: {ci8_output}")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <level.PHD> <output.atx>")
        print(f"  AegisTex texture compiler — CI4/CI8 format chooser")
        sys.exit(1)
    print(f"AegisTex Pack: {sys.argv[1]}")
    aegistex_pack(sys.argv[1], sys.argv[2])
