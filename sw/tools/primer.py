#!/usr/bin/env python3

import io
import os
import queue
import serial
import signal
import sys
import time
from binascii import crc32
from enum import IntEnum
from serial.tools import list_ports
from sys import exit
from typing import Callable, Optional



class Utils:
    __progress_active = False

    @staticmethod
    def log(message: str='') -> None:
        print(message)

    @staticmethod
    def log_no_end(message: str='') -> None:
        print(message, end='', flush=True)

    @staticmethod
    def info(message: str='') -> None:
        print(f'\033[92m{message}\033[0m')

    @staticmethod
    def warning(message: str='') -> None:
        print(f'\033[93m{message}\033[0m')

    @staticmethod
    def die(reason: str) -> None:
        print(f'\033[91m{reason}\033[0m')
        exit(-1)

    @property
    def get_progress_active(self):
        return self.__progress_active

    def progress(self, length: int, position: int, description: str) -> None:
        value = ((position / length) * 100.0)
        if (position == 0):
            self.__progress_active = True
        Utils.log_no_end(f'\r{value:5.1f}%: [{description}]')
        if (position == length):
            Utils.log()
            self.__progress_active = False

    def exit_warning(self):
        if (self.__progress_active):
            Utils.log()
        Utils.warning('Ctrl-C is prohibited during bring-up procedure')


class SC64UpdateDataException(Exception):
    pass

class SC64UpdateData:
    __UPDATE_TOKEN              = b'SC64 Update v2.0'

    __CHUNK_ID_UPDATE_INFO      = 1
    __CHUNK_ID_MCU_DATA         = 2
    __CHUNK_ID_FPGA_DATA        = 3
    __CHUNK_ID_BOOTLOADER_DATA  = 4
    __CHUNK_ID_PRIMER_DATA      = 5

    __update_info: Optional[str]
    __mcu_data: Optional[bytes]
    __fpga_data: Optional[bytes]
    __bootloader_data: Optional[bytes]
    __primer_data: Optional[bytes]

    def __int_to_bytes(self, value: int) -> bytes:
        return value.to_bytes(4, byteorder='little')
    
    def __align(self, value: int) -> int:
        if (value % 16 != 0):
            value += (16 - (value % 16))
        return value

    def __load_int(self, f: io.BufferedReader) -> int:
        try:
            data = f.read(4)
            if (len(data) != 4):
                raise ValueError('Read size did not match requested amount')
            value = int.from_bytes(data, byteorder='little')
        except ValueError as e:
            raise SC64UpdateDataException(f'Error while reading chunk header: {e}')
        return value

    def __load_chunk(self, f: io.BufferedReader) -> tuple[int, bytes]:
        id = self.__load_int(f)
        aligned_length = self.__load_int(f)
        checksum = self.__load_int(f)
        data_length = self.__load_int(f)

        data = f.read(data_length)

        align = (aligned_length - 4 - 4 - data_length)
        f.seek(align, io.SEEK_CUR)

        if (crc32(data) != checksum):
            raise SC64UpdateDataException(f'Invalid checksum for chunk id [{id}] inside update file')

        return (id, data)

    def load(self, path: str, require_all: bool=False) -> None:
        self.__update_info = None
        self.__mcu_data = None
        self.__fpga_data = None
        self.__bootloader_data = None
        self.__primer_data = None

        try:
            with open(path, 'rb') as f:
                if (f.read(len(self.__UPDATE_TOKEN)) != self.__UPDATE_TOKEN):
                    raise SC64UpdateDataException('Invalid update file header')

                while (f.peek(1) != b''):
                    (id, data) = self.__load_chunk(f)
                    if (id == self.__CHUNK_ID_UPDATE_INFO):
                        self.__update_info = data.decode('ascii')
                    elif (id == self.__CHUNK_ID_MCU_DATA):
                        self.__mcu_data = data
                    elif (id == self.__CHUNK_ID_FPGA_DATA):
                        self.__fpga_data = data
                    elif (id == self.__CHUNK_ID_BOOTLOADER_DATA):
                        self.__bootloader_data = data
                    elif (id == self.__CHUNK_ID_PRIMER_DATA):
                        self.__primer_data = data
                    else:
                        raise SC64UpdateDataException('Unknown chunk inside update file')

            if (require_all):
                if (not self.__update_info):
                    raise SC64UpdateDataException('No update info inside update file')
                if (not self.__mcu_data):
                    raise SC64UpdateDataException('No MCU data inside update file')
                if (not self.__fpga_data):
                    raise SC64UpdateDataException('No FPGA data inside update file')
                if (not self.__bootloader_data):
                    raise SC64UpdateDataException('No bootloader data inside update file')
                if (not self.__primer_data):
                    raise SC64UpdateDataException('No primer data inside update file')

        except IOError as e:
            raise SC64UpdateDataException(f'IO error while loading update data: {e}')

    def get_update_info(self) -> Optional[str]:
        return self.__update_info

    def get_mcu_data(self) -> Optional[bytes]:
        return self.__mcu_data

    def get_fpga_data(self) -> Optional[bytes]:
        return self.__fpga_data

    def get_bootloader_data(self) -> Optional[bytes]:
        return self.__bootloader_data

    def get_primer_data(self) -> Optional[bytes]:
        return self.__primer_data
    
    def create_bootloader_only_firmware(self):
        if (self.__bootloader_data == None):
            raise SC64UpdateDataException('No bootloader data available for firmware creation')

        chunk = b''
        chunk += self.__int_to_bytes(self.__CHUNK_ID_BOOTLOADER_DATA)
        chunk += self.__int_to_bytes(8 + self.__align(len(self.__bootloader_data)))
        chunk += self.__int_to_bytes(crc32(self.__bootloader_data))
        chunk += self.__int_to_bytes(len(self.__bootloader_data))
        chunk += self.__bootloader_data
        chunk += bytes([0] * (self.__align(len(chunk)) - len(chunk)))

        data = b''
        data += self.__UPDATE_TOKEN
        data += chunk

        return data


