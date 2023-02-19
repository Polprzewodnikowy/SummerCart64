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
from dd64 import BadBlockError, DD64Image
from enum import Enum, IntEnum
from serial.tools import list_ports
from threading import Thread
from typing import Callable, Optional
from PIL import Image



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

    __MIN_SUPPORTED_API_VERSION = 2

    __isv_line_buffer: bytes = b''
    __debug_header: Optional[bytes] = None
    __gdb_client: Optional[socket.socket] = None

    def __init__(self) -> None:
        self.__link = SC64Serial()
        version = self.__link.execute_cmd(cmd=b'v')
        if (version != b'SCv2'):
            raise ConnectionException('Unknown SC64 HW version')

    def __get_int(self, data: bytes) -> int:
        return int.from_bytes(data[:4], byteorder='big')

    def check_api_version(self) -> None:
        try:
            data = self.__link.execute_cmd(cmd=b'V')
        except ConnectionException:
            raise ConnectionException('Outdated SC64 API, please update firmware')
        version = self.__get_int(data)
        if (version < self.__MIN_SUPPORTED_API_VERSION):
            raise ConnectionException('Unsupported SC64 API version, please update firmware')

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

    def autodetect_save_type(self, data: bytes) -> Optional[SaveType]:
        if (len(data) < 0x40):
            return None
        if (data[0x3C:0x3E] != b'ED'):
            return None
        save = (data[0x3F] >> 4) & 0x0F
        if (save < 0 or save > 6):
            return None
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

    def download_memory(self) -> bytes:
        return self.__read_memory(self.__Address.MEMORY, self.__Length.MEMORY)

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

    def set_tv_type(self, type: TVType) -> None:
        self.__set_config(self.__CfgId.TV_TYPE, type)

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

    def set_cic_parameters(self, seed: Optional[int]=None, disabled=False) -> tuple[int, int, bool]:
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
        print(data)
        self.__link.execute_cmd(cmd=b'B', args=[self.__get_int(data[0:4]), self.__get_int(data[4:8])])
        return (seed, checksum, boot_mode == self.BootMode.DIRECT_DDIPL)

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

        self.__set_config(self.__CfgId.ROM_WRITE_ENABLE, isv)
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
    def cic_params_type(argument: str):
        params = argument.split(',')
        if (len(params) > 2):
            raise argparse.ArgumentError()
        seed = int(params[0], 0) if len(params) >= 1 else None
        disabled = bool(int(params[1])) if len(params) >= 2 else None
        return (seed, disabled)

    parser = argparse.ArgumentParser(description='SC64 control software')
    parser.add_argument('--backup-firmware', metavar='file', help='backup SC64 firmware and write it to specified file')
    parser.add_argument('--update-firmware', metavar='file', help='update SC64 firmware from specified file')
    parser.add_argument('--update-bootloader', metavar='file', help='update SC64 bootloader (not recommended, use --update-firmware instead)')
    parser.add_argument('--reset-state', action='store_true', help='reset SC64 internal state')
    parser.add_argument('--print-state', action='store_true', help='print SC64 internal state')
    parser.add_argument('--led-enabled', metavar='{true,false}', help='disable or enable LED I/O activity blinking')
    parser.add_argument('--boot', type=SC64.BootMode, action=EnumAction, help='set boot mode')
    parser.add_argument('--tv', type=SC64.TVType, action=EnumAction, help='force TV type to set value')
    parser.add_argument('--cic', type=SC64.CICSeed, action=EnumAction, help='force CIC seed to set value')
    parser.add_argument('--cic-params', metavar='seed,[disabled]', type=cic_params_type, help='set CIC emulation parameters')
    parser.add_argument('--rtc', action='store_true', help='update clock in SC64 to system time')
    parser.add_argument('--no-shadow', action='store_false', help='do not put last 128 kB of ROM inside flash memory (can corrupt non EEPROM saves)')
    parser.add_argument('--rom', metavar='file', help='upload ROM from specified file')
    parser.add_argument('--save-type', type=SC64.SaveType, action=EnumAction, help='set save type')
    parser.add_argument('--save', metavar='file', help='upload save from specified file')
    parser.add_argument('--backup-save', metavar='file', help='download save and write it to specified file')
    parser.add_argument('--ddipl', metavar='file', help='upload 64DD IPL from specified file')
    parser.add_argument('--disk', metavar='file', action='append', help='path to 64DD disk (.ndd format), can be specified multiple times')
    parser.add_argument('--isv', type=lambda x: int(x, 0), default=0, help='enable IS-Viewer64 support at provided ROM offset')
    parser.add_argument('--gdb', metavar='port', type=int, help='expose socket port for GDB debugging')
    parser.add_argument('--debug', action='store_true', help='run debug loop')
    parser.add_argument('--download-memory', metavar='file', help='download whole memory space and write it to specified file')

    if (len(sys.argv) <= 1):
        parser.print_help()
        parser.exit()

    args = parser.parse_args()

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

        sc64.check_api_version()

        if (args.update_bootloader):
            with open(args.update_bootloader, 'rb') as f:
                print('Uploading Bootloader... ', end='', flush=True)
                sc64.upload_bootloader(f.read())
                print('done')

        if (args.reset_state):
            sc64.reset_state()
            print('SC64 internal state reset')

        if (args.print_state):
            state = sc64.get_state()
            print('Current SC64 internal state:')
            for key, value in state.items():
                if (hasattr(value, 'name')):
                    value = getattr(value, 'name')
                print(f'  {key}: {value}')

        if (args.led_enabled != None):
            value = (args.led_enabled == 'true')
            sc64.set_led_enable(value)
            print(f'LED blinking set to [{"ENABLED" if value else "DISABLED"}]')

        if (args.tv != None):
            sc64.set_tv_type(args.tv)
            print(f'TV type set to [{args.tv.name}]')

        if (args.cic != None):
            sc64.set_cic_seed(args.cic)
            print(f'CIC seed set to [0x{args.cic:X}]')

        if (args.rtc):
            value = datetime.now()
            sc64.set_rtc(value)
            print(f'RTC set to [{value.strftime("%Y-%m-%d %H:%M:%S")}]')

        if (args.rom):
            with open(args.rom, 'rb') as f:
                print('Uploading ROM... ', end='', flush=True)
                rom_data = f.read()
                sc64.upload_rom(rom_data, use_shadow=args.no_shadow)
                autodetected_save_type = sc64.autodetect_save_type(rom_data)
                print('done')

        if (args.save_type != None or autodetected_save_type != None):
            save_type = args.save_type if args.save_type != None else autodetected_save_type
            sc64.set_save_type(save_type)
            print(f'Save type set to [{save_type.name}]{" (autodetected)" if autodetected_save_type != None else ""}')

        if (args.save):
            with open(args.save, 'rb') as f:
                print('Uploading save... ', end='', flush=True)
                sc64.upload_save(f.read())
                print('done')

        if (args.ddipl):
            with open(args.ddipl, 'rb') as f:
                print('Uploading 64DD IPL... ', end='', flush=True)
                sc64.upload_ddipl(f.read())
                print('done')

        if (args.boot != None):
            sc64.set_boot_mode(args.boot)
            print(f'Boot mode set to [{args.boot.name}]')

        if (args.cic_params != None):
            (seed, disabled) = args.cic_params
            (seed, checksum, dd_mode) = sc64.set_cic_parameters(seed, disabled)
            print('CIC parameters set to [', end='')
            print(f'{"DISABLED" if disabled else "ENABLED"}, ', end='')
            print(f'{"DDIPL" if dd_mode else "ROM"}, ', end='')
            print(f'seed: 0x{seed:02X}, checksum: 0x{checksum:012X}', end='')
            print(']')
        else:
            sc64.set_cic_parameters()

        if (args.debug or args.isv or args.disk or args.gdb):
            sc64.debug_loop(isv=args.isv, disks=args.disk, gdb_port=args.gdb)

        if (args.backup_save):
            with open(args.backup_save, 'wb') as f:
                print('Downloading save... ', end='', flush=True)
                f.write(sc64.download_save())
                print('done')

        if (args.download_memory):
            with open(args.download_memory, 'wb') as f:
                print('Downloading memory... ', end='', flush=True)
                f.write(sc64.download_memory())
                print('done')
    except ValueError as e:
        print(f'\nValue error: {e}')
    except ConnectionException as e:
        print(f'\nSC64 error: {e}')
