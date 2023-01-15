#!/usr/bin/env python3

import argparse
import math
import os
import platform
import sys
from binascii import crc32
from datetime import datetime
from io import BufferedRandom



class JedecError(Exception):
    pass


class JedecFile:
    __fuse_length: int = 0
    __fuse_offset: int = 0
    __fuse_data: bytes = b''
    __byte_buffer: int = 0

    def __handle_q_field(self, f: BufferedRandom) -> None:
        type = f.read(1)
        if (type == b'F'):
            value = b''
            while (True):
                data = f.read(1)
                if (data == b'*'):
                    value = value.decode('ascii', errors='backslashreplace')
                    if (not value.isdecimal()):
                        raise JedecError('Invalid Q field data')
                    self.__fuse_length = int(value)
                    break
                else:
                    value += data
        else:
            self.__ignore_field(f)

    def __handle_l_field(self, f: BufferedRandom) -> None:
        if (self.__fuse_length <= 0):
            raise JedecError('Found fuse data before declaring fuse count')

        offset = b''
        while (True):
            data = f.read(1)
            if (data >= b'0' and data <= b'9'):
                offset += data
            elif (data == b'\r' or data == b'\n'):
                offset = offset.decode('ascii', errors='backslashreplace')
                if (not offset.isdecimal()):
                    raise JedecError('Invalid L field offset data')
                offset = int(offset)
                if (offset != self.__fuse_offset):
                    raise JedecError('Fuse data is not continuous')
                break
            else:
                raise JedecError('Unexpected byte inside L field offset data')

        data = b''
        while (True):
            data = f.read(1)
            if (data == b'0' or data == b'1'):
                shift = (7 - (self.__fuse_offset % 8))
                self.__byte_buffer |= (1 if data == b'1' else 0) << shift
                if (((self.__fuse_offset % 8) == 7) or (self.__fuse_offset == (self.__fuse_length - 1))):
                    self.__fuse_data += int.to_bytes(self.__byte_buffer, 1, byteorder='little')
                    self.__byte_buffer = 0
                self.__fuse_offset += 1
            elif (data == b'\r' or data == b'\n'):
                pass
            elif (data == b'*'):
                break
            elif (data == b''):
                raise JedecError('Unexpected end of file')
            else:
                raise JedecError('Unexpected byte inside L field fuse data')

    def __ignore_field(self, f: BufferedRandom) -> None:
        data = None
        while (data != b'*'):
            data = f.read(1)
            if (data == b''):
                raise JedecError('Unexpected end of file')

    def parse(self, path: str) -> bytes:
        self.__fuse_length = 0
        self.__fuse_offset = 0
        self.__fuse_data = b''
        self.__byte_buffer = 0

        field = None
        with open(path, 'rb+') as f:
            while (True):
                field = f.read(1)
                if (field == b'\x02'):
                    f.seek(-1, os.SEEK_CUR)
                    break
                elif (field == b''):
                    raise JedecError('Unexpected end of file')

            while (True):
                field = f.read(1)
                if (field == b'Q'):
                    self.__handle_q_field(f)
                elif (field == b'L'):
                    self.__handle_l_field(f)
                elif (field == b'\r' or field == b'\n'):
                    pass
                elif (field == b'\x03'):
                    break
                elif (field == b''):
                    raise JedecError('Unexpected end of file')
                else:
                    self.__ignore_field(f)

        if (self.__fuse_length <= 0):
            raise JedecError('No fuse data found')

        if (self.__fuse_offset != self.__fuse_length):
            raise JedecError('Missing fuse data inside JEDEC file')

        if (len(self.__fuse_data) != math.ceil(self.__fuse_length / 8)):
            raise JedecError('Missing fuse data inside JEDEC file')

        return self.__fuse_data


class SC64UpdateData:
    __UPDATE_TOKEN = b'SC64 Update v2.0'

    __CHUNK_ID_UPDATE_INFO = 1
    __CHUNK_ID_MCU_DATA = 2
    __CHUNK_ID_FPGA_DATA = 3
    __CHUNK_ID_BOOTLOADER_DATA = 4
    __CHUNK_ID_PRIMER_DATA = 5

    __data = b''

    def __int_to_bytes(self, value: int) -> bytes:
        return value.to_bytes(4, byteorder='little')
    
    def __align(self, value: int) -> int:
        if (value % 16 != 0):
            value += (16 - (value % 16))
        return value

    def __add_chunk(self, id: int, data: bytes) -> None:
        chunk = b''
        chunk_length = (16 + len(data))
        aligned_length = self.__align(chunk_length)
        chunk += self.__int_to_bytes(id)
        chunk += self.__int_to_bytes(aligned_length - 8)
        chunk += self.__int_to_bytes(crc32(data))
        chunk += self.__int_to_bytes(len(data))
        chunk += data
        chunk += bytes([0] * (aligned_length - chunk_length))
        self.__data += chunk

    def create_update_data(self) -> None:
        self.__data = self.__UPDATE_TOKEN

    def add_update_info(self, data: bytes) -> None:
        self.__add_chunk(self.__CHUNK_ID_UPDATE_INFO, data)

    def add_mcu_data(self, data: bytes) -> None:
        self.__add_chunk(self.__CHUNK_ID_MCU_DATA, data)

    def add_fpga_data(self, data: bytes) -> None:
        self.__add_chunk(self.__CHUNK_ID_FPGA_DATA, data)

    def add_bootloader_data(self, data: bytes) -> None:
        self.__add_chunk(self.__CHUNK_ID_BOOTLOADER_DATA, data)

    def add_primer_data(self, data: bytes) -> None:
        self.__add_chunk(self.__CHUNK_ID_PRIMER_DATA, data)

    def get_update_data(self) -> bytes:
        return self.__data



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='SC64 update file generator')
    parser.add_argument('--git', metavar='git', required=False, help='git text to embed in update info')
    parser.add_argument('--mcu', metavar='mcu_path', required=False, help='path to MCU update data')
    parser.add_argument('--fpga', metavar='fpga_path', required=False, help='path to FPGA update data')
    parser.add_argument('--boot', metavar='bootloader_path', required=False, help='path to N64 bootloader update data')
    parser.add_argument('--primer', metavar='primer_path', required=False, help='path to MCU board bring-up data')
    parser.add_argument('output', metavar='output_path', help='path to final update data')

    if (len(sys.argv) <= 1):
        parser.print_help()
        parser.exit()

    args = parser.parse_args()

    try:
        update = SC64UpdateData()
        update.create_update_data()

        hostname = platform.node()
        creation_datetime = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
        info = [
            f'build system: [{hostname}]',
            f'creation datetime: [{creation_datetime}]',
        ]
        if (args.git):
            info.append(args.git)
        update_info = ' '.join(info)
        print(update_info)
        update.add_update_info(update_info.encode())

        if (args.mcu):
            with open(args.mcu, 'rb+') as f:
                update.add_mcu_data(f.read())

        if (args.fpga):
            update.add_fpga_data(JedecFile().parse(args.fpga))

        if (args.boot):
            with open(args.boot, 'rb+') as f:
                update.add_bootloader_data(f.read())

        if (args.primer):
            with open(args.primer, 'rb+') as f:
                update.add_primer_data(f.read())

        with open(args.output, 'wb+') as f:
            f.write(update.get_update_data())
    except JedecError as e:
        print(f'Error while parsing FPGA update data: {e}')
        exit(-1)
    except IOError as e:
        print(f'IOError: {e}')
        exit(-1)