class STM32BootloaderException(Exception):
    pass

class STM32Bootloader:
    __INIT  = b'\x7F'
    __ACK   = b'\x79'
    __NACK  = b'\x1F'

    __MEMORY_RW_MAX_SIZE    = 256
    __FLASH_LOAD_ADDRESS    = 0x08000000
    __FLASH_MAX_LOAD_SIZE   = 0x8000
    __RAM_LOAD_ADDRESS      = 0x20001000
    __RAM_MAX_LOAD_SIZE     = 0x1000

    DEV_ID_STM32G030XX = b'\x04\x66'

    __connected = False

    def __init__(
            self,
            write: Callable[[bytes], None],
            read: Callable[[int], bytes],
            flush: Callable[[None], None],
            progress: Callable[[int, int, str], None]
        ):
        self.__write = write
        self.__read = read
        self.__flush = flush
        self.__progress = progress

    def __append_xor(self, data: bytes) -> bytes:
        xor = (0xFF if (len(data) == 1) else 0x00)
        for b in data:
            xor ^= b
        return bytes([*data, xor])

    def __check_ack(self) -> None:
        response = self.__read(1)
        if (len(response) != 1):
            raise STM32BootloaderException('No ACK/NACK byte received')
        if (response == self.__NACK):
            raise STM32BootloaderException('NACK byte received')
        if (response != self.__ACK):
            raise STM32BootloaderException('Unknown ACK/NACK byte received')

    def __cmd_send(self, cmd: bytes) -> None:
        if (len(cmd) != 1):
            raise ValueError('Command must contain only one byte')
        self.__write(self.__append_xor(cmd))
        self.__flush()
        self.__check_ack()

    def __data_write(self, data: bytes) -> None:
        self.__write(self.__append_xor(data))
        self.__flush()
        self.__check_ack()

    def __data_read(self) -> bytes:
        length = self.__read(1)
        if (len(length) != 1):
            raise STM32BootloaderException('Did not receive length byte')
        length = (length[0] + 1)
        data = self.__read(length)
        if (len(data) != length):
            raise STM32BootloaderException('Did not receive requested data bytes')
        self.__check_ack()
        return data

    def __get_id(self) -> bytes:
        self.__cmd_send(b'\x02')
        return self.__data_read()

    def __read_memory(self, address: int, length: int) -> bytes:
        if (length == 0 or length > self.__MEMORY_RW_MAX_SIZE):
            raise ValueError('Wrong data size for read memory command')
        self.__cmd_send(b'\x11')
        self.__data_write(address.to_bytes(4, byteorder='big'))
        self.__data_write(bytes([length - 1]))
        data = self.__read(length)
        if (len(data) != length):
            raise STM32BootloaderException(f'Did not receive requested memory bytes')
        return data

    def __go(self, address: int) -> None:
        self.__cmd_send(b'\x21')
        self.__data_write(address.to_bytes(4, byteorder='big'))
        self.__connected = False

    def __write_memory(self, address: int, data: bytes) -> None:
        length = len(data)
        if (length == 0 or length > self.__MEMORY_RW_MAX_SIZE):
            raise ValueError('Wrong data size for write memory command')
        if (((address % 4) != 0) or ((length % 4) != 0)):
            raise ValueError('Write memory command requires 4 byte alignment')
        self.__cmd_send(b'\x31')
        self.__data_write(address.to_bytes(4, byteorder='big'))
        self.__data_write(bytes([length - 1, *data]))

    def __mass_erase(self) -> None:
        self.__cmd_send(b'\x44')
        self.__data_write(b'\xFF\xFF')

    def __load_memory(self, address: int, data: bytes, description: str='') -> None:
        length = len(data)
        self.__progress(length, 0, description)
        for offset in range(0, length, self.__MEMORY_RW_MAX_SIZE):
            chunk = data[offset:offset + self.__MEMORY_RW_MAX_SIZE]
            self.__write_memory(address + offset, chunk)
            verify = self.__read_memory(address + offset, len(chunk))
            if (chunk != verify):
                raise STM32BootloaderException('Memory verify failed')
            self.__progress(length, offset, description)
        self.__progress(length, length, description)

    def connect(self, id: int) -> None:
        if (not self.__connected):
            try:
                self.__write(self.__INIT)
                self.__flush()
                self.__check_ack()
            except STM32BootloaderException as e:
                raise STM32BootloaderException(f'Could not connect to the STM32 ({e})')
            self.__connected = True
        dev_id = self.__get_id()
        if (dev_id != id):
            raise STM32BootloaderException('Unknown chip detected')

    def load_ram_and_run(self, data: bytes, description: str='') -> None:
        if (len(data) > self.__RAM_MAX_LOAD_SIZE):
            raise STM32BootloaderException('RAM image too big')
        self.__load_memory(self.__RAM_LOAD_ADDRESS, data, description)
        self.__go(self.__RAM_LOAD_ADDRESS)

    def load_flash_and_run(self, data: bytes, description: str='') -> None:
        if (len(data) > self.__FLASH_MAX_LOAD_SIZE):
            raise STM32BootloaderException('Flash image too big')
        self.__mass_erase()
        try:
            self.__load_memory(self.__FLASH_LOAD_ADDRESS, data, description)
            self.__go(self.__FLASH_LOAD_ADDRESS)
        except STM32BootloaderException as e:
            self.__mass_erase()
            raise STM32BootloaderException(e)


