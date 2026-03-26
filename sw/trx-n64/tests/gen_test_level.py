#!/usr/bin/env python3
"""
Generate a synthetic TR1 .PHD level file for testing the parser.

Creates a minimal but structurally valid level with:
  - 2 rooms connected by a portal
  - Simple box geometry (8 vertices, 6 quads per room)
  - 1 texture page (solid colored tiles)
  - 1 object (Lara placeholder) with basic animation
  - Floor data with a trigger

This lets us test the entire TR1 pipeline without real game data.
"""

import struct
import os
import sys

def write_s16(f, v): f.write(struct.pack('<h', v))
def write_u16(f, v): f.write(struct.pack('<H', v))
def write_s32(f, v): f.write(struct.pack('<i', v))
def write_u32(f, v): f.write(struct.pack('<I', v))
def write_u8(f, v):  f.write(struct.pack('<B', v))
def write_s8(f, v):  f.write(struct.pack('<b', v))

def make_box_room(x, z, w, h, d, ambient=0x1000):
    """Generate vertices and quads for a box-shaped room."""
    verts = [
        (0, 0, 0), (w, 0, 0), (w, 0, d), (0, 0, d),      # Floor
        (0, -h, 0), (w, -h, 0), (w, -h, d), (0, -h, d),   # Ceiling
    ]
    # Floor quad
    quads = [
        (0, 1, 2, 3, 0),  # Floor (tex 0)
        (7, 6, 5, 4, 0),  # Ceiling
        (0, 4, 5, 1, 1),  # North wall (tex 1)
        (2, 6, 7, 3, 1),  # South wall
        (1, 5, 6, 2, 1),  # East wall
        (3, 7, 4, 0, 1),  # West wall
    ]
    return verts, quads

