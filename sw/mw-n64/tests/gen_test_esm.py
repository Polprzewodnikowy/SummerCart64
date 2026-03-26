#!/usr/bin/env python3
"""
Generate a synthetic Morrowind .ESM file for testing the parser.

Creates a minimal ESM with:
  - TES3 header
  - 2 STAT records (static meshes)
  - 1 NPC_ record
  - 2 CELL records (1 interior, 1 exterior with refs)
  - 1 LAND record (heightmap for exterior cell)
"""

import struct
import os
import sys

def write_record(f, name, data):
    """Write a complete record: name(4) + size(4) + unused(4) + flags(4) + data."""
    f.write(name.encode()[:4].ljust(4))
    f.write(struct.pack('<I', len(data)))
    f.write(struct.pack('<I', 0))  # unused
    f.write(struct.pack('<I', 0))  # flags
    f.write(data)

def make_subrec(name, data):
    """Build a subrecord: name(4) + size(4) + data."""
    return name.encode()[:4].ljust(4) + struct.pack('<I', len(data)) + data

def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    outpath = os.path.join(outdir, "Test.esm")
    os.makedirs(outdir, exist_ok=True)

    with open(outpath, 'wb') as f:
        # TES3 Header
        hedr = struct.pack('<f', 1.3)       # version
        hedr += struct.pack('<I', 1)         # type (ESM)
        hedr += b'TestGen\x00' + b'\x00' * 24  # author (32 bytes)
        hedr += b'Test ESM for MW-N64\x00' + b'\x00' * 236  # description (256 bytes)
        hedr += struct.pack('<I', 6)         # record count
        tes3_data = make_subrec('HEDR', hedr)
        write_record(f, 'TES3', tes3_data)

        # STAT record: "test_rock"
        stat_data = make_subrec('NAME', b'test_rock\x00')
        stat_data += make_subrec('MODL', b'meshes\\test_rock.nif\x00')
        write_record(f, 'STAT', stat_data)

        # STAT record: "test_tree"
        stat_data = make_subrec('NAME', b'test_tree\x00')
        stat_data += make_subrec('MODL', b'meshes\\test_tree.nif\x00')
        write_record(f, 'STAT', stat_data)

        # NPC_ record: "test_npc"
        npc_data = make_subrec('NAME', b'test_npc\x00')
        npc_data += make_subrec('FNAM', b'Test NPC\x00')
        npc_data += make_subrec('RNAM', b'Imperial\x00')
        npc_data += make_subrec('CNAM', b'Warrior\x00')
        npc_data += make_subrec('MODL', b'meshes\\test_npc.nif\x00')
        # NPDT: 52 bytes
        npdt = struct.pack('<h', 5)         # level
        npdt += bytes([40,30,30,30,40,40,50,40])  # attributes
        npdt += bytes(27)                    # skills
        npdt += struct.pack('<3h', 100, 50, 200)  # HP, MP, fatigue
        npdt += struct.pack('<B', 50)        # disposition
        npdt += bytes(2)                     # reputation, rank
        npdt += struct.pack('<i', 100)       # gold
        npc_data += make_subrec('NPDT', npdt)
        npc_data += make_subrec('FLAG', struct.pack('<B', 0))
        write_record(f, 'NPC_', npc_data)

        # CELL record: interior "Test Room"
        cell_data = make_subrec('NAME', b'Test Room\x00')
        cell_data += make_subrec('DATA', struct.pack('<3i', 0x01, 0, 0))  # interior flag
        cell_data += make_subrec('WHGT', struct.pack('<f', 0.0))
        cell_data += make_subrec('AMBI',
            struct.pack('<4B', 80, 80, 80, 255) +   # ambient
            struct.pack('<4B', 200, 200, 180, 255) + # sunlight
            struct.pack('<4B', 120, 120, 140, 255) + # fog
            struct.pack('<f', 0.5))                   # fog density
        # Add 2 object references
        cell_data += make_subrec('FRMR', struct.pack('<I', 1))
        cell_data += make_subrec('NAME', b'test_rock\x00')
        cell_data += make_subrec('DATA', struct.pack('<6f', 100, 200, 0, 0, 0, 0))
        cell_data += make_subrec('FRMR', struct.pack('<I', 2))
        cell_data += make_subrec('NAME', b'test_npc\x00')
        cell_data += make_subrec('DATA', struct.pack('<6f', 300, 400, 0, 0, 0, 1.57))
        write_record(f, 'CELL', cell_data)

        # CELL record: exterior (0, 0)
        ext_data = make_subrec('NAME', b'\x00')  # exterior cells have empty name
        ext_data += make_subrec('DATA', struct.pack('<3i', 0x00, 0, 0))  # exterior, grid 0,0
        ext_data += make_subrec('FRMR', struct.pack('<I', 3))
        ext_data += make_subrec('NAME', b'test_tree\x00')
        ext_data += make_subrec('DATA', struct.pack('<6f', 4096, 4096, 0, 0, 0, 0))
        write_record(f, 'CELL', ext_data)

        # LAND record for exterior cell (0, 0)
        land_data = make_subrec('INTV', struct.pack('<2i', 0, 0))
        land_data += make_subrec('DATA', struct.pack('<I', 0x01))  # has heights
        # VHGT: float offset + 3 pad bytes + 65*65 height deltas
        vhgt = struct.pack('<f', 100.0)  # base offset
        vhgt += bytes(3)  # padding
        # Gentle hill: deltas create a small hill in the center
        deltas = bytearray(65 * 65)
        for y in range(65):
            for x in range(65):
                if 20 < x < 44 and 20 < y < 44:
                    deltas[y * 65 + x] = 1 if x == 21 else 0
                else:
                    deltas[y * 65 + x] = 0 if x > 0 else 0
        vhgt += bytes(deltas)
        land_data += make_subrec('VHGT', vhgt)
        write_record(f, 'LAND', land_data)

    size = os.path.getsize(outpath)
    print(f"Generated {outpath} ({size:,d} bytes)")
    print(f"  2 STAT, 1 NPC_, 2 CELL (1 interior + 1 exterior), 1 LAND")

if __name__ == "__main__":
    main()