class LCMXO2PrimerException(Exception):
    pass

class LCMXO2Primer:
    __PRIMER_ID_LCMXO2  = b'MXO2'

    __CMD_GET_PRIMER_ID = b'?'
    __CMD_PROBE_FPGA    = b'#'
    __CMD_RESTART       = b'$'
    __CMD_GET_DEVICE_ID = b'I'
    __CMD_ENABLE_FLASH  = b'E'
    __CMD_ERASE_FLASH   = b'X'
    __CMD_RESET_ADDRESS = b'A'
    __CMD_WRITE_PAGE    = b'W'
    __CMD_READ_PAGE     = b'R'
    __CMD_PROGRAM_DONE  = b'F'
    __CMD_INIT_FEATBITS = b'Q'
    __CMD_REFRESH       = b'B'

    __FLASH_PAGE_SIZE   = 16
    __FLASH_NUM_PAGES   = 11260

    __FPGA_PROBE_VALUE  = b'\x64'

    DEV_ID_LCMXO2_7000HC = b'\x01\x2B\xD0\x43'

    def __init__(
            self,
            write: Callable[[bytes], None],
            read: Callable[[int], bytes],
            flush: Callable[[None], None],
            progress: Callable[[int, int, str], None]
        ):
        self.__write = write
        self.__read = read
        self.__flush = flush
        self.__progress = progress

    def __cmd_execute(self, cmd: bytes, data: bytes=b'') -> bytes:
        if (len(cmd) != 1):
            raise ValueError('Command must contain only one byte')
        if (len(data) >= 256):
            raise ValueError('Data size too big')

        packet = b'CMD' + cmd
        packet += len(data).to_bytes(1, byteorder='little')
        packet += data
        packet += crc32(packet).to_bytes(4, byteorder='little')
        self.__write(packet)
        self.__flush()

        response = self.__read(5)
        if (len(response) != 5):
            raise LCMXO2PrimerException(f'No response received [{cmd}]')
        length = int.from_bytes(response[4:5], byteorder='little')
        response_data = self.__read(length)
        if (len(response_data) != length):
            raise LCMXO2PrimerException(f'No response data received [{cmd}]')
        checksum = self.__read(4)
        if (len(checksum) != 4):
            raise LCMXO2PrimerException(f'No response data checksum received [{cmd}]')
        calculated_checksum = crc32(response + response_data)
        received_checksum = int.from_bytes(checksum, byteorder='little')

        if (response[0:3] != b'RSP'):
            raise LCMXO2PrimerException(f'Invalid response token [{response[0:3]} / {cmd}]')
        if (response[3:4] != cmd):
            raise LCMXO2PrimerException(f'Invalid response command [{cmd} / {response[3]}]')
        if (calculated_checksum != received_checksum):
            raise LCMXO2PrimerException(f'Invalid response checksum [{cmd}]')

        return response_data

    def connect(self, id: bytes) -> None:
        try:
            primer_id = self.__cmd_execute(self.__CMD_GET_PRIMER_ID)
            if (primer_id != self.__PRIMER_ID_LCMXO2):
                raise LCMXO2PrimerException('Invalid primer ID received')

            dev_id = self.__cmd_execute(self.__CMD_GET_DEVICE_ID)
            if (dev_id != id):
                raise LCMXO2PrimerException('Invalid FPGA device id received')
        except LCMXO2PrimerException as e:
            raise LCMXO2PrimerException(f'Could not connect to the LCMXO2 primer ({e})')

    def load_flash_and_run(self, data: bytes, description: str) -> None:
        erase_description = f'{description} / Erase'
        program_description = f'{description} / Program'
        verify_description = f'{description} / Verify'

        length = len(data)
        if (length > (self.__FLASH_PAGE_SIZE * self.__FLASH_NUM_PAGES)):
            raise LCMXO2PrimerException('FPGA data size too big')
        if ((length % self.__FLASH_PAGE_SIZE) != 0):
            raise LCMXO2PrimerException('FPGA data size not aligned to page size')

        self.__cmd_execute(self.__CMD_ENABLE_FLASH)

        self.__progress(length, 0, erase_description)
        self.__cmd_execute(self.__CMD_ERASE_FLASH)
        self.__progress(length, length, erase_description)

        try:
            self.__cmd_execute(self.__CMD_RESET_ADDRESS)
            self.__progress(length, 0, program_description)
            for offset in range(0, length, self.__FLASH_PAGE_SIZE):
                page_data = data[offset:(offset + self.__FLASH_PAGE_SIZE)]
                self.__cmd_execute(self.__CMD_WRITE_PAGE, page_data)
                self.__progress(length, offset, program_description)
            self.__progress(length, length, program_description)

            self.__cmd_execute(self.__CMD_RESET_ADDRESS)
            self.__progress(length, 0, verify_description)
            for offset in range(0, length, self.__FLASH_PAGE_SIZE):
                page_data = data[offset:(offset + self.__FLASH_PAGE_SIZE)]
                verify_data = self.__cmd_execute(self.__CMD_READ_PAGE)
                self.__progress(length, offset, verify_description)
                if (page_data != verify_data):
                    raise LCMXO2PrimerException('FPGA verification error')
            self.__progress(length, length, verify_description)

            self.__cmd_execute(self.__CMD_INIT_FEATBITS)

            self.__cmd_execute(self.__CMD_PROGRAM_DONE)

            self.__cmd_execute(self.__CMD_REFRESH)

            if (self.__cmd_execute(self.__CMD_PROBE_FPGA) != self.__FPGA_PROBE_VALUE):
                raise LCMXO2PrimerException('Invalid FPGA ID value received')

        except LCMXO2PrimerException as e:
            self.__cmd_execute(self.__CMD_ENABLE_FLASH)
            self.__cmd_execute(self.__CMD_ERASE_FLASH)
            self.__cmd_execute(self.__CMD_REFRESH)
            self.__cmd_execute(self.__CMD_RESTART)
            raise LCMXO2PrimerException(e)

        self.__cmd_execute(self.__CMD_RESTART)


