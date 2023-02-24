#!/usr/bin/env python3

import argparse
import os
import queue
import serial
import socket
import sys
import time
from binascii import crc32
from datetime import datetime
from enum import Enum, IntEnum
from io import BufferedReader
from serial.tools import list_ports
from threading import Thread
from typing import Callable, Optional
from PIL import Image



class BadBlockError(Exception):
    pass


class DD64Image:
    __DISK_HEADS = 2
    __DISK_TRACKS = 1175
    __DISK_BLOCKS_PER_TRACK = 2
    __DISK_SECTORS_PER_BLOCK = 85
    __DISK_BAD_TRACKS_PER_ZONE = 12
    __DISK_SYSTEM_SECTOR_SIZE = 232
    __DISK_ZONES = [
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
    __DISK_VZONE_TO_PZONE = [
        [0, 1, 2, 9, 8, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10],
        [0, 1, 2, 3, 10, 9, 8, 4, 5, 6, 7, 15, 14, 13, 12, 11],
        [0, 1, 2, 3, 4, 11, 10, 9, 8, 5, 6, 7, 15, 14, 13, 12],
        [0, 1, 2, 3, 4, 5, 12, 11, 10, 9, 8, 6, 7, 15, 14, 13],
        [0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7, 15, 14],
        [0, 1, 2, 3, 4, 5, 6, 7, 14, 13, 12, 11, 10, 9, 8, 15],
        [0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8],
    ]
    __DISK_DRIVE_TYPES = [(
        'development',
        192,
        [11, 10, 3, 2],
        [0, 1, 8, 9, 16, 17, 18, 19, 20, 21, 22, 23],
    ), (
        'retail',
        232,
        [9, 8, 1, 0],
        [2, 3, 10, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23],
    )]

    __file: Optional[BufferedReader]
    __drive_type: Optional[str]
    __block_info_table: list[tuple[int, int]]
    loaded: bool = False

    def __init__(self) -> None:
        self.__file = None
        self.__drive_type = None
        block_info_table_length = self.__DISK_HEADS * self.__DISK_TRACKS * self.__DISK_BLOCKS_PER_TRACK
        self.__block_info_table = [None] * block_info_table_length

    def __del__(self) -> None:
        self.unload()

    def __check_system_block(self, lba: int, sector_size: int, check_disk_type: bool) -> tuple[bool, bytes]:
        self.__file.seek(lba * self.__DISK_SYSTEM_SECTOR_SIZE * self.__DISK_SECTORS_PER_BLOCK)
        system_block_data = self.__file.read(sector_size * self.__DISK_SECTORS_PER_BLOCK)
        system_data = system_block_data[:sector_size]
        for sector in range(1, self.__DISK_SECTORS_PER_BLOCK):
            sector_data = system_block_data[(sector * sector_size):][:sector_size]
            if (system_data != sector_data):
                return (False, None)
        if (check_disk_type):
            if (system_data[4] != 0x10):
                return (False, None)
            if ((system_data[5] & 0xF0) != 0x10):
                return (False, None)
        return (True, system_data)

    def __parse_disk(self) -> None:
        disk_system_data = None
        disk_id_data = None
        disk_bad_lbas = []

        drive_index = 0
        while (disk_system_data == None) and (drive_index < len(self.__DISK_DRIVE_TYPES)):
            (drive_type, system_sector_size, system_data_lbas, bad_lbas) = self.__DISK_DRIVE_TYPES[drive_index]
            disk_bad_lbas.clear()
            disk_bad_lbas.extend(bad_lbas)
            for system_lba in system_data_lbas:
                (valid, system_data) = self.__check_system_block(system_lba, system_sector_size, check_disk_type=True)
                if (valid):
                    self.__drive_type = drive_type
                    disk_system_data = system_data
                else:
                    disk_bad_lbas.append(system_lba)
            drive_index += 1

        for id_lba in [15, 14]:
            (valid, id_data) = self.__check_system_block(id_lba, self.__DISK_SYSTEM_SECTOR_SIZE, check_disk_type=False)
            if (valid):
                disk_id_data = id_data
            else:
                disk_bad_lbas.append(id_lba)

        if not (disk_system_data and disk_id_data):
            raise ValueError('Provided 64DD disk file is not valid')

        disk_zone_bad_tracks = []

        for zone in range(len(self.__DISK_ZONES)):
            zone_bad_tracks = []
            start = 0 if zone == 0 else system_data[0x07 + zone]
            stop = system_data[0x07 + zone + 1]
            for offset in range(start, stop):
                zone_bad_tracks.append(system_data[0x20 + offset])
            for ignored_track in range(self.__DISK_BAD_TRACKS_PER_ZONE - len(zone_bad_tracks)):
                zone_bad_tracks.append(self.__DISK_ZONES[zone][2] - ignored_track - 1)
            disk_zone_bad_tracks.append(zone_bad_tracks)

        disk_type = disk_system_data[5] & 0x0F

        current_lba = 0
        starting_block = 0
        disk_file_offset = 0

        for zone in self.__DISK_VZONE_TO_PZONE[disk_type]:
            (head, sector_size, tracks, track) = self.__DISK_ZONES[zone]

            for zone_track in range(tracks):
                current_zone_track = (
                    (tracks - 1) - zone_track) if head else zone_track

                if (current_zone_track in disk_zone_bad_tracks[zone]):
                    track += (-1) if head else 1
                    continue

                for block in range(self.__DISK_BLOCKS_PER_TRACK):
                    index = (track << 2) | (head << 1) | (starting_block ^ block)
                    if (current_lba not in disk_bad_lbas):
                        self.__block_info_table[index] = (disk_file_offset, sector_size * self.__DISK_SECTORS_PER_BLOCK)
                    else:
                        self.__block_info_table[index] = None
                    disk_file_offset += sector_size * self.__DISK_SECTORS_PER_BLOCK
                    current_lba += 1

                track += (-1) if head else 1
                starting_block ^= 1

    def __check_track_head_block(self, track: int, head: int, block: int) -> None:
        if (track < 0 or track >= self.__DISK_TRACKS):
            raise ValueError('Track outside of possible range')
        if (head < 0 or head >= self.__DISK_HEADS):
            raise ValueError('Head outside of possible range')
        if (block < 0 or block >= self.__DISK_BLOCKS_PER_TRACK):
            raise ValueError('Block outside of possible range')

    def __get_table_index(self, track: int, head: int, block: int) -> int:
        return (track << 2) | (head << 1) | (block)

    def __get_block_info(self, track: int, head: int, block: int) -> Optional[tuple[int, int]]:
        if (self.__file.closed):
            return None
        self.__check_track_head_block(track, head, block)
        index = self.__get_table_index(track, head, block)
        return self.__block_info_table[index]

    def load(self, path: str) -> None:
        self.unload()
        self.__file = open(path, 'rb+')
        self.__parse_disk()
        self.loaded = True

    def unload(self) -> None:
        self.loaded = False
        if (self.__file != None and not self.__file.closed):
            self.__file.close()
        self.__drive_type = None

    def get_block_info_table(self) -> list[tuple[int, int]]:
        return self.__block_info_table

    def get_drive_type(self) -> str:
        return self.__drive_type

    def read_block(self, track: int, head: int, block: int) -> bytes:
        info = self.__get_block_info(track, head, block)
        if (info == None):
            raise BadBlockError
        (offset, block_size) = info
        self.__file.seek(offset)
        return self.__file.read(block_size)

    def write_block(self, track: int, head: int, block: int, data: bytes) -> None:
        info = self.__get_block_info(track, head, block)
        if (info == None):
            raise BadBlockError
        (offset, block_size) = info
        if (len(data) != block_size):
            raise ValueError(f'Provided data block size is different than expected ({len(data)} != {block_size})')
        self.__file.seek(offset)
        self.__file.write(data)


class ConnectionException(Exception):
    pass


class SC64Serial:
    __disconnect = False
    __serial: Optional[serial.Serial] = None
    __thread_read = None
    __thread_write = None
    __queue_output = queue.Queue()
    __queue_input = queue.Queue()
    __queue_packet = queue.Queue()

    __VID = 0x0403
    __PID = 0x6014

    __CHUNK_SIZE = (64 * 1024)

    def __init__(self) -> None:
        ports = list_ports.comports()
        device_found = False

        if (self.__serial != None and self.__serial.is_open):
            raise ConnectionException('Serial port is already open')

        for p in ports:
            if (p.vid == self.__VID and p.pid == self.__PID and p.serial_number.startswith('SC64')):
                try:
                    self.__serial = serial.Serial(p.device, timeout=1.0, write_timeout=1.0)
                    self.__reset_link()
                except (serial.SerialException, ConnectionException):
                    if (self.__serial):
                        self.__serial.close()
                    continue
                device_found = True
                break

        if (not device_found):
            raise ConnectionException('No SC64 device was found')

        self.__thread_read = Thread(target=self.__serial_process_input, daemon=True)
        self.__thread_write = Thread(target=self.__serial_process_output, daemon=True)

        self.__thread_read.start()
        self.__thread_write.start()

    def __del__(self) -> None:
        self.__disconnect = True
        if (self.__thread_read != None and self.__thread_read.is_alive()):
            self.__thread_read.join(1)
        if (self.__thread_write != None and self.__thread_write.is_alive()):
            self.__thread_write.join(1)
        if (self.__serial != None and self.__serial.is_open):
            self.__serial.close()

    def __reset_link(self) -> None:
        self.__serial.reset_output_buffer()

        retry_counter = 0
        self.__serial.dtr = 1
        while (self.__serial.dsr == 0):
            time.sleep(0.1)
            retry_counter += 1
            if (retry_counter >= 10):
                raise ConnectionException('Could not reset SC64 device')

        self.__serial.reset_input_buffer()

        retry_counter = 0
        self.__serial.dtr = 0
        while (self.__serial.dsr == 1):
            time.sleep(0.1)
            retry_counter += 1
            if (retry_counter >= 10):
                raise ConnectionException('Could not reset SC64 device')

    def __write(self, data: bytes) -> None:
        try:
            if (self.__disconnect):
                raise ConnectionException
            for offset in range(0, len(data), self.__CHUNK_SIZE):
                self.__serial.write(data[offset:offset + self.__CHUNK_SIZE])
            self.__serial.flush()
        except (serial.SerialException, serial.SerialTimeoutException):
            raise ConnectionException

    def __read(self, length: int) -> bytes:
        try:
            data = b''
            while (len(data) < length and not self.__disconnect):
                data += self.__serial.read(length - len(data))
            if (self.__disconnect):
                raise ConnectionException
            return data
        except serial.SerialException:
            raise ConnectionException

    def __read_int(self) -> int:
        return int.from_bytes(self.__read(4), byteorder='big')

    def __serial_process_output(self) -> None:
        while (not self.__disconnect and self.__serial != None and self.__serial.is_open):
            try:
                packet: bytes = self.__queue_output.get(timeout=0.1)
                self.__write(packet)
                self.__queue_output.task_done()
            except queue.Empty:
                continue
            except ConnectionException:
                break

    def __serial_process_input(self) -> None:
        while (not self.__disconnect and self.__serial != None and self.__serial.is_open):
            try:
                token = self.__read(4)
                if (len(token) == 4):
                    identifier = token[0:3]
                    command = token[3:4]
                    if (identifier == b'PKT'):
                        data = self.__read(self.__read_int())
                        self.__queue_packet.put((command, data))
                    elif (identifier == b'CMP' or identifier == b'ERR'):
                        data = self.__read(self.__read_int())
                        success = identifier == b'CMP'
                        self.__queue_input.put((command, data, success))
                    else:
                        raise ConnectionException
            except ConnectionException:
                break

    def __check_threads(self) -> None:
        if (not (self.__thread_write.is_alive() and self.__thread_read.is_alive())):
            raise ConnectionException('Serial link is closed')

    def __queue_cmd(self, cmd: bytes, args: list[int]=[0, 0], data: bytes=b'') -> None:
        if (len(cmd) != 1):
            raise ValueError('Length of command is different than 1 byte')
        if (len(args) != 2):
            raise ValueError('Number of arguments is different than 2')
        packet: bytes = b'CMD'
        packet += cmd[0:1]
        for arg in args:
            packet += arg.to_bytes(4, byteorder='big')
        packet += data
        self.__queue_output.put(packet)

    def __pop_response(self, cmd: bytes, timeout: float, raise_on_err: bool) -> bytes:
        try:
            (response_cmd, data, success) = self.__queue_input.get(timeout=timeout)
            self.__queue_input.task_done()
            if (cmd != response_cmd):
                raise ConnectionException('CMD wrong command response')
            if (raise_on_err and success == False):
                raise ConnectionException('CMD response error')
            return data
        except queue.Empty:
            raise ConnectionException('CMD response timeout')

    def execute_cmd(self, cmd: bytes, args: list[int]=[0, 0], data: bytes=b'', response: bool=True, timeout: float=5.0, raise_on_err: bool=True) -> Optional[bytes]:
        self.__check_threads()
        self.__queue_cmd(cmd, args, data)
        if (response):
            return self.__pop_response(cmd, timeout, raise_on_err)
        return None

    def get_packet(self, timeout: float=0.1) -> Optional[tuple[bytes, bytes]]:
        self.__check_threads()
        try:
            packet = self.__queue_packet.get(timeout=timeout)
            self.__queue_packet.task_done()
            return packet
        except queue.Empty:
            return None


class SC64:
    class __Address(IntEnum):
        MEMORY = 0x0000_0000
        SDRAM = 0x0000_0000
        FLASH = 0x0400_0000
        BUFFER = 0x0500_0000
        EEPROM = 0x0500_2000
        END = 0x0500_297F
        FIRMWARE = 0x0200_0000
        DDIPL = 0x03BC_0000
        SAVE = 0x03FE_0000
        EXTENDED = 0x0400_0000
        BOOTLOADER = 0x04E0_0000
        SHADOW = 0x04FE_0000

    class __Length(IntEnum):
        MEMORY = 0x0500_2980
        SDRAM = (64 * 1024 * 1024)
        FLASH = (16 * 1024 * 1024)
        BUFFER = (8 * 1024)
        EEPROM = (2 * 1024)
        DDIPL = (4 * 1024 * 1024)
        SAVE = (128 * 1024)
        EXTENDED = (14 * 1024 * 1024)
        BOOTLOADER = (1920 * 1024)
        SHADOW = (128 * 1024)

    class __SaveLength(IntEnum):
        NONE = 0
        EEPROM_4K = 512
        EEPROM_16K = (2 * 1024)
        SRAM = (32 * 1024)
        FLASHRAM = (128 * 1024)
        SRAM_BANKED = (3 * 32 * 1024)

    class __CfgId(IntEnum):
        BOOTLOADER_SWITCH = 0
        ROM_WRITE_ENABLE = 1
        ROM_SHADOW_ENABLE = 2
        DD_MODE = 3
        ISV_ADDRESS = 4
        BOOT_MODE = 5
        SAVE_TYPE = 6
        CIC_SEED = 7
        TV_TYPE = 8
        DD_SD_ENABLE = 9
        DD_DRIVE_TYPE = 10
        DD_DISK_STATE = 11
        BUTTON_STATE = 12
        BUTTON_MODE = 13
        ROM_EXTENDED_ENABLE = 14

    class __SettingId(IntEnum):
        LED_ENABLE = 0

    class __UpdateError(IntEnum):
        OK = 0
        TOKEN = 1
        CHECKSUM = 2
        SIZE = 3
        UNKNOWN_CHUNK = 4
        READ = 5

    class __UpdateStatus(IntEnum):
        MCU = 1
        FPGA = 2
        BOOTLOADER = 3
        DONE = 0x80
        ERROR = 0xFF

    class __DDMode(IntEnum):
        NONE = 0
        REGS = 1
        DDIPL = 2
        FULL = 3

    class __DDDriveType(IntEnum):
        RETAIL = 0
        DEVELOPMENT = 1

    class __DDDiskState(IntEnum):
        EJECTED = 0
        INSERTED = 1
        CHANGED = 2

    class __ButtonMode(IntEnum):
        NONE = 0
        N64_IRQ = 1
        USB_PACKET = 2
        DD_DISK_SWAP = 3

    class BootMode(IntEnum):
        MENU = 0
        ROM = 1
        DDIPL = 2
        DIRECT_ROM = 3
        DIRECT_DDIPL = 4

    class SaveType(IntEnum):
        NONE = 0
        EEPROM_4K = 1
        EEPROM_16K = 2
        SRAM = 3
        FLASHRAM = 4
        SRAM_BANKED = 5

    class CICSeed(IntEnum):
        DEFAULT = 0x3F
        X103 = 0x78
        X105 = 0x91
        X106 = 0x85
        ALECK = 0xAC
        DD_JP = 0xDD
        DD_US = 0xDE
        AUTO = 0xFFFF

    class TVType(IntEnum):
        PAL = 0
        NTSC = 1
        MPAL = 2
        AUTO = 3

    class __DebugDatatype(IntEnum):
        TEXT = 1
        RAWBINARY = 2
        HEADER = 3
        SCREENSHOT = 4
        GDB = 0xDB

    __SUPPORTED_MAJOR_VERSION = 2
    __SUPPORTED_MINOR_VERSION = 12

    __isv_line_buffer: bytes = b''
    __debug_header: Optional[bytes] = None
    __gdb_client: Optional[socket.socket] = None

    def __init__(self) -> None:
        self.__link = SC64Serial()
        identifier = self.__link.execute_cmd(cmd=b'v')
        if (identifier != b'SCv2'):
            raise ConnectionException('Unknown SC64 v2 identifier')

    def __get_int(self, data: bytes) -> int:
        return int.from_bytes(data[:4], byteorder='big')

    def check_firmware_version(self) -> tuple[str, bool]:
        try:
            version = self.__link.execute_cmd(cmd=b'V')
            major = self.__get_int(version[0:2])
            minor = self.__get_int(version[2:4])
            if (major != self.__SUPPORTED_MAJOR_VERSION):
                raise ConnectionException()
            if (minor < self.__SUPPORTED_MINOR_VERSION):
                raise ConnectionException()
            return (f'{major}.{minor}', minor > self.__SUPPORTED_MINOR_VERSION)
        except ConnectionException:
            raise ConnectionException(f'Unsupported SC64 version [{major}.{minor}], please update firmware')

    def __set_config(self, config: __CfgId, value: int) -> None:
        try:
            self.__link.execute_cmd(cmd=b'C', args=[config, value])
        except ConnectionException:
            raise ValueError(f'Could not set config {config.name} to {value:08X}')

    def __get_config(self, config: __CfgId) -> int:
        try:
            data = self.__link.execute_cmd(cmd=b'c', args=[config, 0])
        except ConnectionException:
            raise ValueError(f'Could not get config {config.name}')
        return self.__get_int(data)

    def __set_setting(self, setting: __SettingId, value: int) -> None:
        try:
            self.__link.execute_cmd(cmd=b'A', args=[setting, value])
        except ConnectionException:
            raise ValueError(f'Could not set setting {setting.name} to {value:08X}')

    def __get_setting(self, setting: __SettingId) -> int:
        try:
            data = self.__link.execute_cmd(cmd=b'a', args=[setting, 0])
        except ConnectionException:
            raise ValueError(f'Could not get setting {setting.name}')
        return self.__get_int(data)

    def __write_memory(self, address: int, data: bytes) -> None:
        if (len(data) > 0):
            self.__link.execute_cmd(cmd=b'M', args=[address, len(data)], data=data, timeout=20.0)

    def __read_memory(self, address: int, length: int) -> bytes:
        if (length > 0):
            return self.__link.execute_cmd(cmd=b'm', args=[address, length], timeout=20.0)
        return bytes([])

    def __dd_set_block_ready(self, error: int) -> None:
        self.__link.execute_cmd(cmd=b'D', args=[error, 0])

    def __flash_wait_busy(self) -> None:
        self.__link.execute_cmd(cmd=b'p', args=[True, 0])

    def __flash_get_erase_block_size(self) -> int:
        data = self.__link.execute_cmd(cmd=b'p', args=[False, 0])
        return self.__get_int(data[0:4])

    def __flash_erase_block(self, address: int) -> None:
        self.__link.execute_cmd(cmd=b'P', args=[address, 0])

    def __erase_flash_region(self, address: int, length: int) -> None:
        if (address < self.__Address.FLASH):
            raise ValueError('Flash erase address or length outside of possible range')
        if ((address + length) > (self.__Address.FLASH + self.__Length.FLASH)):
            raise ValueError('Flash erase address or length outside of possible range')
        erase_block_size = self.__flash_get_erase_block_size()
        if (address % erase_block_size != 0):
            raise ValueError('Flash erase address not aligned to block size')
        for offset in range(address, address + length, erase_block_size):
            self.__flash_erase_block(offset)

    def __program_flash(self, address: int, data: bytes):
        program_chunk_size = (128 * 1024)
        if (self.__read_memory(address, len(data)) != data):
            self.__erase_flash_region(address, len(data))
            for offset in range(0, len(data), program_chunk_size):
                self.__write_memory(address + offset, data[offset:offset + program_chunk_size])
            self.__flash_wait_busy()
            if (self.__read_memory(address, len(data)) != data):
                raise ConnectionException('Flash memory program failure')

    def autodetect_save_type(self, data: bytes) -> SaveType:
        if (len(data) < 0x40):
            return self.SaveType.NONE

        if (data[0x3C:0x3E] == b'ED'):
            save = (data[0x3F] >> 4) & 0x0F
            if (save < 0 or save > 6):
                return self.SaveType.NONE
            save_type_mapping = [
                self.SaveType.NONE,
                self.SaveType.EEPROM_4K,
                self.SaveType.EEPROM_16K,
                self.SaveType.SRAM,
                self.SaveType.SRAM_BANKED,
                self.SaveType.FLASHRAM,
                self.SaveType.SRAM,
            ]
            return save_type_mapping[save]

        # Original ROM database sourced from ares emulator: https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp

        rom_id = data[0x3B:0x3E]
        region = data[0x3E]
        revision = data[0x3F]

        save_type_mapping = [
            ([
                b'NTW', b'NHF', b'NOS', b'NTC', b'NER', b'NAG', b'NAB', b'NS3', b'NTN', b'NBN', b'NBK', b'NFH',
                b'NMU', b'NBC', b'NBH', b'NHA', b'NBM', b'NBV', b'NBD', b'NCT', b'NCH', b'NCG', b'NP2', b'NXO',
                b'NCU', b'NCX', b'NDY', b'NDQ', b'NDR', b'NN6', b'NDU', b'NJM', b'NFW', b'NF2', b'NKA', b'NFG',
                b'NGL', b'NGV', b'NGE', b'NHP', b'NPG', b'NIJ', b'NIC', b'NFY', b'NKI', b'NLL', b'NLR', b'NKT',
                b'CLB', b'NLB', b'NMW', b'NML', b'NTM', b'NMI', b'NMG', b'NMO', b'NMS', b'NMR', b'NCR', b'NEA',
                b'NPW', b'NPY', b'NPT', b'NRA', b'NWQ', b'NSU', b'NSN', b'NK2', b'NSV', b'NFX', b'NFP', b'NS6',
                b'NNA', b'NRS', b'NSW', b'NSC', b'NSA', b'NB6', b'NSS', b'NTX', b'NT6', b'NTP', b'NTJ', b'NRC',
                b'NTR', b'NTB', b'NGU', b'NIR', b'NVL', b'NVY', b'NWC', b'NAD', b'NWU', b'NYK', b'NMZ', b'NSM',
                b'NWR',
            ], self.SaveType.EEPROM_4K),
            ([
                b'NB7', b'NGT', b'NFU', b'NCW', b'NCZ', b'ND6', b'NDO', b'ND2', b'N3D', b'NMX', b'NGC', b'NIM',
                b'NNB', b'NMV', b'NM8', b'NEV', b'NPP', b'NUB', b'NPD', b'NRZ', b'NR7', b'NEP', b'NYS',
            ], self.SaveType.EEPROM_16K),
            ([
                b'NTE', b'NVB', b'NB5', b'CFZ', b'NFZ', b'NSI', b'NG6', b'NGP', b'NYW', b'NHY', b'NIB', b'NPS',
                b'NPA', b'NP4', b'NJ5', b'NP6', b'NPE', b'NJG', b'CZL', b'NZL', b'NKG', b'NMF', b'NRI', b'NUT',
                b'NUM', b'NOB', b'CPS', b'NPM', b'NRE', b'NAL', b'NT3', b'NS4', b'NA2', b'NVP', b'NWL', b'NW2',
                b'NWX',
            ], self.SaveType.SRAM),
            ([
                b'CDZ',
            ], self.SaveType.SRAM_BANKED),
            ([
                b'NCC', b'NDA', b'NAF', b'NJF', b'NKJ', b'NZS', b'NM6', b'NCK', b'NMQ', b'NPN', b'NPF', b'NPO',
                b'CP2', b'NP3', b'NRH', b'NSQ', b'NT9', b'NW4', b'NDP',
            ], self.SaveType.FLASHRAM),
        ]

        for (rom_id_table, save) in save_type_mapping:
            if (rom_id in rom_id_table):
                return save

        special_mapping = [
            (b'NKD', b'J', None, self.SaveType.EEPROM_4K),
            (b'NWT', b'J', None, self.SaveType.EEPROM_4K),
            (b'ND3', b'J', None, self.SaveType.EEPROM_16K),
            (b'ND4', b'J', None, self.SaveType.EEPROM_16K),
            (b'N3H', b'J', None, self.SaveType.SRAM),
            (b'NK4', b'J', 2,    self.SaveType.SRAM),
        ]

        for (special_rom_id, special_region, special_revision, save) in special_mapping:
            if (rom_id != special_rom_id):
                continue
            if (region != special_region):
                continue
            if (special_revision != None and revision >= special_revision):
                continue
            return save

        return self.SaveType.NONE

    def reset_state(self) -> None:
        self.__link.execute_cmd(cmd=b'R')

    def get_state(self):
        return {
            'bootloader_switch': bool(self.__get_config(self.__CfgId.BOOTLOADER_SWITCH)),
            'rom_write_enable': bool(self.__get_config(self.__CfgId.ROM_WRITE_ENABLE)),
            'rom_shadow_enable': bool(self.__get_config(self.__CfgId.ROM_SHADOW_ENABLE)),
            'dd_mode': self.__DDMode(self.__get_config(self.__CfgId.DD_MODE)),
            'isv_address': self.__get_config(self.__CfgId.ISV_ADDRESS),
            'boot_mode': self.BootMode(self.__get_config(self.__CfgId.BOOT_MODE)),
            'save_type': self.SaveType(self.__get_config(self.__CfgId.SAVE_TYPE)),
            'cic_seed': self.CICSeed(self.__get_config(self.__CfgId.CIC_SEED)),
            'tv_type': self.TVType(self.__get_config(self.__CfgId.TV_TYPE)),
            'dd_sd_enable': bool(self.__get_config(self.__CfgId.DD_SD_ENABLE)),
            'dd_drive_type': self.__DDDriveType(self.__get_config(self.__CfgId.DD_DRIVE_TYPE)),
            'dd_disk_state': self.__DDDiskState(self.__get_config(self.__CfgId.DD_DISK_STATE)),
            'button_state': bool(self.__get_config(self.__CfgId.BUTTON_STATE)),
            'button_mode': self.__ButtonMode(self.__get_config(self.__CfgId.BUTTON_MODE)),
            'rom_extended_enable': bool(self.__get_config(self.__CfgId.ROM_EXTENDED_ENABLE)),
            'led_enable': bool(self.__get_setting(self.__SettingId.LED_ENABLE)),
        }

    def debug_send(self, datatype: __DebugDatatype, data: bytes) -> None:
        if (len(data) > (8 * 1024 * 1024)):
            raise ValueError('Debug data size too big')
        self.__link.execute_cmd(cmd=b'U', args=[datatype, len(data)], data=data, response=False)

    def download_memory(self, address: int, length: int) -> bytes:
        if ((address < 0) or (length < 0) or ((address + length) > self.__Address.END)):
            raise ValueError('Invalid address or length')
        return self.__read_memory(address, length)

    def upload_rom(self, data: bytes, use_shadow: bool=True) -> None:
        rom_length = len(data)
        if (rom_length > (self.__Length.SDRAM + self.__Length.EXTENDED)):
            raise ValueError('ROM size too big')
        sdram_length = self.__Length.SDRAM
        shadow_enabled = use_shadow and rom_length > (self.__Length.SDRAM - self.__Length.SHADOW)
        extended_enabled = rom_length > self.__Length.SDRAM
        if (shadow_enabled):
            sdram_length = (self.__Length.SDRAM - self.__Length.SHADOW)
            shadow_data = data[sdram_length:sdram_length + self.__Length.SHADOW]
            self.__program_flash(self.__Address.SHADOW, shadow_data)
        self.__set_config(self.__CfgId.ROM_SHADOW_ENABLE, shadow_enabled)
        if (extended_enabled):
            extended_data = data[self.__Length.SDRAM:]
            self.__program_flash(self.__Address.EXTENDED, extended_data)
        self.__set_config(self.__CfgId.ROM_EXTENDED_ENABLE, extended_enabled)
        self.__write_memory(self.__Address.SDRAM, data[:sdram_length])

    def upload_ddipl(self, data: bytes) -> None:
        if (len(data) > self.__Length.DDIPL):
            raise ValueError('DDIPL size too big')
        self.__write_memory(self.__Address.DDIPL, data)

    def upload_save(self, data: bytes) -> None:
        save_type = self.SaveType(self.__get_config(self.__CfgId.SAVE_TYPE))
        if (save_type == self.SaveType.NONE):
            raise ValueError('No save type set inside SC64 device')
        if (len(data) != self.__SaveLength[save_type.name]):
            raise ValueError('Wrong save data length')
        address = self.__Address.SAVE
        if (save_type == self.SaveType.EEPROM_4K or save_type == self.SaveType.EEPROM_16K):
            address = self.__Address.EEPROM
        self.__write_memory(address, data)

    def download_save(self) -> bytes:
        save_type = self.SaveType(self.__get_config(self.__CfgId.SAVE_TYPE))
        if (save_type == self.SaveType.NONE):
            raise ValueError('No save type set inside SC64 device')
        address = self.__Address.SAVE
        length = self.__SaveLength[save_type.name]
        if (save_type == self.SaveType.EEPROM_4K or save_type == self.SaveType.EEPROM_16K):
            address = self.__Address.EEPROM
        return self.__read_memory(address, length)

    def upload_bootloader(self, data: bytes) -> None:
        if (len(data) > self.__Length.BOOTLOADER):
            raise ValueError('Bootloader size too big')
        padded_data = data + (b'\xFF' * (self.__Length.BOOTLOADER - len(data)))
        if (self.__read_memory(self.__Address.BOOTLOADER, self.__Length.BOOTLOADER) != padded_data):
            self.__erase_flash_region(self.__Address.BOOTLOADER, self.__Length.BOOTLOADER)
            self.__write_memory(self.__Address.BOOTLOADER, data)
            self.__flash_wait_busy()
            if (self.__read_memory(self.__Address.BOOTLOADER, self.__Length.BOOTLOADER) != padded_data):
                raise ConnectionException('Bootloader program failure')

    def set_rtc(self, t: datetime) -> None:
        to_bcd = lambda v: ((int((v / 10) % 10) << 4) | int(int(v) % 10))
        data = bytes([
            to_bcd(t.weekday() + 1),
            to_bcd(t.hour),
            to_bcd(t.minute),
            to_bcd(t.second),
            0,
            to_bcd(t.year),
            to_bcd(t.month),
            to_bcd(t.day),
        ])
        self.__link.execute_cmd(cmd=b'T', args=[self.__get_int(data[0:4]), self.__get_int(data[4:8])])

    def set_boot_mode(self, mode: BootMode) -> None:
        self.__set_config(self.__CfgId.BOOT_MODE, mode)

    def set_cic_seed(self, seed: int) -> None:
        if (seed != self.CICSeed.AUTO):
            if (seed < 0 or seed > 0xFF):
                raise ValueError('CIC seed outside of allowed values')
        self.__set_config(self.__CfgId.CIC_SEED, seed)

    def set_tv_type(self, type: TVType) -> bool:
        self.__set_config(self.__CfgId.TV_TYPE, type)
        boot_mode = self.__get_config(self.__CfgId.BOOT_MODE)
        direct = (boot_mode == self.BootMode.DIRECT_ROM) or (boot_mode == self.BootMode.DIRECT_DDIPL)
        return direct

    def set_save_type(self, type: SaveType) -> None:
        self.__set_config(self.__CfgId.SAVE_TYPE, type)

    def set_led_enable(self, enabled: bool) -> None:
        self.__set_setting(self.__SettingId.LED_ENABLE, enabled)

    def update_firmware(self, data: bytes, status_callback: Optional[Callable[[str], None]]=None) -> None:
        address = self.__Address.FIRMWARE
        self.__write_memory(address, data)
        response = self.__link.execute_cmd(cmd=b'F', args=[address, len(data)], raise_on_err=False)
        error = self.__UpdateError(self.__get_int(response[0:4]))
        if (error != self.__UpdateError.OK):
            raise ConnectionException(f'Bad update image [{error.name}]')
        status = None
        while status != self.__UpdateStatus.DONE:
            packet = self.__link.get_packet(timeout=60.0)
            if (packet == None):
                raise ConnectionException('Update timeout')
            (cmd, data) = packet
            if (cmd != b'F'):
                raise ConnectionException('Wrong update status packet')
            status = self.__UpdateStatus(self.__get_int(data))
            if (status_callback):
                status_callback(status.name)
            if (status == self.__UpdateStatus.ERROR):
                raise ConnectionException('Update error, device is most likely bricked')
        time.sleep(2)

    def backup_firmware(self) -> bytes:
        address = self.__Address.FIRMWARE
        info = self.__link.execute_cmd(cmd=b'f', args=[address, 0], timeout=60.0, raise_on_err=False)
        error = self.__UpdateError(self.__get_int(info[0:4]))
        length = self.__get_int(info[4:8])
        if (error != self.__UpdateError.OK):
            raise ConnectionException('Error while getting firmware backup')
        return self.__read_memory(address, length)

    def update_cic_parameters(self, seed: Optional[int]=None, disabled: Optional[bool]=False) -> tuple[int, int, bool, bool]:
        if ((seed != None) and (seed < 0 or seed > 0xFF)):
            raise ValueError('CIC seed outside of allowed values')
        boot_mode = self.__get_config(self.__CfgId.BOOT_MODE)
        address = self.__Address.BOOTLOADER
        if (boot_mode == self.BootMode.DIRECT_ROM):
            address = self.__Address.SDRAM
        elif (boot_mode == self.BootMode.DIRECT_DDIPL):
            address = self.__Address.DDIPL
        ipl3 = self.__read_memory(address, 4096)[0x40:0x1000]
        seed = seed if (seed != None) else self.__guess_ipl3_seed(ipl3)
        checksum = self.__calculate_ipl3_checksum(ipl3, seed)
        data = [(1 << 0) if disabled else 0, seed, *checksum.to_bytes(6, byteorder='big')]
        self.__link.execute_cmd(cmd=b'B', args=[self.__get_int(data[0:4]), self.__get_int(data[4:8])])
        direct = (boot_mode == self.BootMode.DIRECT_ROM) or (boot_mode == self.BootMode.DIRECT_DDIPL)
        return (seed, checksum, boot_mode == self.BootMode.DIRECT_DDIPL, direct)

    def __guess_ipl3_seed(self, ipl3: bytes) -> int:
        checksum = crc32(ipl3)
        seed_mapping = {
            0x587BD543: 0xAC,   # 5101
            0x6170A4A1: 0x3F,   # 6101
            0x009E9EA3: 0x3F,   # 7102
            0x90BB6CB5: 0x3F,   # 6102/7101
            0x0B050EE0: 0x78,   # x103
            0x98BC2C86: 0x91,   # x105
            0xACC8580A: 0x85,   # x106
            0x0E018159: 0xDD,   # 5167
            0x10C68B18: 0xDD,   # NDXJ0
            0xBC605D0A: 0xDD,   # NDDJ0
            0x502C4466: 0xDD,   # NDDJ1
            0x0C965795: 0xDD,   # NDDJ2
            0x8FEBA21E: 0xDE,   # NDDE0
        }
        return seed_mapping[checksum] if checksum in seed_mapping else 0x3F

    def __calculate_ipl3_checksum(self, ipl3: bytes, seed: int) -> int:
        _CHECKSUM_MAGIC = 0x6C078965

        _add = lambda a1, a2: ((a1 + a2) & 0xFFFFFFFF)
        _sub = lambda a1, a2: ((a1 - a2) & 0xFFFFFFFF)
        _mul = lambda a1, a2: ((a1 * a2) & 0xFFFFFFFF)
        _xor = lambda a1, a2: ((a1 ^ a2) & 0xFFFFFFFF)
        _lsh = lambda a, s: (((a & 0xFFFFFFFF) << s) & 0xFFFFFFFF)
        _rsh = lambda a, s: (((a & 0xFFFFFFFF) >> s) & 0xFFFFFFFF)

        def _get(offset: int) -> int:
            offset *= 4
            return int.from_bytes(ipl3[offset:(offset + 4)], byteorder='big')

        def _checksum(a0: int, a1: int, a2: int) -> int:
            prod = (a0 * (a2 if (a1 == 0) else a1))
            hi = ((prod >> 32) & 0xFFFFFFFF)
            lo = (prod & 0xFFFFFFFF)
            diff = ((hi - lo) & 0xFFFFFFFF)
            return ((a0 if (diff == 0) else diff) & 0xFFFFFFFF)

        if (seed < 0x00 or seed > 0xFF):
            raise ValueError('Invalid seed')

        buffer = [_xor(_add(_mul(_CHECKSUM_MAGIC, seed), 1), _get(0))] * 16

        for i in range(1, 1009):
            data_prev = data_curr if (i > 1) else _get(0)
            data_curr = _get(i - 1)
            data_next = _get(i)

            buffer[0] = _add(buffer[0], _checksum(_sub(1007, i), data_curr, i))
            buffer[1] = _checksum(buffer[1], data_curr, i)
            buffer[2] = _xor(buffer[2], data_curr)
            buffer[3] = _add(buffer[3], _checksum(_add(data_curr, 5), _CHECKSUM_MAGIC, i))

            shift = (data_prev & 0x1F)
            data_left = _lsh(data_curr, (32 - shift))
            data_right = _rsh(data_curr, shift)
            b4_shifted = (data_left | data_right)
            buffer[4] = _add(buffer[4], b4_shifted)

            shift = _rsh(data_prev, 27)
            data_left = _lsh(data_curr, shift)
            data_right = _rsh(data_curr, (32 - shift))
            b5_shifted = (data_left | data_right)
            buffer[5] = _add(buffer[5], b5_shifted)

            if (data_curr < buffer[6]):
                buffer[6] = _xor(_add(buffer[3], buffer[6]), _add(data_curr, i))
            else:
                buffer[6] = _xor(_add(buffer[4], data_curr), buffer[6])

            shift = (data_prev & 0x1F)
            data_left = _lsh(data_curr, shift)
            data_right = _rsh(data_curr, (32 - shift))
            buffer[7] = _checksum(buffer[7], (data_left | data_right), i)

            shift = _rsh(data_prev, 27)
            data_left = _lsh(data_curr, (32 - shift))
            data_right = _rsh(data_curr, shift)
            buffer[8] = _checksum(buffer[8], (data_left | data_right), i)

            if (data_prev < data_curr):
                buffer[9] = _checksum(buffer[9], data_curr, i)
            else:
                buffer[9] = _add(buffer[9], data_curr)

            if (i == 1008):
                break

            buffer[10] = _checksum(_add(buffer[10], data_curr), data_next, i)
            buffer[11] = _checksum(_xor(buffer[11], data_curr), data_next, i)
            buffer[12] = _add(buffer[12], _xor(buffer[8], data_curr))

            shift = (data_curr & 0x1F)
            data_left = _lsh(data_curr, (32 - shift))
            data_right = _rsh(data_curr, shift)
            tmp = (data_left | data_right)
            shift = (data_next & 0x1F)
            data_left = _lsh(data_next, (32 - shift))
            data_right = _rsh(data_next, shift)
            buffer[13] = _add(buffer[13], _add(tmp, (data_left | data_right)))

            shift = (data_curr & 0x1F)
            data_left = _lsh(data_next, (32 - shift))
            data_right = _rsh(data_next, shift)
            sum = _checksum(buffer[14], b4_shifted, i)
            buffer[14] = _checksum(sum, (data_left | data_right), i)

            shift = _rsh(data_curr, 27)
            data_left = _lsh(data_next, shift)
            data_right = _rsh(data_next, (32 - shift))
            sum = _checksum(buffer[15], b5_shifted, i)
            buffer[15] = _checksum(sum, (data_left | data_right), i)

        final_buffer = [buffer[0]] * 4

        for i in range(16):
            data = buffer[i]

            shift = (data & 0x1F)
            data_left = _lsh(data, (32 - shift))
            data_right = _rsh(data, shift)
            b0_shifted = _add(final_buffer[0], (data_left | data_right))
            final_buffer[0] = b0_shifted

            if (data < b0_shifted):
                final_buffer[1] = _add(final_buffer[1], data)
            else:
                final_buffer[1] = _checksum(final_buffer[1], data, i)

            if (_rsh((data & 0x02), 1) == (data & 0x01)):
                final_buffer[2] = _add(final_buffer[2], data)
            else:
                final_buffer[2] = _checksum(final_buffer[2], data, i)

            if (data & 0x01):
                final_buffer[3] = _xor(final_buffer[3], data)
            else:
                final_buffer[3] = _checksum(final_buffer[3], data, i)

        sum = _checksum(final_buffer[0], final_buffer[1], 16)
        xor = _xor(final_buffer[3], final_buffer[2])

        return ((sum << 32) | xor) & 0xFFFF_FFFFFFFF

    def __generate_filename(self, prefix: str, extension: str) -> str:
        return f'{prefix}-{datetime.now().strftime("%y%m%d%H%M%S.%f")}.{extension}'

    def __handle_dd_packet(self, dd: Optional[DD64Image], data: bytes) -> None:
        CMD_READ_BLOCK = 1
        CMD_WRITE_BLOCK = 2
        cmd = self.__get_int(data[0:])
        address = self.__get_int(data[4:])
        track_head_block = self.__get_int(data[8:])
        track = (track_head_block >> 2) & 0xFFF
        head = (track_head_block >> 1) & 0x1
        block = track_head_block & 0x1
        try:
            if (not dd or not dd.loaded):
                raise BadBlockError
            if (cmd == CMD_READ_BLOCK):
                block_data = dd.read_block(track, head, block)
                self.__write_memory(address, block_data)
                self.__dd_set_block_ready(0)
            elif (cmd == CMD_WRITE_BLOCK):
                block_data = data[12:]
                dd.write_block(track, head, block, block_data)
                self.__dd_set_block_ready(0)
            else:
                self.__dd_set_block_ready(1)
        except BadBlockError:
            self.__dd_set_block_ready(1)

    def __handle_isv_packet(self, data: bytes) -> None:
        self.__isv_line_buffer += data
        while (b'\n' in self.__isv_line_buffer):
            (line, self.__isv_line_buffer) = self.__isv_line_buffer.split(b'\n', 1)
            print(line.decode('EUC-JP', errors='backslashreplace'))

    def __handle_usb_packet(self, data: bytes) -> None:
        header = self.__get_int(data[0:4])
        datatype = self.__DebugDatatype((header >> 24) & 0xFF)
        length = (header & 0xFFFFFF)
        packet = data[4:]
        if (len(packet) != length):
            print(f'Debug packet length and data length did not match')
        elif (datatype == self.__DebugDatatype.TEXT):
            print(packet.decode('UTF-8', errors='backslashreplace'), end='')
        elif (datatype == self.__DebugDatatype.RAWBINARY):
            filename = self.__generate_filename('binaryout', 'bin')
            with open(filename, 'wb') as f:
                f.write(packet)
                print(f'Wrote {len(packet)} bytes to {filename}')
        elif (datatype == self.__DebugDatatype.HEADER):
            if (len(packet) == 16):
                self.__debug_header = packet
            else:
                print(f'Size of header packet is invalid: {len(packet)}')
        elif (datatype == self.__DebugDatatype.SCREENSHOT):
            filename = self.__generate_filename('screenshot', 'png')
            if (self.__debug_header != None):
                header_datatype = self.__get_int(self.__debug_header[0:4])
                pixel_format = self.__get_int(self.__debug_header[4:8])
                image_w = self.__get_int(self.__debug_header[8:12])
                image_h = self.__get_int(self.__debug_header[12:16])
                if (header_datatype == 0x04 and pixel_format != 0 and image_w != 0 and image_h != 0):
                    mode = 'RGBA' if pixel_format == 4 else 'I;16'
                    screenshot = Image.frombytes(mode, (image_w, image_h), packet)
                    screenshot.save(filename)
                    print(f'Wrote {image_w}x{image_h} pixels to {filename}')
                else:
                    print('Screenshot header data is invalid')
            else:
                print('Got screenshot packet without header data')
        elif (datatype == self.__DebugDatatype.GDB):
            self.__handle_gdb_datatype(packet)

    def __handle_gdb_socket(self, gdb_port: int) -> None:
        MAX_PACKET_SIZE = 65536
        gdb_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        gdb_socket.bind(('localhost', gdb_port))
        gdb_socket.listen()
        while (True):
            (self.__gdb_client, address) = gdb_socket.accept()
            client_address = f'{address[0]}:{address[1]}'
            print(f'[GDB]: New connection ({client_address})')
            try:
                connected = True
                while (connected):
                    data = self.__gdb_client.recv(MAX_PACKET_SIZE)
                    if (data):
                        self.debug_send(self.__DebugDatatype.GDB, data)
                    else:
                        connected = False
            except:
                pass
            finally:
                self.__gdb_client.close()
                print(f'[GDB]: Connection closed ({client_address})')

    def __handle_gdb_datatype(self, data: bytes) -> None:
        if (self.__gdb_client):
            try:
                self.__gdb_client.sendall(data)
            except:
                pass

    def __handle_debug_input(self) -> None:
        running = True
        while (running):
            try:
                command = input()
                if (len(command) > 0):
                    data = b''
                    datatype = self.__DebugDatatype.TEXT
                    if (command.count('@') != 2):
                        data += f'{command}\0'.encode()
                    else:
                        start = command.index('@')
                        end = command.rindex('@')
                        filename = command[(start + 1):end]
                        if (len(filename) == 0):
                            raise FileNotFoundError
                        with open(filename, 'rb') as f:
                            file_data = f.read()
                        if (command.startswith('@') and command.endswith('@')):
                            datatype = self.__DebugDatatype.RAWBINARY
                            data += file_data
                        else:
                            data += f'{command[:start]}@{len(file_data)}@'.encode()
                            data += file_data
                            data += b'\0'
                    self.debug_send(datatype, data)
            except ValueError as e:
                print(f'Error: {e}')
            except FileNotFoundError as e:
                print(f'Error: Cannot open file {e.filename}')
            except EOFError:
                running = False

    def debug_loop(self, isv: int=0, disks: Optional[list[str]]=None, gdb_port: Optional[int]=None) -> None:
        dd = None
        current_image = 0
        next_image = 0

        self.__set_config(self.__CfgId.ROM_WRITE_ENABLE, 1 if (isv != 0) else 0)
        self.__set_config(self.__CfgId.ISV_ADDRESS, isv)
        if (isv != 0):
            print(f'IS-Viewer64 support set to [ENABLED] at ROM offset [0x{(isv):08X}]')
            if (self.__get_config(self.__CfgId.ROM_SHADOW_ENABLE)):
                print('ROM shadow enabled - IS-Viewer64 will NOT work (use --no-shadow option while uploading ROM to disable it)')

        if (disks):
            dd = DD64Image()
            drive_type = None
            for path in disks:
                try:
                    dd.load(path)
                    if (drive_type == None):
                        drive_type = dd.get_drive_type()
                    elif (drive_type != dd.get_drive_type()):
                        raise ValueError('Disk drive type mismatch')
                    dd.unload()
                except ValueError as e:
                    dd = None
                    print(f'64DD disabled, incorrect disk images provided: {e}')
                    break
            if (dd):
                self.__set_config(self.__CfgId.DD_MODE, self.__DDMode.FULL)
                self.__set_config(self.__CfgId.DD_SD_ENABLE, False)
                self.__set_config(self.__CfgId.DD_DRIVE_TYPE, {
                    'retail': self.__DDDriveType.RETAIL,
                    'development': self.__DDDriveType.DEVELOPMENT
                }[drive_type])
                self.__set_config(self.__CfgId.DD_DISK_STATE, self.__DDDiskState.EJECTED)
                self.__set_config(self.__CfgId.BUTTON_MODE, self.__ButtonMode.USB_PACKET)
                print('64DD enabled, loaded disks:')
                for disk in disks:
                    print(f' - {os.path.basename(disk)}')
                print('Press button on SC64 device to cycle through provided disks')

        if (gdb_port):
            gdb_thread = Thread(target=lambda: self.__handle_gdb_socket(gdb_port), daemon=True)
            gdb_thread.start()
            print(f'GDB server started and listening on port [{gdb_port}]')

        print('\nDebug loop started, press Ctrl-C to exit\n')

        try:
            thread_input = Thread(target=self.__handle_debug_input, daemon=True)
            thread_input.start()
            while (thread_input.is_alive()):
                packet = self.__link.get_packet()
                if (packet != None):
                    (cmd, data) = packet
                    if (cmd == b'D'):
                        self.__handle_dd_packet(dd, data)
                    if (cmd == b'I'):
                        self.__handle_isv_packet(data)
                    if (cmd == b'U'):
                        self.__handle_usb_packet(data)
                    if (cmd == b'B'):
                        if (not dd.loaded):
                            dd.load(disks[next_image])
                            self.__set_config(self.__CfgId.DD_DISK_STATE, self.__DDDiskState.INSERTED)
                            current_image = next_image
                            next_image += 1
                            if (next_image >= len(disks)):
                                next_image = 0
                            print(f'64DD disk inserted - {os.path.basename(disks[current_image])}')
                        else:
                            self.__set_config(self.__CfgId.DD_DISK_STATE, self.__DDDiskState.EJECTED)
                            dd.unload()
                            print(f'64DD disk ejected - {os.path.basename(disks[current_image])}')
        except KeyboardInterrupt:
            pass
        finally:
            print('\nDebug loop stopped\n')

        if (dd and dd.loaded):
            self.__set_config(self.__CfgId.DD_DISK_STATE, self.__DDDiskState.EJECTED)
        if (isv != 0):
            self.__set_config(self.__CfgId.ISV_ADDRESS, 0)


class EnumAction(argparse.Action):
    def __init__(self, **kwargs):
        type = kwargs.pop('type', None)
        if type is None:
            raise ValueError('No type was provided')
        if not issubclass(type, Enum):
            raise TypeError('Provided type is not an Enum subclass')
        items = (choice.lower().replace('_', '-') for (choice, _) in type.__members__.items())
        kwargs.setdefault('choices', tuple(items))
        super(EnumAction, self).__init__(**kwargs)
        self.__enum = type

    def __call__(self, parser, namespace, values, option_string):
        key = str(values).upper().replace('-', '_')
        value = self.__enum[key]
        setattr(namespace, self.dest, value)



if __name__ == '__main__':
    def download_memory_type(argument: str):
        params = argument.split(',')
        if (len(params) < 2 or len(params) > 3):
            raise argparse.ArgumentError()
        address = int(params[0], 0)
        length = int(params[1], 0)
        file = params[2] if len(params) >= 3 else 'sc64dump.bin'
        return (address, length, file)

    parser = argparse.ArgumentParser(description='SC64 control software')
    parser.add_argument('rom', nargs='?', help='upload ROM from specified file')
    parser.add_argument('--backup-firmware', metavar='file', help='backup SC64 firmware and write it to specified file')
    parser.add_argument('--update-firmware', metavar='file', help='update SC64 firmware from specified file')
    parser.add_argument('--reset-state', action='store_true', help='reset SC64 internal state')
    parser.add_argument('--print-state', action='store_true', help='print SC64 internal state')
    parser.add_argument('--led-blink', metavar='{yes,no}', help='enable or disable LED I/O activity blinking')
    parser.add_argument('--rtc', action='store_true', help='update clock in SC64 to system time')
    parser.add_argument('--boot', type=SC64.BootMode, action=EnumAction, help='set boot mode')
    parser.add_argument('--tv', type=SC64.TVType, action=EnumAction, help='force TV type to set value, ignored when one of direct boot modes are selected')
    parser.add_argument('--no-shadow', action='store_false', help='do not put last 128 kB of ROM inside flash memory (can corrupt non EEPROM saves)')
    parser.add_argument('--save-type', type=SC64.SaveType, action=EnumAction, help='set save type')
    parser.add_argument('--save', metavar='file', help='upload save from specified file')
    parser.add_argument('--backup-save', metavar='file', help='download save and write it to specified file')
    parser.add_argument('--ddipl', metavar='file', help='upload 64DD IPL from specified file')
    parser.add_argument('--disk', metavar='file', action='append', help='path to 64DD disk (.ndd format), can be specified multiple times')
    parser.add_argument('--isv', metavar='offset', type=lambda x: int(x, 0), default=0, help='enable IS-Viewer64 support at provided ROM offset')
    parser.add_argument('--gdb', metavar='port', type=int, help='expose TCP socket port for GDB debugging')
    parser.add_argument('--debug', action='store_true', help='run debug loop')
    parser.add_argument('--download-memory', metavar='address,length,[file]', type=download_memory_type, help='download specified memory region and write it to file')

    if (len(sys.argv) <= 1):
        parser.print_help()
        parser.exit()

    args = parser.parse_args()

    def fix_rom_endianness(rom: bytes) -> bytes:
        data = bytearray(rom)
        pi_config = int.from_bytes(rom[0:4], byteorder='big')
        if (pi_config == 0x37804012):
            data[0::2], data[1::2] = data[1::2], data[0::2]
        elif (pi_config == 0x40123780):
            data[0::4], data[1::4], data[2::4], data[3::4] = data[3::4], data[2::4], data[1::4], data[0::4]
        return bytes(data)

    try:
        sc64 = SC64()
        autodetected_save_type = None

        if (args.backup_firmware):
            with open(args.backup_firmware, 'wb') as f:
                print('Generating backup, this might take a while... ', end='', flush=True)
                f.write(sc64.backup_firmware())
                print('done')

        if (args.update_firmware):
            with open(args.update_firmware, 'rb') as f:
                print('Updating firmware, this might take a while... ', end='', flush=True)
                status_callback = lambda status: print(f'{status} ', end='', flush=True)
                sc64.update_firmware(f.read(), status_callback)
                print('done')

        (version, script_outdated) = sc64.check_firmware_version()

        print(f'\x1b[32mSC64 firmware version: [{version}]\x1b[0m')
        if (script_outdated):
            print('\x1b[33m')
            print('[      SC64 firmware is newer than last known version       ]')
            print('[     Consider downloading latest sc64 executable from      ]')
            print('[ https://github.com/Polprzewodnikowy/SummerCart64/releases ]')
            print('\x1b[0m')

        if (args.reset_state):
            sc64.reset_state()
            print('SC64 internal state reset')

        if (args.led_blink):
            blink = (args.led_blink == 'yes')
            sc64.set_led_enable(blink)
            print(f'LED blinking set to [{"ENABLED" if blink else "DISABLED"}]')

        if (args.rtc):
            value = datetime.now()
            sc64.set_rtc(value)
            print(f'RTC set to [{value.strftime("%Y-%m-%d %H:%M:%S")}]')

        if (args.rom):
            with open(args.rom, 'rb') as f:
                rom_data = fix_rom_endianness(f.read())
                print(f'Uploading ROM ({len(rom_data) / (1 * 1024 * 1024):.2f} MiB)... ', end='', flush=True)
                sc64.upload_rom(rom_data, use_shadow=args.no_shadow)
                autodetected_save_type = sc64.autodetect_save_type(rom_data)
                print('done')

        if (args.ddipl):
            with open(args.ddipl, 'rb') as f:
                print('Uploading 64DD IPL... ', end='', flush=True)
                sc64.upload_ddipl(f.read())
                print('done')

        if (args.rom or args.ddipl or args.boot != None):
            mode = args.boot
            if (mode == None):
                mode = SC64.BootMode.ROM if args.rom else SC64.BootMode.DDIPL
            sc64.set_boot_mode(mode)
            print(f'Boot mode set to [{mode.name}]')
            (seed, checksum, dd_mode, direct) = sc64.update_cic_parameters()
            if (direct):
                print('CIC parameters set to [', end='')
                print(f'{"DDIPL" if dd_mode else "ROM"}, ', end='')
                print(f'seed: 0x{seed:02X}, checksum: 0x{checksum:012X}', end='')
                print(']')

        if (args.rom or args.ddipl or args.tv != None):
            tv = args.tv if args.tv else SC64.TVType.AUTO
            direct = sc64.set_tv_type(tv)
            if (args.tv != None):
                print(f'TV type set to [{args.tv.name}]{" (ignored)" if direct else ""}')

        if (args.save_type != None or autodetected_save_type != None):
            save_type = args.save_type if args.save_type != None else autodetected_save_type
            sc64.set_save_type(save_type)
            print(f'Save type set to [{save_type.name}]{" (autodetected)" if autodetected_save_type != None else ""}')

        if (args.save):
            with open(args.save, 'rb') as f:
                print('Uploading save... ', end='', flush=True)
                sc64.upload_save(f.read())
                print('done')

        if (args.print_state):
            state = sc64.get_state()
            print('Current SC64 internal state:')
            for key, value in state.items():
                if (hasattr(value, 'name')):
                    value = getattr(value, 'name')
                print(f'  {key}: {value}')

        if (args.debug or args.isv or args.disk or args.gdb):
            sc64.debug_loop(isv=args.isv, disks=args.disk, gdb_port=args.gdb)

        if (args.backup_save):
            with open(args.backup_save, 'wb') as f:
                print('Downloading save... ', end='', flush=True)
                f.write(sc64.download_save())
                print('done')

        if (args.download_memory != None):
            (address, length, file) = args.download_memory
            with open(file, 'wb') as f:
                print('Downloading memory... ', end='', flush=True)
                f.write(sc64.download_memory(address, length))
                print('done')
    except ValueError as e:
        print(f'\n\x1b[31mValue error: {e}\x1b[0m\n')
    except ConnectionException as e:
        print(f'\n\x1b[31mSC64 error: {e}\x1b[0m\n')
    except Exception as e:
        print(f'\n\x1b[31mUnhandled error "{e.__class__.__name__}": {e}\x1b[0m\n')
