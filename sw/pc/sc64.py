#!/usr/bin/env python3

from ft232 import Ft232, Ft232Exception
from io import TextIOWrapper
import argparse
import filecmp
import os
import progressbar
import re
import struct
import sys
import time



class SC64Exception(Exception):
    pass



class SC64:
    __CFG_ID_DD_ENABLE = 3
    __CFG_ID_SAVE_TYPE = 4
    __CFG_ID_CIC_SEED = 5
    __CFG_ID_TV_TYPE = 6
    __CFG_ID_SAVE_OFFEST = 7
    __CFG_ID_DDIPL_OFFEST = 8
    __CFG_ID_BOOT_MODE = 9
    __CFG_ID_FLASH_SIZE = 10
    __CFG_ID_FLASH_READ = 11
    __CFG_ID_FLASH_PROGRAM = 12
    __CFG_ID_RECONFIGURE = 13
    __CFG_ID_DD_DRIVE_ID = 14
    __CFG_ID_DD_DISK_STATE = 15
    __CFG_ID_DD_THB_TABLE_OFFSET = 16
    __CFG_ID_IS_VIEWER_ENABLE = 17

    __SC64_VERSION_V2 = 0x53437632

    __UPDATE_OFFSET = 0x03B00000

    __CHUNK_SIZE = 256 * 1024

    __MIN_ROM_LENGTH = 0x101000
    __DDIPL_ROM_LENGTH = 0x400000

    __DEBUG_ID_TEXT = 0x01
    __EVENT_ID_FSD_READ = 0xF0
    __EVENT_ID_FSD_WRITE = 0xF1
    __EVENT_ID_FSD_LOAD = 0xF2
    __EVENT_ID_FSD_STORE = 0xF3
    __EVENT_ID_DD_BLOCK = 0xF4
    __EVENT_ID_IS_VIEWER = 0xF5

    __DD_DRIVE_ID_RETAIL = 3
    __DD_DRIVE_ID_DEVELOPMENT = 4
    __DD_DISK_STATE_EJECTED = 0
    __DD_DISK_STATE_INSERTED = 1
    __DD_DISK_STATE_CHANGED = 2


    def __init__(self) -> None:
        self.__usb = None
        self.__progress_init = None
        self.__progress_value = None
        self.__progress_finish = None
        self.__fsd_file = None
        self.__disk_file = None
        self.__disk_lba_table = []
        self.__dd_turbo = False
        self.__find_sc64()


    def __del__(self) -> None:
        if (self.__usb):
            self.__usb.close()
        if (self.__fsd_file):
            self.__fsd_file.close()


    def __set_progress_init(self, value: int, label: str) -> None:
        if (self.__progress_init != None):
            self.__progress_init(value, label)


    def __set_progress_value(self, value: int) -> None:
        if (self.__progress_value != None):
            self.__progress_value(value)


    def __set_progress_finish(self) -> None:
        if (self.__progress_finish != None):
            self.__progress_finish()


    def __align(self, value: int, align: int) -> int:
        return (value + (align - 1)) & ~(align - 1)


    def __escape(self, data: bytes) -> bytes:
        return re.sub(b"\x1B", b"\x1B\x1B", data)


    def reset_link(self) -> None:
        self.__usb.write(b"\x1BR")
        time.sleep(0.1)
        self.__usb.flushInput()


    def __read(self, bytes: int) -> bytes:
        return self.__usb.read(bytes)


    def __write(self, data: bytes) -> None:
        self.__usb.write(self.__escape(data))


    def __read_long(self, length: int) -> bytes:
        data = bytearray()
        while (len(data) < length):
            data += self.__read(length - len(data))
        return bytes(data)


    def __write_dummy(self, length: int) -> None:
        self.__write(bytes(length))


    def __read_int(self) -> int:
        return int.from_bytes(self.__read(4), byteorder="big")


    def __write_int(self, value: int) -> None:
        self.__write(value.to_bytes(4, byteorder="big"))


    def __read_cmd_status(self, cmd: str) -> None:
        if (self.__read(4) != f"CMP{cmd[0]}".encode(encoding="ascii")):
            raise SC64Exception("Wrong command response")


    def __write_cmd(self, cmd: str, arg1: int, arg2: int) -> None:
        self.__write(f"CMD{cmd[0]}".encode())
        self.__write_int(arg1)
        self.__write_int(arg2)


    def reset_n64(self) -> None:
        self.__usb.cbus_setup(mask=1, init=0)
        time.sleep(0.1)
        self.__usb.cbus_setup(mask=0)


    def __find_sc64(self) -> None:
        if (self.__usb != None and not self.__usb.closed):
            self.__usb.close()

        try:
            self.__usb = Ft232(description="SummerCart64")
            self.__usb.flushOutput()
            self.reset_link()
            self.__probe_device()
        except Ft232Exception as e:
            if (self.__usb):
                self.__usb.close()
            raise SC64Exception(f"No SummerCart64 device was found: {e}")


    def __probe_device(self) -> None:
        self.__write_cmd("V", 0, 0)
        version = self.__read_int()
        self.__read_cmd_status("V")
        if (version != self.__SC64_VERSION_V2):
            raise SC64Exception(f"Unknown hardware version: {hex(version)}")


    def __query_config(self, id: int) -> int:
        self.__write_cmd("Q", id, 0)
        value = self.__read_int()
        self.__read_cmd_status("Q")
        return value


    def __change_config(self, id: int, arg: int = 0, ignore_response: bool = False) -> None:
        self.__write_cmd("C", id, arg)
        if (not ignore_response):
            self.__read_cmd_status("C")


    def __read_file_from_sdram(self, file: str, offset: int, length: int, align: int = 2) -> None:
        with open(file, "wb") as f:
            transfer_size = self.__align(length, align)
            self.__set_progress_init(transfer_size, os.path.basename(f.name))
            self.__write_cmd("R", offset, transfer_size)
            while (f.tell() < length):
                chunk_size = min(self.__CHUNK_SIZE, length - f.tell())
                f.write(self.__read(chunk_size))
                self.__set_progress_value(f.tell())
            if (transfer_size != length):
                self.__read(transfer_size - length)
            self.__read_cmd_status("R")
            self.__set_progress_finish()


    def __write_file_to_sdram(self, file: str, offset: int, align: int = 2, min_length: int = 0) -> None:
        with open(file, "rb") as f:
            length = os.fstat(f.fileno()).st_size
            transfer_size = self.__align(max(length, min_length), align)
            self.__set_progress_init(transfer_size, os.path.basename(f.name))
            self.__write_cmd("W", offset, transfer_size)
            while (f.tell() < length):
                self.__write(f.read(min(self.__CHUNK_SIZE, length - f.tell())))
                self.__set_progress_value(f.tell())
            if (transfer_size != length):
                self.__write_dummy(transfer_size - length)
            self.__read_cmd_status("W")
            self.__set_progress_finish()


    def __get_save_length(self) -> None:
        save_type = self.__query_config(self.__CFG_ID_SAVE_TYPE)
        return {
            0: 0,
            1: 512,
            2: 2048,
            3: (32 * 1024),
            4: (128 * 1024),
            5: (3 * 32 * 1024),
            6: (128 * 1024)
        }[save_type]


    def __reconfigure(self) -> None:
        magic = self.__query_config(self.__CFG_ID_RECONFIGURE)
        self.__change_config(self.__CFG_ID_RECONFIGURE, magic, ignore_response=True)
        time.sleep(0.2)


    def backup_firmware(self, file: str) -> None:
        length = self.__query_config(self.__CFG_ID_FLASH_SIZE)
        self.__change_config(self.__CFG_ID_FLASH_READ, self.__UPDATE_OFFSET)
        self.__read_file_from_sdram(file, self.__UPDATE_OFFSET, length)


    def update_firmware(self, file: str) -> None:
        self.__write_file_to_sdram(file, self.__UPDATE_OFFSET)
        saved_timeout = self.__usb.timeout
        self.__usb.timeout = 20.0
        self.__change_config(self.__CFG_ID_FLASH_PROGRAM, self.__UPDATE_OFFSET)
        self.__usb.timeout = saved_timeout
        self.__reconfigure()
        self.__find_sc64()


    def set_boot_mode(self, mode: int) -> None:
        if (mode >= 0 and mode <= 4):
            self.__change_config(self.__CFG_ID_BOOT_MODE, mode)
        else:
            raise SC64Exception("Boot mode outside of supported values")


    def get_boot_mode_label(self, mode: int) -> None:
        if (mode < 0 or mode > 4):
            return "Unknown"
        return {
            0: "Load menu from SD card",
            1: "Load menu from USB",
            2: "Load ROM from SDRAM",
            3: "Load DDIPL from SDRAM",
            4: "Load ROM from SDRAM directly without bootloader",
        }[mode]


    def set_tv_type(self, type: int = None) -> None:
        if (type == None or type == 3):
            self.__change_config(self.__CFG_ID_TV_TYPE, 3)
        elif (type >= 0 and type <= 2):
            self.__change_config(self.__CFG_ID_TV_TYPE, type)
        else:
            raise SC64Exception("TV type outside of supported values")


    def get_tv_type_label(self, type: int) -> None:
        if (type < 0 or type > 2):
            return "Unknown"
        return {
            0: "PAL",
            1: "NTSC",
            2: "MPAL"
        }[type]


    def set_cic_seed(self, seed: int = None) -> None:
        if (seed == None or seed == 0xFFFF):
            self.__change_config(self.__CFG_ID_CIC_SEED, 0xFFFF)
        elif (seed >= 0 and seed <= 0x1FF):
            self.__change_config(self.__CFG_ID_CIC_SEED, seed)
        else:
            raise SC64Exception("CIC seed outside of supported values")


    def download_rom(self, file: str, length: int, offset: int = 0) -> None:
        self.__read_file_from_sdram(file, offset, length)


    def upload_rom(self, file: str, offset: int = 0) -> None:
        self.__write_file_to_sdram(file, offset, min_length=self.__MIN_ROM_LENGTH)


    def set_save_type(self, type: int) -> None:
        if (type >= 0 and type <= 6):
            self.__change_config(self.__CFG_ID_SAVE_TYPE, type)
        else:
            raise SC64Exception("Save type outside of supported values")


    def get_save_type_label(self, type: int) -> None:
        if (type < 0 or type > 6):
            return "Unknown"
        return {
            0: "No save",
            1: "EEPROM 4Kb",
            2: "EEPROM 16Kb",
            3: "SRAM 256Kb",
            4: "FlashRAM 1Mb",
            5: "SRAM 768Kb",
            6: "FlashRAM 1Mb (Pokemon Stadium 2 special case)"
        }[type]


    def download_save(self, file: str) -> None:
        length = self.__get_save_length()
        if (length > 0):
            offset = self.__query_config(self.__CFG_ID_SAVE_OFFEST)
            self.__read_file_from_sdram(file, offset, length)
        else:
            raise SC64Exception("Can't read save data - no save type is set")


    def upload_save(self, file: str) -> None:
        length = self.__get_save_length()
        save_length = os.path.getsize(file)
        if (length <= 0):
            raise SC64Exception("Can't write save data - no save type is set")
        elif (length != save_length):
            raise SC64Exception("Can't write save data - save file size is different than expected")
        else:
            offset = self.__query_config(self.__CFG_ID_SAVE_OFFEST)
            self.__write_file_to_sdram(file, offset)


    def set_dd_enable(self, enable: bool) -> None:
        self.__change_config(self.__CFG_ID_DD_ENABLE, 1 if enable else 0)


    def download_dd_ipl(self, file: str) -> None:
        dd_ipl_offset = self.__query_config(self.__CFG_ID_DDIPL_OFFEST)
        self.__read_file_from_sdram(file, dd_ipl_offset, length=self.__DDIPL_ROM_LENGTH)


    def upload_dd_ipl(self, file: str, offset: int = None) -> None:
        if (offset != None):
            self.__change_config(self.__CFG_ID_DDIPL_OFFEST, offset)
        dd_ipl_offset = self.__query_config(self.__CFG_ID_DDIPL_OFFEST)
        self.__write_file_to_sdram(file, dd_ipl_offset, min_length=self.__DDIPL_ROM_LENGTH)


    def set_dd_disk_state(self, state: str) -> None:
        state_mapping = {
            "ejected": self.__DD_DISK_STATE_EJECTED,
            "inserted": self.__DD_DISK_STATE_INSERTED,
            "changed": self.__DD_DISK_STATE_CHANGED,
        }
        if (state in state_mapping):
            self.__change_config(self.__CFG_ID_DD_DISK_STATE, state_mapping[state])
        else:
            raise SC64Exception("DD disk state outside of supported values")


    def __dd_create_configuration(self, handle: TextIOWrapper) -> tuple[str, int, list[tuple[int, int]]]:
        DISK_HEADS = 2
        DISK_TRACKS = 1175
        DISK_BLOCKS_PER_TRACK = 2
        DISK_SECTORS_PER_BLOCK = 85
        DISK_BAD_TRACKS_PER_ZONE = 12
        DISK_SYSTEM_SECTOR_SIZE = 232

        DISK_ZONES = [
            (0, 232, 158, 0),
            (0, 216, 158, 158),
            (0, 208, 149, 316),
            (0, 192, 149, 465),
            (0, 176, 149, 614),
            (0, 160, 149, 763),
            (0, 144, 149, 912),
            (0, 128, 114, 1061),
            (1, 216, 158, 157),
            (1, 208, 158, 315),
            (1, 192, 149, 464),
            (1, 176, 149, 613),
            (1, 160, 149, 762),
            (1, 144, 149, 911),
            (1, 128, 149, 1060),
            (1, 112, 114, 1174),
        ]

        DISK_VZONE_TO_PZONE = [
            [0, 1, 2, 9, 8, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10],
            [0, 1, 2, 3, 10, 9, 8, 4, 5, 6, 7, 15, 14, 13, 12, 11],
            [0, 1, 2, 3, 4, 11, 10, 9, 8, 5, 6, 7, 15, 14, 13, 12],
            [0, 1, 2, 3, 4, 5, 12, 11, 10, 9, 8, 6, 7, 15, 14, 13],
            [0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7, 15, 14],
            [0, 1, 2, 3, 4, 5, 6, 7, 14, 13, 12, 11, 10, 9, 8, 15],
            [0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8],
        ]

        DISK_DRIVE_TYPES = [(
            "development",
            192,
            [11, 10, 3, 2],
            [0, 1, 8, 9, 16, 17, 18, 19, 20, 21, 22, 23],
        ), (
            "retail",
            232,
            [9, 8, 1, 0],
            [2, 3, 10, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23],
        )]

        def __check_system_block(lba: int, sector_size: int, check_disk_type: bool) -> tuple[bool, bytes]:
            handle.seek(lba * DISK_SYSTEM_SECTOR_SIZE * DISK_SECTORS_PER_BLOCK)
            system_block_data = handle.read(sector_size * DISK_SECTORS_PER_BLOCK)
            system_data = system_block_data[:sector_size]
            for sector in range(1, DISK_SECTORS_PER_BLOCK):
                sector_data = system_block_data[(sector * sector_size):][:sector_size]
                if (system_data != sector_data):
                    return (False, None)
            if (check_disk_type):
                if (system_data[4] != 0x10):
                    return (False, None)
                if ((system_data[5] & 0xF0) != 0x10):
                    return (False, None)
            return (True, system_data)

        disk_drive_type = None
        disk_system_data = None
        disk_id_data = None
        disk_bad_lbas = []

        drive_index = 0
        while (disk_system_data == None) and (drive_index < len(DISK_DRIVE_TYPES)):
            (drive_type, system_sector_size, system_data_lbas, bad_lbas) = DISK_DRIVE_TYPES[drive_index]
            disk_bad_lbas.clear()
            disk_bad_lbas.extend(bad_lbas)
            for system_lba in system_data_lbas:
                (valid, system_data) = __check_system_block(system_lba, system_sector_size, check_disk_type=True)
                if (valid):
                    disk_drive_type = drive_type
                    disk_system_data = system_data
                else:
                    disk_bad_lbas.append(system_lba)
            drive_index += 1

        for id_lba in [15, 14]:
            (valid, id_data) = __check_system_block(id_lba, DISK_SYSTEM_SECTOR_SIZE, check_disk_type=False)
            if (valid):
                disk_id_data = id_data
            else:
                disk_bad_lbas.append(id_lba)

        if not (disk_system_data and disk_id_data):
            raise SC64Exception("Provided 64DD disk file is not valid")

        disk_zone_bad_tracks = []

        for zone in range(len(DISK_ZONES)):
            zone_bad_tracks = []
            start = 0 if zone == 0 else system_data[0x07 + zone]
            stop = system_data[0x07 + zone + 1]
            for offset in range(start, stop):
                zone_bad_tracks.append(system_data[0x20 + offset])
            for ignored_track in range(DISK_BAD_TRACKS_PER_ZONE - len(zone_bad_tracks)):
                zone_bad_tracks.append(DISK_ZONES[zone][2] - ignored_track - 1)
            disk_zone_bad_tracks.append(zone_bad_tracks)

        thb_lba_table = [(0xFFFFFFFF, -1)] * (DISK_HEADS * DISK_TRACKS * DISK_BLOCKS_PER_TRACK)

        disk_type = disk_system_data[5] & 0x0F

        current_lba = 0
        starting_block = 0
        disk_file_offset = 0

        for zone in DISK_VZONE_TO_PZONE[disk_type]:
            (head, sector_size, tracks, track) = DISK_ZONES[zone]

            for zone_track in range(tracks):
                current_zone_track = ((tracks - 1) - zone_track) if head else zone_track

                if (current_zone_track in disk_zone_bad_tracks[zone]):
                    track += (-1) if head else 1 
                    continue

                for block in range(DISK_BLOCKS_PER_TRACK):
                    if (current_lba not in disk_bad_lbas):
                        index = (track << 2) | (head << 1) | (starting_block ^ block)
                        thb_lba_table[index] = (disk_file_offset, current_lba)
                    disk_file_offset += sector_size * DISK_SECTORS_PER_BLOCK
                    current_lba += 1

                track += (-1) if head else 1
                starting_block ^= 1

        return (disk_drive_type, thb_lba_table)


    def set_dd_configuration_for_disk(self, file: str = None) -> None:
        if (file):
            with open(file, "rb+") as handle:
                (disk_drive_type, thb_lba_table) = self.__dd_create_configuration(handle)
                thb_table_offset = self.__query_config(self.__CFG_ID_DD_THB_TABLE_OFFSET)            
                data = bytearray()
                self.__disk_lba_table = [0xFFFFFFFF] * len(thb_lba_table)
                for (offset, lba) in thb_lba_table:
                    data += struct.pack(">I", offset)
                    self.__disk_lba_table[lba] = offset
                self.__write_cmd("W", thb_table_offset, len(data))
                self.__write(data)
                self.__read_cmd_status("W")
                id_mapping = {
                    "retail": self.__DD_DRIVE_ID_RETAIL,
                    "development": self.__DD_DRIVE_ID_DEVELOPMENT,
                }
                if (disk_drive_type in id_mapping):
                    self.__change_config(self.__CFG_ID_DD_DRIVE_ID, id_mapping[disk_drive_type])
                else:
                    raise SC64Exception("DD drive type outside of supported values")
        else:
            raise SC64Exception("No DD disk file provided for disk info creation")


    def __debug_process_fsd_read(self, data: bytes) -> None:
        sector = int.from_bytes(data[0:4], byteorder="little")
        offset = int.from_bytes(data[4:8], byteorder="little")

        if (self.__fsd_file):
            self.__fsd_file.seek(sector * 512)
            self.__write_cmd("T", offset, 512)
            self.__write(self.__fsd_file.read(512))
        else:
            self.__write_cmd("T", offset, 0)


    def __debug_process_fsd_write(self, data: bytes) -> None:
        sector = int.from_bytes(data[0:4], byteorder="little")
        offset = int.from_bytes(data[4:8], byteorder="little")

        if (self.__fsd_file):
            self.__fsd_file.seek(sector * 512)
            self.__write_cmd("F", offset, 512)
            self.__fsd_file.write(self.__read(512))
        else:
            self.__write_cmd("F", offset, 0)


    def __debug_process_dd_block(self, data: bytes) -> None:
        transfer_mode = int.from_bytes(data[0:4], byteorder="little")
        sdram_offset = int.from_bytes(data[4:8], byteorder="little")
        disk_file_offset = int.from_bytes(data[8:12], byteorder="big")
        block_length = int.from_bytes(data[12:16], byteorder="little")

        if (self.__disk_file):
            self.__disk_file.seek(disk_file_offset)
            if (transfer_mode):
                if (not self.__dd_turbo):
                    # Fixes weird bug in Mario Artist Paint Studio prototype minigame
                    time.sleep(0.016)
                self.__write_cmd("S", sdram_offset, block_length)
                self.__write(self.__disk_file.read(block_length))
            else:
                self.__write_cmd("L", sdram_offset, block_length)
                self.__disk_file.write(self.__read(block_length))


    def __debug_process_is_viewer(self, data: bytes) -> None:
        length = int.from_bytes(data[0:4], byteorder="little")
        address = int.from_bytes(data[4:8], byteorder="little")
        offset = (address & 0x01)
        transfer_length = self.__align(length + offset, 2)
        self.__write_cmd("L", (address & 0xFFFFFFFE), transfer_length)
        text = self.__read(transfer_length)[offset:][:length]
        print(text.decode("EUC-JP", errors="backslashreplace"), end="")


    def debug_init(self, fsd_file: str = None, disk_file: str = None, dd_turbo: bool = False) -> None:
        if (fsd_file):
            self.__fsd_file = open(fsd_file, "rb+")
        if (disk_file):
            self.__disk_file = open(disk_file, "rb+")
        self.__dd_turbo = dd_turbo


    def debug_loop(self, is_viewer_enabled: bool = False) -> None:
        self.__change_config(self.__CFG_ID_IS_VIEWER_ENABLE, is_viewer_enabled)

        print("\r\n\033[34m --- Debug server started --- \033[0m\r\n")

        self.__usb.setTimeout(0.1)

        start_indicator = bytearray()
        dropped_bytes = 0

        while (True):
            while (start_indicator != b"DMA@"):
                start_indicator.append(self.__read_long(1)[0])
                if (len(start_indicator) > 4):
                    dropped_bytes += 1
                    start_indicator.pop(0)
            start_indicator.clear()

            if (dropped_bytes):
                print(f"\033[35mWarning - dropped {dropped_bytes} bytes from stream\033[0m", file=sys.stderr)
                dropped_bytes = 0

            header = self.__read_long(4)
            id = int(header[0])
            length = int.from_bytes(header[1:4], byteorder="big")
            data = self.__read_long(length)
            self.__read_long(self.__align(length, 4) - length)
            end_indicator = self.__read_long(4)

            if (end_indicator != b"CMPH"):
                print(f"\033[35mGot unknown end indicator: {end_indicator.decode(encoding='ascii', errors='backslashreplace')}\033[0m", file=sys.stderr)
            else:
                if (id == self.__DEBUG_ID_TEXT):
                    print(data.decode(encoding="ascii", errors="backslashreplace"), end="")
                elif (id == self.__EVENT_ID_FSD_READ):
                    self.__debug_process_fsd_read(data)
                elif (id == self.__EVENT_ID_FSD_WRITE):
                    self.__debug_process_fsd_write(data)
                elif (id == self.__EVENT_ID_DD_BLOCK):
                    self.__debug_process_dd_block(data)
                elif (id == self.__EVENT_ID_IS_VIEWER):
                    self.__debug_process_is_viewer(data)
                else:
                    print(f"\033[35mGot unknown id: {id}, length: {length}\033[0m", file=sys.stderr)