class SC64Exception(Exception):
    pass

class SC64:
    __serial: Optional[serial.Serial] = None
    __packets = queue.Queue()

    class __UpdateStatus(IntEnum):
        MCU = 1
        FPGA = 2
        BOOTLOADER = 3
        DONE = 0x80
        ERROR = 0xFF

    def __init__(self) -> None:
        SC64_VID = 0x0403
        SC64_PID = 0x6014
        SC64_SID = "SC64"
        for p in list_ports.comports():
            if (p.vid == SC64_VID and p.pid == SC64_PID and p.serial_number.startswith(SC64_SID)):
                try:
                    self.__serial = serial.Serial(p.device, timeout=10.0, write_timeout=10.0)
                except serial.SerialException:
                    if (self.__serial):
                        self.__serial.close()
                    continue
                return
        raise SC64Exception('No SC64 USB device found')

    def __reset(self) -> None:
        WAIT_DURATION = 0.01
        RETRY_COUNT = 100

        self.__serial.dtr = 1
        for n in range(0, RETRY_COUNT + 1):
            self.__serial.reset_input_buffer()
            self.__serial.reset_output_buffer()
            time.sleep(WAIT_DURATION)
            if (self.__serial.dsr == 1):
                break
            if n == RETRY_COUNT:
                raise SC64Exception('Couldn\'t reset SC64 device (on)')

        self.__serial.dtr = 0
        for n in range(0, RETRY_COUNT + 1):
            time.sleep(WAIT_DURATION)
            if (self.__serial.dsr == 0):
                break
            if n == RETRY_COUNT:
                raise SC64Exception('Couldn\'t reset SC64 device (on)')

    def __process_incoming_data(self, wait_for_response: bool) -> Optional[tuple[bytes, bytes]]:
        while (wait_for_response or self.__serial.in_waiting >= 4):
            buffer = self.__serial.read(4)
            token = buffer[0:3]
            id = buffer[3:4]
            if (token == b'CMP'):
                length = int.from_bytes(self.__serial.read(4), byteorder='big')
                data = self.__serial.read(length)
                return (id, data)
            elif (token == b'PKT'):
                length = int.from_bytes(self.__serial.read(4), byteorder='big')
                data = self.__serial.read(length)
                self.__packets.put((id, data))
                if (not wait_for_response):
                    break
            elif (token == b'ERR'):
                raise SC64Exception('Command response error')
            else:
                raise SC64Exception('Invalid token received')
        return None

    def __execute_command(self, cmd: bytes, args: list[int]=[0, 0], data: bytes=b'') -> bytes:
        if (len(cmd) != 1):
            raise SC64Exception('Length of command is different than 1 byte')
        if (len(args) != 2):
            raise SC64Exception('Number of arguments is different than 2')
        try:
            self.__serial.write(b'CMD' + cmd)
            self.__serial.write(args[0].to_bytes(4, byteorder='big'))
            self.__serial.write(args[1].to_bytes(4, byteorder='big'))
            if (len(data) > 0):
                self.__serial.write(data)
            self.__serial.flush()
            (id, response) = self.__process_incoming_data(True)
            if (cmd != id):
                raise SC64Exception('Command response ID didn\'t match')
            return response
        except serial.SerialException as e:
            raise SC64Exception(f'Serial exception: {e}')

    def __receive_data_packet(self) -> Optional[tuple[bytes, bytes]]:
        if (self.__packets.empty()):
            try:
                if (self.__process_incoming_data(False) != None):
                    raise SC64Exception('Unexpected command response')
            except serial.SerialException as e:
                raise SC64Exception(f'Serial exception: {e}')
        if (not self.__packets.empty()):
            packet = self.__packets.get()
            self.__packets.task_done()
            return packet
        return None

    def update_firmware(self, data: bytes) -> None:
        FIRMWARE_ADDRESS = 0x00100000
        FIRMWARE_UPDATE_TIMEOUT = 90.0

        self.__reset()
        self.__execute_command(b'R')
        self.__execute_command(b'M', [FIRMWARE_ADDRESS, len(data)], data)
        self.__execute_command(b'F', [FIRMWARE_ADDRESS, len(data)])

        timeout = time.time() + FIRMWARE_UPDATE_TIMEOUT
        while True:
            if (time.time() > timeout):
                raise SC64Exception('Firmware update timeout')
            packet = self.__receive_data_packet()
            if (packet == None):
                time.sleep(0.001)
                continue
            (id, packet_data) = packet
            if (id != b'F'):
                raise SC64Exception('Unexpected packet id received')
            status = self.__UpdateStatus(int.from_bytes(packet_data[0:4], byteorder='big'))
            if (status == self.__UpdateStatus.ERROR):
                raise SC64Exception('Firmware update error')
            if (status == self.__UpdateStatus.DONE):
                time.sleep(2)
                break