def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    outpath = os.path.join(outdir, "TEST.PHD")
    os.makedirs(outdir, exist_ok=True)

    with open(outpath, 'wb') as f:
        # Version
        write_u32(f, 32)  # TR1 version

        # Texture pages: 1 page of 256x256 8-bit
        write_s32(f, 1)  # num pages
        # Write 64KB of simple gradient pattern
        for y in range(256):
            for x in range(256):
                write_u8(f, (x + y) & 0xFF)

        # File level number
        write_s32(f, 0)

        # === ROOMS (2 rooms) ===
        write_s16(f, 2)  # num rooms

        for room_idx in range(2):
            room_x = room_idx * 2048
            room_z = 0

            write_s32(f, room_x)   # Room X
            write_s32(f, room_z)   # Room Z
            write_s32(f, 0)        # min_floor
            write_s32(f, -2048)    # max_ceiling

            verts, quads = make_box_room(room_x, room_z, 2048, 2048, 2048)

            # Mesh data length (in S16 words) — approximate
            mesh_len = 2 + len(verts)*4 + 2 + len(quads)*5 + 2 + 2  # verts + quads + tris + sprites
            write_u32(f, mesh_len)

            # Vertices
            write_s16(f, len(verts))
            for vx, vy, vz in verts:
                write_s16(f, vx)
                write_s16(f, vy)
                write_s16(f, vz)
                write_s16(f, 0x1000)  # light

            # Quads
            write_s16(f, len(quads))
            for v0, v1, v2, v3, tex in quads:
                write_u16(f, v0)
                write_u16(f, v1)
                write_u16(f, v2)
                write_u16(f, v3)
                write_u16(f, tex)

            # Tris
            write_s16(f, 0)

            # Sprites
            write_s16(f, 0)

            # Portal connecting room 0 to room 1
            if room_idx == 0:
                write_s16(f, 1)  # 1 portal
                write_s16(f, 1)  # dest room
                write_s16(f, 1); write_s16(f, 0); write_s16(f, 0)  # normal
                # Portal vertices (east wall opening)
                write_s16(f, 2048); write_s16(f, 0); write_s16(f, 0)
                write_s16(f, 2048); write_s16(f, 0); write_s16(f, 2048)
                write_s16(f, 2048); write_s16(f, -2048); write_s16(f, 2048)
                write_s16(f, 2048); write_s16(f, -2048); write_s16(f, 0)
            else:
                write_s16(f, 1)  # 1 portal back
                write_s16(f, 0)  # dest room 0
                write_s16(f, -1); write_s16(f, 0); write_s16(f, 0)
                write_s16(f, 0); write_s16(f, 0); write_s16(f, 0)
                write_s16(f, 0); write_s16(f, 0); write_s16(f, 2048)
                write_s16(f, 0); write_s16(f, -2048); write_s16(f, 2048)
                write_s16(f, 0); write_s16(f, -2048); write_s16(f, 0)

            # Sectors: 2x2 grid
            write_s16(f, 2)  # sectors_z
            write_s16(f, 2)  # sectors_x
            for _ in range(4):
                write_u16(f, 0)    # floor_data_idx
                write_s16(f, -1)   # box
                write_u8(f, 255)   # pit_room (none)
                write_s8(f, 0)     # floor height (0 = ground level)
                write_u8(f, 255)   # sky_room (none)
                write_s8(f, -8)    # ceiling (-8 * -256 = 2048)

            # Lights
            write_s16(f, 0x1000)  # ambient
            write_s16(f, 1)       # num lights
            write_s32(f, 1024); write_s32(f, -1024); write_s32(f, 1024)  # pos
            write_s16(f, 0x0800)  # intensity
            write_s32(f, 4096)    # falloff

            # Static meshes
            write_s16(f, 0)

            # Flipped room + flags
            write_s16(f, -1)  # no flip
            write_u16(f, 0)   # flags

        # Floor data (empty)
        write_s32(f, 0)

        # === OBJECT MESHES ===
        # Mesh data blob
        write_s32(f, 20)  # mesh data size (in S16 words)
        # Simple cube mesh
        for i in range(20):
            write_s16(f, 0)
        # Mesh pointers
        write_s32(f, 1)   # 1 mesh
        write_s32(f, 0)   # offset 0

        # === ANIMATIONS ===
        write_s32(f, 1)   # 1 animation
        # Anim data
        write_u32(f, 0)   # frame_offset
        write_u8(f, 1)    # interpolation
        write_u8(f, 20)   # frame_size
        write_s16(f, 0)   # state_id
        write_s32(f, 0)   # velocity
        write_s32(f, 0)   # acceleration
        write_s16(f, 0)   # frame_base
        write_s16(f, 0)   # frame_end
        write_s16(f, 0)   # jump_anim
        write_s16(f, 0)   # jump_frame
        write_s16(f, 0)   # num_changes
        write_s16(f, 0)   # change_idx
        write_s16(f, 0)   # num_commands
        write_s16(f, 0)   # command_idx

        # Anim changes
        write_s32(f, 0)
        # Anim ranges
        write_s32(f, 0)
        # Anim commands
        write_s32(f, 0)
        # Anim bones (4 S32 per bone)
        write_s32(f, 0)   # 0 bones
        # Anim frames
        write_s32(f, 10)  # 10 S16 words
        # Frame: bounds + offset + 1 rotation
        for i in range(10):
            write_s16(f, 0)

        # === OBJECTS ===
        write_s32(f, 1)   # 1 object (Lara)
        write_s32(f, 0)   # object_id = 0 (Lara)
        write_s16(f, 1)   # mesh_count
        write_s16(f, 0)   # mesh_idx
        write_s32(f, 0)   # bone_idx
        write_u32(f, 0)   # frame_offset
        write_s16(f, 0)   # anim_idx

        # Static objects
        write_s32(f, 0)

        # === TEXTURES ===
        # Object textures
        write_s32(f, 2)  # 2 textures
        for tex in range(2):
            write_u16(f, 0)  # draw_type (opaque)
            write_u16(f, 0)  # tex_page 0
            # UV coords (full 64x64 tile)
            x0 = tex * 64
            write_u16(f, x0 << 8); write_u16(f, 0)
            write_u16(f, (x0+63) << 8); write_u16(f, 0)
            write_u16(f, (x0+63) << 8); write_u16(f, 63 << 8)
            write_u16(f, x0 << 8); write_u16(f, 63 << 8)

        # Sprite textures
        write_s32(f, 0)
        # Sprite sequences
        write_s32(f, 0)

        # === CAMERAS ===
        write_s32(f, 0)
        # Sound sources
        write_s32(f, 0)
        # Boxes
        write_s32(f, 0)
        # Overlaps
        write_s32(f, 0)
        # Animated textures
        write_s32(f, 0)

        # === ITEMS ===
        write_s32(f, 1)  # 1 item (Lara)
        write_s16(f, 0)     # object_id = 0 (Lara)
        write_s16(f, 0)     # room = 0
        write_s32(f, 1024)  # pos.x
        write_s32(f, 0)     # pos.y
        write_s32(f, 1024)  # pos.z
        write_s16(f, 0)     # rotation
        write_s16(f, 0)     # shade
        write_u16(f, 0)     # flags

        # Light map (32 * 256 bytes)
        for i in range(32 * 256):
            write_u8(f, i & 0xFF)

        # Palette (256 * RGB)
        for i in range(256):
            write_u8(f, (i * 4) & 0xFF)   # R
            write_u8(f, (i * 2) & 0xFF)   # G
            write_u8(f, (63 - (i >> 2)) & 0x3F)  # B (6-bit)

        # Cinematic frames
        write_s16(f, 0)
        # Demo data
        write_u16(f, 0)

    size = os.path.getsize(outpath)
    print(f"Generated {outpath} ({size:,d} bytes)")
    print(f"  2 rooms, 1 portal, 1 texture page, 1 item (Lara)")

if __name__ == "__main__":
    main()
