import argparse
import os
import queue
import serial
import sys
import time
from datetime import datetime
from dd64 import BadBlockError, DD64Image
from enum import Enum, IntEnum
from serial.tools import list_ports
from threading import Thread
from typing import Optional



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

    def __init__(self) -> None:
        ports = list_ports.comports()
        device_found = False

        if (self.__serial != None and self.__serial.is_open):
            raise ConnectionException('Serial port is already open')

        for p in ports:
            if (p.vid == self.__VID and p.pid == self.__PID and p.serial_number.startswith('SC64')):
                try:
                    self.__serial = serial.Serial(p.device, timeout=0.1, write_timeout=5.0)
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
        if (self.__thread_read.is_alive()):
            self.__thread_read.join(1)
        if (self.__thread_write.is_alive()):
            self.__thread_write.join(6)
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
            self.__serial.write(data)
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

    def __pop_response(self, cmd: bytes, timeout: float) -> bytes:
        try:
            (response_cmd, data, success) = self.__queue_input.get(timeout=timeout)
            self.__queue_input.task_done()
            if (cmd != response_cmd):
                raise ConnectionException('CMD wrong command response')
            if (success == False):
                raise ConnectionException('CMD response error')
            return data
        except queue.Empty:
            raise ConnectionException('CMD response timeout')

    def execute_cmd(self, cmd: bytes, args: list[int]=[0, 0], data: bytes=b'', response: bool=True, timeout: float=5.0) -> Optional[bytes]:
        self.__check_threads()
        self.__queue_cmd(cmd, args, data)
        if (response):
            return self.__pop_response(cmd, timeout)
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
        SDRAM = 0x0000_0000
        FLASH = 0x0400_0000
        BUFFER = 0x0500_0000
        EEPROM = 0x0500_2000
        FIRMWARE = 0x0200_0000
        DDIPL = 0x03BC_0000
        SAVE = 0x03FE_0000
        SHADOW = 0x04FE_0000

    class __Length(IntEnum):
        SDRAM = (64 * 1024 * 1024)
        FLASH = (16 * 1024 * 1024)
        BUFFER = (8 * 1024)
        EEPROM = (2 * 1024)
        DDIPL = (4 * 1024 * 1024)
        SAVE = (128 * 1024)
        SHADOW = (128 * 1024)

    class __SaveLength(IntEnum):
        NONE = 0
        EEPROM_4K = 512
        EEPROM_16K = (2 * 1024)
        SRAM = (32 * 1024)
        FLASHRAM = (128 * 1024)
        SRAM_3X = (3 * 32 * 1024)

    class __CfgId(IntEnum):
        BOOTLOADER_SWITCH = 0
        ROM_WRITE_ENABLE = 1
        ROM_SHADOW_ENABLE = 2
        DD_MODE = 3
        ISV_ENABLE = 4
        BOOT_MODE = 5
        SAVE_TYPE = 6
        CIC_SEED = 7
        TV_TYPE = 8
        FLASH_ERASE_BLOCK = 9
        DD_DRIVE_TYPE = 10
        DD_DISK_STATE = 11
        BUTTON_STATE = 12
        BUTTON_MODE = 13

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
        SD = 0
        USB = 1
        ROM = 2
        DDIPL = 3
        DIRECT = 4

    class SaveType(IntEnum):
        NONE = 0
        EEPROM_4K = 1
        EEPROM_16K = 2
        SRAM = 3
        FLASHRAM = 4
        SRAM_X3 = 5

    class CICSeed(IntEnum):
        ALECK = 0xAC
        X101 = 0x3F
        X102 = 0x3F
        X103 = 0x78
        X105 = 0x91
        X106 = 0x85
        DD_JP = 0xDD
        DD_US = 0xDE
        AUTO = 0xFFFF

    class TVType(IntEnum):
        PAL = 0
        NTSC = 1
        MPAL = 2
        AUTO = 3

    def __init__(self) -> None:
        self.__link = SC64Serial()
        version = self.__link.execute_cmd(cmd=b'v')
        if (version != b'SCv2'):
            raise ConnectionException('Unknown SC64 API version')

    def __get_int(self, data: bytes) -> int:
        return int.from_bytes(data[:4], byteorder='big')

    def __set_config(self, config: __CfgId, value: int) -> None:
        self.__link.execute_cmd(cmd=b'C', args=[config, value])

    def __get_config(self, config: __CfgId) -> int:
        data = self.__link.execute_cmd(cmd=b'c', args=[config, 0])
        return self.__get_int(data)

    def __write_memory(self, address: int, data: bytes) -> None:
        if (len(data) > 0):
            self.__link.execute_cmd(cmd=b'M', args=[address, len(data)], data=data, timeout=10.0)

    def __read_memory(self, address: int, length: int) -> bytes:
        if (length > 0):
            return self.__link.execute_cmd(cmd=b'm', args=[address, length], timeout=10.0)
        return bytes([])

    def __erase_flash_region(self, address: int, length: int) -> None:
        if (address < self.__Address.FLASH):
            raise ValueError('Flash erase address or length outside of possible range')
        if ((address + length) > (self.__Address.FLASH + self.__Length.FLASH)):
            raise ValueError('Flash erase address or length outside of possible range')
        erase_block_size = self.__get_config(self.__CfgId.FLASH_ERASE_BLOCK)
        if ((address % erase_block_size != 0) or (length % erase_block_size != 0)):
            raise ValueError('Flash erase address or length not aligned to block size')
        for offset in range(address, address + length, erase_block_size):
            self.__set_config(self.__CfgId.FLASH_ERASE_BLOCK, offset)

    def reset_state(self) -> None:
        self.__set_config(self.__CfgId.ROM_WRITE_ENABLE, False)
        self.__set_config(self.__CfgId.ROM_SHADOW_ENABLE, False)
        self.__set_config(self.__CfgId.DD_MODE, self.__DDMode.NONE)
        self.__set_config(self.__CfgId.ISV_ENABLE, False)
        self.__set_config(self.__CfgId.BOOT_MODE, self.BootMode.USB)
        self.__set_config(self.__CfgId.SAVE_TYPE, self.SaveType.NONE)
        self.__set_config(self.__CfgId.CIC_SEED, self.CICSeed.AUTO)
        self.__set_config(self.__CfgId.TV_TYPE, self.TVType.AUTO)
        self.__set_config(self.__CfgId.DD_DRIVE_TYPE, self.__DDDriveType.RETAIL)
        self.__set_config(self.__CfgId.DD_DISK_STATE, self.__DDDiskState.EJECTED)
        self.__set_config(self.__CfgId.BUTTON_MODE, self.__ButtonMode.NONE)
        self.set_cic_parameters()

    def upload_rom(self, data: bytes, use_shadow: bool=True):
        rom_length = len(data)
        if (rom_length > self.__Length.SDRAM):
            raise ValueError('ROM size too big')
        sdram_length = rom_length
        shadow_enabled = use_shadow and rom_length > (self.__Length.SDRAM - self.__Length.SHADOW)
        if (shadow_enabled):
            sdram_length = (self.__Length.SDRAM - self.__Length.SHADOW)
            shadow_length = rom_length - sdram_length
            if (self.__read_memory(self.__Address.SHADOW, shadow_length) != data[sdram_length:]):
                self.__erase_flash_region(self.__Address.SHADOW, self.__Length.SHADOW)
                self.__write_memory(self.__Address.SHADOW, data[sdram_length:])
                if (self.__read_memory(self.__Address.SHADOW, shadow_length) != data[sdram_length:]):
                    raise ConnectionException('Shadow ROM program failure')
        self.__write_memory(self.__Address.SDRAM, data[:sdram_length])
        self.__set_config(self.__CfgId.ROM_SHADOW_ENABLE, shadow_enabled)

    def upload_ddipl(self, data: bytes) -> None:
        if (len(data) > self.__Length.DDIPL):
            raise ValueError('DDIPL size too big')
        self.__write_memory(self.__Address.DDIPL, data)

    def upload_save(self, data: bytes) -> None:
        save_type = self.SaveType(self.__get_config(self.__CfgId.SAVE_TYPE))
        if (save_type not in self.SaveType):
            raise ConnectionError('Unknown save type fetched from SC64 device')
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
        if (save_type not in self.SaveType):
            raise ConnectionError('Unknown save type fetched from SC64 device')
        if (save_type == self.SaveType.NONE):
            raise ValueError('No save type set inside SC64 device')
        address = self.__Address.SAVE
        length = self.__SaveLength[save_type.name]
        if (save_type == self.SaveType.EEPROM_4K or save_type == self.SaveType.EEPROM_16K):
            address = self.__Address.EEPROM
        return self.__read_memory(address, length)

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
        if (mode not in self.BootMode):
            raise ValueError('Boot mode outside of allowed values')
        self.__set_config(self.__CfgId.BOOT_MODE, mode)

    def set_cic_seed(self, seed: int) -> None:
        if (seed != self.CICSeed.AUTO):
            if (seed < 0 or seed > 0xFF):
                raise ValueError('CIC seed outside of allowed values')
        self.__set_config(self.__CfgId.CIC_SEED, seed)

    def set_tv_type(self, type: TVType) -> None:
        if (type not in self.TVType):
            raise ValueError('TV type outside of allowed values')
        self.__set_config(self.__CfgId.TV_TYPE, type)

    def set_save_type(self, type: SaveType) -> None:
        if (type not in self.SaveType):
            raise ValueError('Save type outside of allowed values')
        self.__set_config(self.__CfgId.SAVE_TYPE, type)

    def set_cic_parameters(self, dd_mode: bool=False, seed: int=0x3F, checksum: bytes=bytes([0xA5, 0x36, 0xC0, 0xF1, 0xD8, 0x59])) -> None:
        if (seed < 0 or seed > 0xFF):
            raise ValueError('CIC seed outside of allowed values')
        if (len(checksum) != 6):
            raise ValueError('CIC checksum length outside of allowed values')
        data = bytes([1 if dd_mode else 0, seed])
        data = [*data, *checksum]
        self.__link.execute_cmd(cmd=b'B', args=[self.__get_int(data[0:4]), self.__get_int(data[4:8])])

    def update_firmware(self, data: bytes) -> None:
        address = self.__Address.FIRMWARE
        self.__write_memory(address, data)
        response = self.__link.execute_cmd(cmd=b'F', args=[address, len(data)])
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
            print(f'Update status [{status.name}]')
            if (status == self.__UpdateStatus.ERROR):
                raise ConnectionException('Update error, device is most likely bricked')
        time.sleep(2)

    def backup_firmware(self) -> bytes:
        address = self.__Address.FIRMWARE
        info = self.__link.execute_cmd(cmd=b'f', args=[address, 0], timeout=60.0)
        error = self.__UpdateError(self.__get_int(info[0:4]))
        length = self.__get_int(info[4:8])
        print(f'Backup info - error: {error.name}, length: {hex(length)}')
        if (error != self.__UpdateError.OK):
            raise ConnectionException('Error while getting firmware backup')
        return self.__read_memory(address, length)

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
                self.__link.execute_cmd(cmd=b'D', args=[address, len(block_data)], data=block_data)
            elif (cmd == CMD_WRITE_BLOCK):
                dd.write_block(track, head, block, data[12:])
                self.__link.execute_cmd(cmd=b'd', args=[0, 0])
            else:
                self.__link.execute_cmd(cmd=b'd', args=[-1, 0])
        except BadBlockError:
            self.__link.execute_cmd(cmd=b'd', args=[1, 0])

    def __handle_isv_packet(self, data: bytes) -> None:
        print(data.decode('EUC-JP', errors='backslashreplace'), end='')

    def __handle_usb_packet(self, data: bytes) -> None:
        print(data)

    def debug_loop(self, isv: bool=False, disks: Optional[list[str]]=None) -> None:
        dd = None
        current_image = 0
        next_image = 0

        self.__set_config(self.__CfgId.ROM_WRITE_ENABLE, isv)
        self.__set_config(self.__CfgId.ISV_ENABLE, isv)
        if (isv):
            print('IS-Viewer64 support set to [ENABLED]')
            if (self.__get_config(self.__CfgId.ROM_SHADOW_ENABLE)):
                print('ROM shadow enabled - ISV support will NOT work (use --no-shadow option to disable it)')

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

        print('Debug loop started, use Ctrl-C to exit')

        try:
            while (True):
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
            if (dd and dd.loaded):
                self.__set_config(self.__CfgId.DD_DISK_STATE, self.__DDDiskState.EJECTED)


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
    parser = argparse.ArgumentParser(description='SC64 control software')
    parser.add_argument('--backup', help='backup SC64 firmware and write it to specified file')
    parser.add_argument('--update', help='update SC64 firmware from specified file')
    parser.add_argument('--reset-state', action='store_true', help='reset SC64 internal state')
    parser.add_argument('--boot', type=SC64.BootMode, action=EnumAction, help='set boot mode')
    parser.add_argument('--tv', type=SC64.TVType, action=EnumAction, help='force TV type to set value')
    parser.add_argument('--cic', type=SC64.CICSeed, action=EnumAction, help='force CIC seed to set value')
    parser.add_argument('--rtc', action='store_true', help='update clock in SC64 to system time')
    parser.add_argument('--rom', help='upload ROM from specified file')
    parser.add_argument('--no-shadow', action='store_false', help='do not put last 128 kB of ROM inside flash memory (can corrupt non EEPROM saves)')
    parser.add_argument('--save-type', type=SC64.SaveType, action=EnumAction, help='set save type')
    parser.add_argument('--save', help='upload save from specified file')
    parser.add_argument('--backup-save', help='download save and write it to specified file')
    parser.add_argument('--ddipl', help='upload 64DD IPL from specified file')
    parser.add_argument('--disk', action='append', help='path to 64DD disk (.ndd format), can be specified multiple times')
    parser.add_argument('--isv', action='store_true', help='enable IS-Viewer64 support')
    parser.add_argument('--debug', action='store_true', help='run debug loop (required for IS-Viewer64 and 64DD)')

    if (len(sys.argv) <= 1):
        parser.print_help()
        parser.exit()

    args = parser.parse_args()

    try:
        sc64 = SC64()

        if (args.backup):
            with open(args.backup, 'wb+') as f:
                print('Generating backup, this might take a while... ', end='', flush=True)
                f.write(sc64.backup_firmware())
                print('done')

        if (args.update):
            with open(args.update, 'rb+') as f:
                print('Updating firmware, this might take a while... ', end='', flush=True)
                sc64.update_firmware(f.read())
                print('done')

        if (args.reset_state):
            sc64.reset_state()

        if (args.boot != None):
            sc64.set_boot_mode(args.boot)
            print(f'Boot mode set to [{args.boot.name}]')

        if (args.tv != None):
            sc64.set_tv_type(args.tv)
            print(f'TV type set to [{args.tv.name}]')

        if (args.cic != None):
            sc64.set_cic_seed(args.cic)
            print(f'CIC seed set to [0x{args.cic:X}]')

        if (args.rtc):
            sc64.set_rtc(datetime.now())

        if (args.rom):
            with open(args.rom, 'rb+') as f:
                print('Uploading ROM... ', end='', flush=True)
                sc64.upload_rom(f.read(), use_shadow=args.no_shadow)
                print('done')

        if (args.save_type != None):
            sc64.set_save_type(args.save_type)
            print(f'Save type set to [{args.save_type.name}]')
        
        if (args.save):
            with open(args.save, 'rb+') as f:
                print('Uploading save... ', end='', flush=True)
                sc64.upload_save(f.read())
                print('done')

        if (args.ddipl):
            with open(args.ddipl, 'rb+') as f:
                print('Uploading 64DD IPL... ', end='', flush=True)
                sc64.upload_ddipl(f.read())
                print('done')

        if (args.debug):
            sc64.debug_loop(isv=args.isv, disks=args.disk)

        if (args.backup_save):
            with open(args.backup_save, 'wb+') as f:
                print('Downloading save... ', end='', flush=True)
                f.write(sc64.download_save())
                print('done')
    except ValueError as e:
        print(f'\nValue error: {e}')
    except ConnectionException as e:
        print(f'\nSC64 error: {e}')