class SC64BringUp:
    __SERIAL_BAUD: int      = 115200
    __SERIAL_TIMEOUT: float = 6.0
    __INTERVAL_TIME: float  = 0.5

    def __init__(self, progress: Callable[[int, int, str], None]) -> None:
        self.__progress = progress

    def load_update_data(self, path: str) -> None:
        self.__sc64_update_data = SC64UpdateData()
        self.__sc64_update_data.load(path, require_all=True)
        self.__bootloader_only_firmware = self.__sc64_update_data.create_bootloader_only_firmware()

    def get_update_info(self) -> str:
        return self.__sc64_update_data.get_update_info()

    def start_bring_up(self, port: str, bootloader_only: bool=False) -> None:
        link = None
        sc64 = SC64()

        try:
            if (not bootloader_only):
                link = serial.Serial(
                    port,
                    baudrate=self.__SERIAL_BAUD,
                    parity=serial.PARITY_EVEN,
                    timeout=self.__SERIAL_TIMEOUT,
                    write_timeout=self.__SERIAL_TIMEOUT
                )

                stm32_bootloader = STM32Bootloader(link.write, link.read, link.flush, self.__progress)
                lcmxo2_primer = LCMXO2Primer(link.write, link.read, link.flush, self.__progress)

                stm32_bootloader.connect(stm32_bootloader.DEV_ID_STM32G030XX)
                stm32_bootloader.load_ram_and_run(self.__sc64_update_data.get_primer_data(), 'FPGA primer -> STM32 RAM')
                time.sleep(self.__INTERVAL_TIME)
                link.read_all()

                lcmxo2_primer.connect(lcmxo2_primer.DEV_ID_LCMXO2_7000HC)
                lcmxo2_primer.load_flash_and_run(self.__sc64_update_data.get_fpga_data(), 'FPGA configuration -> LCMXO2 FLASH')
                time.sleep(self.__INTERVAL_TIME)
                link.read_all()

                stm32_bootloader.connect(stm32_bootloader.DEV_ID_STM32G030XX)
                stm32_bootloader.load_flash_and_run(self.__sc64_update_data.get_mcu_data(), 'MCU software -> STM32 FLASH')
                time.sleep(self.__INTERVAL_TIME)
                link.read_all()

            bootloader_description = 'Bootloader -> SC64 FLASH (no progress reporting)'
            bootloader_length = len(self.__bootloader_only_firmware)
            self.__progress(bootloader_length, 0, bootloader_description)
            sc64.update_firmware(self.__bootloader_only_firmware)
            self.__progress(bootloader_length, bootloader_length, bootloader_description)
        finally:
            if (link and link.is_open):
                link.close()


