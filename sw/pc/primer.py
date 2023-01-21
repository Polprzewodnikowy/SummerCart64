#!/usr/bin/env python3

import io
import os
import serial
import signal
import sys
import time
from binascii import crc32
from sc64 import SC64
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

    def __init__(self, write: Callable[[bytes], None], read: Callable[[int], bytes], progress: Callable[[int, int, str], None]):
        self.__write = write
        self.__read = read
        self.__progress = progress

    def __append_xor(self, data: bytes) -> bytes:
        xor = (0xFF if (len(data) == 1) else 0x00)
        for b in data:
            xor ^= b
        return bytes([*data, xor])

    def __check_ack(self) -> None:
        response = self.__read(1)
        if (response == None):
            raise STM32BootloaderException('No ACK/NACK byte received')
        if (response == self.__NACK):
            raise STM32BootloaderException('NACK byte received')
        if (response != self.__ACK):
            raise STM32BootloaderException('Unknown ACK/NACK byte received')

    def __cmd_send(self, cmd: bytes) -> None:
        if (len(cmd) != 1):
            raise ValueError('Command must contain only one byte')
        self.__write(self.__append_xor(cmd))
        self.__check_ack()

    def __data_write(self, data: bytes) -> None:
        self.__write(self.__append_xor(data))
        self.__check_ack()

    def __data_read(self) -> bytes:
        length = self.__read(1)
        if (len(length) != 1):
            raise STM32BootloaderException('Did not receive length byte')
        length = length[0]
        data = self.__read(length + 1)
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
        return self.__read(length)

    def __go(self, address: int) -> None:
        self.__cmd_send(b'\x21')
        self.__data_write(address.to_bytes(4, byteorder='big'))
        self.__connected = False

    def __write_memory(self, address: int, data: bytes) -> None:
        length = len(data)
        if (length == 0 or length > self.__MEMORY_RW_MAX_SIZE):
            raise ValueError('Wrong data size for write memory command')
        if (length % 4):
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
            self.__write(self.__INIT)
            self.__check_ack()
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

    def __init__(self, write: Callable[[bytes], None], read: Callable[[int], bytes], progress: Callable[[int, int, str], None]):
        self.__write = write
        self.__read = read
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

        response = self.__read(5)
        if (len(response) != 5):
            raise LCMXO2PrimerException(f'No response received [{cmd}]')
        length = int.from_bytes(response[4:5], byteorder='little')
        response += self.__read(length)
        calculated_checksum = crc32(response)
        received_checksum = int.from_bytes(self.__read(4), byteorder='little')

        if (response[0:3] != b'RSP'):
            raise LCMXO2PrimerException(f'Invalid response token [{response[0:3]} / {cmd}]')
        if (response[3:4] != cmd):
            raise LCMXO2PrimerException(f'Invalid response command [{cmd} / {response[3]}]')
        if (calculated_checksum != received_checksum):
            raise LCMXO2PrimerException(f'Invalid response checksum [{cmd}]')

        return response[5:]

    def connect(self, id: bytes) -> None:
        primer_id = self.__cmd_execute(self.__CMD_GET_PRIMER_ID)
        if (primer_id != self.__PRIMER_ID_LCMXO2):
            raise LCMXO2PrimerException('Invalid primer ID received')

        dev_id = self.__cmd_execute(self.__CMD_GET_DEVICE_ID)
        if (dev_id != id):
            raise LCMXO2PrimerException('Invalid FPGA device id received')

    def load_flash_and_run(self, data: bytes, description: str) -> None:
        erase_description = f'{description} / Erase'
        program_description = f'{description} / Program'
        verify_description = f'{description} / Verify'

        length = len(data)
        if (length > (self.__FLASH_PAGE_SIZE * self.__FLASH_NUM_PAGES)):
            raise LCMXO2PrimerException('FPGA data size too big')

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


class SC64BringUp:
    __SERIAL_BAUD: int      = 115200
    __SERIAL_TIMEOUT: float = 6.0
    __INTERVAL_TIME: float  = 0.5

    def __init__(self, progress: Callable[[int, int, str], None]) -> None:
        self.__progress = progress

    def load_update_data(self, path: str) -> None:
        self.__sc64_update_data = SC64UpdateData()
        self.__sc64_update_data.load(path, require_all=True)

    def get_update_info(self) -> str:
        return self.__sc64_update_data.get_update_info()

    def start_bring_up(self, port: str) -> None:
        link = None
        try:
            link = serial.Serial(
                port,
                baudrate=self.__SERIAL_BAUD,
                parity=serial.PARITY_EVEN,
                timeout=self.__SERIAL_TIMEOUT,
                write_timeout=self.__SERIAL_TIMEOUT
            )

            stm32_bootloader = STM32Bootloader(link.write, link.read, self.__progress)
            lcmxo2_primer = LCMXO2Primer(link.write, link.read, self.__progress)

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

            sc64 = SC64()
            bootloader_description = 'Bootloader -> SC64 FLASH'
            bootloader_data = self.__sc64_update_data.get_bootloader_data()
            bootloader_length = len(bootloader_data)
            self.__progress(bootloader_length, 0, bootloader_description)
            sc64.upload_bootloader(bootloader_data)
            self.__progress(bootloader_length, bootloader_length, bootloader_description)
        finally:
            if (link and link.is_open):
                link.close()


if __name__ == '__main__':
    if (len(sys.argv) != 3):
        Utils.die(f'Usage: {sys.argv[0]} serial_port update_file')

    port = sys.argv[1]
    update_data_path = sys.argv[2]
    utils = Utils()
    sc64_bring_up = SC64BringUp(progress=utils.progress)

    Utils.log()
    Utils.info('[ Welcome to SC64 flashcart board bring-up! ]')
    Utils.log()

    Utils.log(f'Serial port: {port}')
    Utils.log(f'Update data path: {os.path.abspath(update_data_path)}')
    try:
        sc64_bring_up.load_update_data(update_data_path)
    except SC64UpdateDataException as e:
        Utils.die(f'Provided \'{update_data_path}\' file is invalid: {e}')
    Utils.log_no_end('Update info: ')
    Utils.log(sc64_bring_up.get_update_info())
    Utils.log()

    Utils.warning('[  CAUTION  ]')
    Utils.warning('Configure FTDI chip with provided ft232h_config.xml before continuing')
    Utils.warning('Connect SC64 USB port to the same computer you\'re running this script')
    Utils.warning('Make sure SC64 USB port is recognized in system before continuing')
    Utils.log()

    Utils.warning('[ IMPORTANT ]')
    Utils.warning('Unplug board from power and reconnect it before proceeding')
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
        sc64_bring_up.start_bring_up(port)
    except (serial.SerialException, STM32BootloaderException, LCMXO2PrimerException) as e:
        if (utils.get_progress_active):
            Utils.log()
        Utils.die(f'Error while running bring-up: {e}')
    finally:
        signal.signal(signal.SIGINT, original_sigint_handler)

    Utils.log()
    Utils.info('[ SC64 flashcart board bring-up finished successfully! ]')
    Utils.log()