class SC64ProgressBar:
    __LABEL_LENGTH = 30


    def __init__(self, sc64: SC64) -> None:
        self.__sc64 = sc64
        self.__widgets = [
            "[ ",
            progressbar.FormatLabel("{variables.label}", new_style=True),
            " | ",
            progressbar.Bar(left="", right=""),
            " ",
            progressbar.Percentage(),
            " | ",
            progressbar.DataSize(prefixes=("  ", "Ki", "Mi")),
            " | ",
            progressbar.AdaptiveTransferSpeed(),
            " ]"
        ]
        self.__variables = {"label": ""}
        self.__bar = None


    def __enter__(self) -> None:
        if (self.__sc64):
            self.__sc64._SC64__progress_init = self.__progress_init
            self.__sc64._SC64__progress_value = self.__progress_value
            self.__sc64._SC64__progress_finish = self.__progress_finish


    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        if (self.__sc64):
            self.__sc64._SC64__progress_init = None
            self.__sc64._SC64__progress_value = None
            self.__sc64._SC64__progress_finish = None


    def __progress_init(self, value: int, label: str) -> None:
        if (self.__bar == None):
            self.__bar = progressbar.ProgressBar(widgets=self.__widgets, variables=self.__variables)
        justified_label = label.ljust(self.__LABEL_LENGTH)
        if (len(justified_label) > self.__LABEL_LENGTH):
            justified_label = f"{justified_label[:self.__LABEL_LENGTH - 3]}..."
        self.__bar.variables.label = justified_label
        self.__bar.start(max_value=value)


    def __progress_value(self, value: int) -> None:
        self.__bar.update(value)


    def __progress_finish(self) -> None:
        self.__bar.finish(end="")
        print()



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SummerCart64 one stop control center")
    parser.add_argument("-b", metavar="boot_mode", default="2", required=False, help="set boot mode (0 - 4)")
    parser.add_argument("-t", metavar="tv_type", default="3", required=False, help="set TV type (0 - 2)")
    parser.add_argument("-c", metavar="cic_seed", default="0xFFFF", required=False, help="set CIC seed")
    parser.add_argument("-s", metavar="save_type", default="0", required=False, help="set save type (0 - 6)")
    parser.add_argument("-d", default=False, action="store_true", required=False, help="enable 64DD emulation")
    parser.add_argument("-df", default=False, action="store_true", required=False, help="set 64DD emulation to turbo mode")
    parser.add_argument("-r", default=False, action="store_true", required=False, help="perform reading operation instead of writing")
    parser.add_argument("-l", metavar="length", default="0x101000", required=False, help="specify ROM length to read")
    parser.add_argument("-u", metavar="update_path", default=None, required=False, help="path to update file")
    parser.add_argument("-e", metavar="save_path", default=None, required=False, help="path to save file")
    parser.add_argument("-i", metavar="ddipl_path", default=None, required=False, help="path to DDIPL file")
    parser.add_argument("-q", default=None, action="store_true", required=False, help="start debug server")
    parser.add_argument("-v", default=False, action="store_true", required=False, help="enable IS-Viewer64 support")
    parser.add_argument("-f", metavar="sd_path", default=None, required=False, help="path to disk or file for fake SD card emulation")
    parser.add_argument("-k", metavar="disk_path", default=None, required=False, help="path to 64DD disk file")
    parser.add_argument("rom", metavar="rom_path", default=None, help="path to ROM file", nargs="?")

    if (len(sys.argv) <= 1):
        parser.print_help()
        parser.exit()

    args = parser.parse_args()

    try:
        sc64 = SC64()

        boot_mode = int(args.b)
        save_type = int(args.s)
        tv_type = int(args.t)
        cic_seed = int(args.c, 0)
        dd_enable = args.d
        dd_turbo = args.df
        is_read = args.r
        rom_length = int(args.l, 0)
        update_file = args.u
        save_file = args.e
        dd_ipl_file = args.i
        rom_file = args.rom
        debug_server = args.q
        is_viewer_enabled = args.v
        sd_file = args.f
        disk_file = args.k

        firmware_backup_file = "sc64firmware.bin.bak"

        with SC64ProgressBar(sc64):
            if (update_file):
                if (is_read):
                    sc64.backup_firmware(update_file)
                else:
                    sc64.backup_firmware(firmware_backup_file)
                    if (not filecmp.cmp(update_file, firmware_backup_file)):
                        print("Update file different than contents of flash - updating SummerCart64")
                        sc64.update_firmware(update_file)
                    else:
                        print("SummerCart64 is already updated to latest version")
                    os.remove(firmware_backup_file)

            if (not is_read):
                if (boot_mode != 2):
                    print(f"Setting boot mode to [{sc64.get_boot_mode_label(boot_mode)}]")
                sc64.set_boot_mode(boot_mode)

                if (save_type):
                    print(f"Setting save type to [{sc64.get_save_type_label(save_type)}]")
                sc64.set_save_type(save_type)

                if (tv_type != 3):
                    print(f"Setting TV type to [{sc64.get_tv_type_label(tv_type)}]")
                sc64.set_tv_type(tv_type)

                if (cic_seed != 0xFFFF):
                    print(f"Setting CIC seed to [{hex(cic_seed) if cic_seed != 0xFFFF else 'Unknown'}]")
                sc64.set_cic_seed(cic_seed)

                if (dd_enable):
                    print(f"Setting 64DD emulation to [{'Enabled' if dd_enable else 'Disabled'}]")
                sc64.set_dd_enable(dd_enable)

            if (rom_file):
                if (is_read):            
                    if (rom_length > 0):
                        sc64.download_rom(rom_file, rom_length)
                    else:
                        raise SC64Exception("ROM length should be larger than 0")
                else:
                    sc64.upload_rom(rom_file)

            if (dd_ipl_file):
                if (is_read):
                    sc64.download_dd_ipl(dd_ipl_file)
                else:
                    sc64.upload_dd_ipl(dd_ipl_file)

            if (save_file):
                if (is_read):
                    sc64.download_save(save_file)
                else:
                    sc64.upload_save(save_file)
   
            sc64.reset_n64()

            if (debug_server):
                sc64.debug_init(sd_file, disk_file, dd_turbo)
                if (is_viewer_enabled):
                    print(f"Setting IS-Viewer 64 emulation to [Enabled]")
                if (sd_file):
                    print(f"Using fake SD emulation file [{sd_file}]")
                if (disk_file):
                    print(f"Using 64DD disk image file [{disk_file}]")
                    sc64.set_dd_configuration_for_disk(disk_file)
                    print(f"Setting 64DD disk state to [Changed]")
                sc64.set_dd_disk_state("changed" if disk_file else "ejected")
                sc64.debug_loop(is_viewer_enabled)

    except SC64Exception as e:
        print(f"Error: {e}")
        parser.exit(1)
    except KeyboardInterrupt:
        pass
    finally:
        if (sc64):
            sc64.reset_link()
            if (disk_file):
                print(f"Setting 64DD disk state to [Ejected]")
                sc64.set_dd_disk_state("ejected")
        sys.stdout.write("\033[0m")
