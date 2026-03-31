#!/usr/bin/env python3
"""
CI8 Texture Tile Baker — Pre-bakes per-object CI8 tiles for ROM streaming.

For each object texture in a TR1 .PHD level, generates a 32×32 CI8 tile
(raw palette indices from the texture page). At runtime, these are DMA'd
from ROM on demand into a small cache instead of being fully resident
in RDRAM (~897 KB → ~128 KB cache).

Binary output format (.ci8tiles):
  Header (16 bytes):
    char[4]   magic = "CI8T"
    uint16_t  tile_count
    uint16_t  tile_dim = 32
    uint8_t[8] reserved

  Tiles (tile_count × 1024 bytes):
    uint8_t[32*32] raw CI8 indices, sequential

Usage:
    python3 bake_ci8_tiles.py <level.PHD> <output.ci8tiles>
"""

import struct
import sys
import os


def read_u16(data, pos):
    return struct.unpack_from('<H', data, pos)[0], pos + 2

def read_u32(data, pos):
    return struct.unpack_from('<I', data, pos)[0], pos + 4

def read_s32(data, pos):
    return struct.unpack_from('<i', data, pos)[0], pos + 4


def parse_for_ci8(data):
    """Parse .PHD to extract texture pages + object textures."""
    pos = 0
    version, pos = read_u32(data, pos)

    # Texture pages (256×256 CI8)
    num_pages, pos = read_u32(data, pos)
    tex_pages = []
    for i in range(num_pages):
        tex_pages.append(data[pos:pos + 65536])
        pos += 65536

    # Skip unused1
    pos += 4

    # Skip rooms
    num_rooms, pos = read_u16(data, pos)
    for ri in range(num_rooms):
        pos += 4 + 4 + 4 + 4  # x, z, y_bottom, y_top
        room_data_size, pos = read_u32(data, pos)
        room_data_end = pos + room_data_size * 2
        pos = room_data_end
        # Portals
        num_portals, pos = read_u16(data, pos)
        pos += num_portals * 32
        # Sectors
        num_z, pos = read_u16(data, pos)
        num_x, pos = read_u16(data, pos)
        pos += num_z * num_x * 8
        # Ambient + lights + static meshes
        ambient, pos = read_u16(data, pos)
        num_lights, pos = read_u16(data, pos)
        pos += num_lights * 18
        num_sm, pos = read_u16(data, pos)
        pos += num_sm * 18
        # Alt room + flags
        pos += 2 + 2

    # Floor data
    floor_size, pos = read_u32(data, pos)
    pos += floor_size * 2

    # Mesh data (raw)
    mesh_data_size, pos = read_u32(data, pos)
    pos += mesh_data_size * 2

    # Mesh pointers
    num_mesh_ptrs, pos = read_u32(data, pos)
    pos += num_mesh_ptrs * 4

    # Animations
    num_anims, pos = read_u32(data, pos)
    pos += num_anims * 32

    # Anim changes
    num_changes, pos = read_u32(data, pos)
    pos += num_changes * 6

    # Anim ranges
    num_ranges, pos = read_u32(data, pos)
    pos += num_ranges * 8

    # Anim commands
    num_cmds, pos = read_u32(data, pos)
    pos += num_cmds * 2

    # Mesh tree (bone data)
    num_tree, pos = read_u32(data, pos)
    pos += num_tree * 4

    # Anim frames
    num_frames, pos = read_u32(data, pos)
    pos += num_frames * 2

    # Objects
    num_objects, pos = read_u32(data, pos)
    pos += num_objects * 18

    # Static objects
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

    return tex_pages, obj_textures


def bake_ci8_tiles(phd_path, output_path):
    with open(phd_path, 'rb') as f:
        data = f.read()

    tex_pages, obj_textures = parse_for_ci8(data)
    num_pages = len(tex_pages)
    num_tiles = len(obj_textures)
    tile_dim = 32

    print(f"  Texture pages: {num_pages}")
    print(f"  Object textures: {num_tiles}")

    # Generate CI8 tiles — same algorithm as main_libdragon.c
    tiles = bytearray(num_tiles * tile_dim * tile_dim)

    for i, otex in enumerate(obj_textures):
        offset = i * tile_dim * tile_dim

        # UV bounding box
        us = [uv[0] for uv in otex['uvs']]
        vs = [uv[1] for uv in otex['uvs']]
        u0, u1 = min(us), max(us)
        v0, v1 = min(vs), max(vs)
        tw = max(u1 - u0 + 1, 1)
        th = max(v1 - v0 + 1, 1)

        if otex['page'] < num_pages:
            page = tex_pages[otex['page']]
            for y in range(tile_dim):
                sy = v0 + y * th // tile_dim
                if sy > 255:
                    sy = 255
                for x in range(tile_dim):
                    sx = u0 + x * tw // tile_dim
                    if sx > 255:
                        sx = 255
                    tiles[offset + y * tile_dim + x] = page[sy * 256 + sx]
        # else: stays zeroed

    # Write output
    header = struct.pack('<4sHH8s',
                         b'CI8T', num_tiles, tile_dim, b'\x00' * 8)

    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(tiles)

    total = len(header) + len(tiles)
    print(f"  Output: {output_path} ({total} bytes, {total/1024:.1f} KB)")
    print(f"  Per tile: {tile_dim}×{tile_dim} CI8 = {tile_dim*tile_dim} bytes")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <level.PHD> <output.ci8tiles>")
        sys.exit(1)
    print(f"Baking CI8 tiles: {sys.argv[1]}")
    bake_ci8_tiles(sys.argv[1], sys.argv[2])