if __name__ == '__main__':
    nargs = len(sys.argv)
    if (nargs < 3 or nargs > 4):
        Utils.die(f'Usage: {sys.argv[0]} serial_port update_file [--bootloader-only]')

    port = sys.argv[1]
    update_data_path = sys.argv[2]
    bootloader_only = False
    if (nargs == 4):
        if (sys.argv[3] == '--bootloader-only'):
            bootloader_only = True
        else:
            Utils.die(f'Unknown argument: {sys.argv[3]}')

    utils = Utils()
    sc64_bring_up = SC64BringUp(progress=utils.progress)

    Utils.log()
    Utils.info('[ Welcome to SummerCart64 flashcart board bring-up! ]')
    Utils.log()

    Utils.log(f'Serial port: {port}')
    Utils.log(f'Update data path: {os.path.abspath(update_data_path)}')
    try:
        sc64_bring_up.load_update_data(update_data_path)
    except SC64UpdateDataException as e:
        Utils.die(f'Provided \'{update_data_path}\' file is invalid: {e}')
    Utils.log('Update info: ')
    Utils.log(sc64_bring_up.get_update_info())
    Utils.log()

    if bootloader_only:
        Utils.log('Running in "bootloader only" mode')
        Utils.log()

    Utils.warning('[  CAUTION  ]')
    Utils.warning('Configure FTDI chip with provided ft232h_config.xml before continuing')
    Utils.warning('Connect SC64 USB port to the same computer you\'re running this script')
    Utils.warning('Make sure SC64 USB port is recognized in system before continuing')
    Utils.log()

    Utils.warning('[ IMPORTANT ]')
    Utils.warning('Unplug SC64 board from power and reconnect it before proceeding')
    Utils.log()

    try:
        if (input('Type YES to continue: ') != 'YES'):
            Utils.die('No confirmation received. Exiting')
        Utils.log()
    except KeyboardInterrupt:
        Utils.log()
        Utils.die('Aborted')

    original_sigint_handler = signal.getsignal(signal.SIGINT)
    try:
        signal.signal(signal.SIGINT, lambda *kwargs: utils.exit_warning())
        Utils.log('Starting SC64 flashcart board bring-up...')
        sc64_bring_up.start_bring_up(port, bootloader_only)
    except (serial.SerialException, STM32BootloaderException, LCMXO2PrimerException, SC64Exception) as e:
        if (utils.get_progress_active):
            Utils.log()
        Utils.die(f'Error while running bring-up: {e}')
    finally:
        signal.signal(signal.SIGINT, original_sigint_handler)

    Utils.log()
    Utils.info('[ SC64 flashcart board bring-up finished successfully! ]')
    Utils.log()
