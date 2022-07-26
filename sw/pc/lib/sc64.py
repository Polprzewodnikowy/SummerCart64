from io import BufferedReader
from time import sleep
from sc64_64dd import BadBlockError, SixtyFourDiskDrive
from sc64_transport import SC64Transport

class SC64Exception(Exception):
    pass


class SC64:
    __VERSION_V2 = b'SCv2'

    __CFG_ID_BOOTLOADER_SWITCH = 0
    __CFG_ID_ROM_WRITE_ENABLE = 1
    __CFG_ID_ROM_SHADOW_ENABLE = 2
    __CFG_ID_DD_MODE = 3
    __CFG_ID_ISV_ENABLE = 4
    __CFG_ID_BOOT_MODE = 5
    __CFG_ID_SAVE_TYPE = 6
    __CFG_ID_CIC_SEED = 7
    __CFG_ID_TV_TYPE = 8
    __CFG_ID_FLASH_ERASE_BLOCK = 9
    __CFG_ID_DD_DRIVE_TYPE = 10
    __CFG_ID_DD_DISK_STATE = 11


    __SDRAM_ADDRESS = 0x00000000
    __SDRAM_LENGTH = (64 * 1024 * 1024)
    __DDIPL_ADDRESS = 0x03BC0000
    __DDIPL_LENGTH = (4 * 1024 * 1024)
    __SAVE_ADDRESS = 0x03FE0000
    __SAVE_LENGTH = (128 * 1024)
    __FLASH_ADDRESS = 0x04000000
    __FLASH_LENGTH = (16 * 1024 * 1024)
    __EXTENDED_ROM_ADDRESS = 0x04000000
    __EXTENDED_ROM_LENGTH = (14 * 1024 * 1024)
    __BOOTLOADER_ADDRESS = 0x04E00000
    __BOOTLOADER_LENGTH = (1920 * 1024)
    __SHADOW_ADDRESS = 0x04FE0000
    __SHADOW_LENGTH = (128 * 1024)
    __BUFFER_ADDRESS = 0x06000000
    __BUFFER_LENGTH = (8 * 1024)
    __EEPROM_ADDRESS = 0x06002000
    __EEPROM_LENGTH = (2 * 1024)
    __DD_SECTOR_ADDRESS = 0x06006000
    __DD_SECTOR_LENGTH = (2 * 1024)

    __FLASH_ERASE_SIZE = (128 * 1024)

    __CHUNK_SIZE = (128 * 1024)

    __MAX_ROM_SIZE = __SDRAM_LENGTH + __EXTENDED_ROM_LENGTH

    __BOOT_MODE = [
        'sd',
        'usb',
        'rom',
        'ddipl',
        'direct',
    ]
    __SAVE_TYPE = [
        'none',
        'eeprom4k',
        'eeprom16k',
        'sram',
        'flashram',
        'sram3x',
    ]
    __SAVE_LENGTH = [
        0,
        512,
        (2 * 1024),
        (32 * 1024),
        (128 * 1024),
        (3 * 32 * 1024),
    ]
    __SAVE_ADDRESS = [
        0,
        __EEPROM_ADDRESS,
        __EEPROM_ADDRESS,
        __SAVE_ADDRESS,
        __SAVE_ADDRESS,
        __SAVE_ADDRESS,
    ]
    __DD_MODE = [
        'none',
        'full',
        'ddipl',
    ]
    __DD_DRIVE_TYPE = [
        'retail',
        'development',
    ]
    __DD_DISK_STATE = [
        'ejected',
        'inserted',
        'changed',
    ]

    def __init__(self) -> None:
        self.__transport = SC64Transport()
        self.__transport.connect()
        version = self.__transport.execute_cmd(cmd=b'v', args=[0, 0])
        if (version != self.__VERSION_V2):
            raise SC64Exception("SC64 version different than expected")

    def __get_int(self, data: bytes) -> int:
        return int.from_bytes(data[:4], byteorder="big")

    def __set_config(self, config: int, value: int) -> None:
        self.__transport.execute_cmd(b'C', [config, value])

    def __get_config(self, config: int) -> int:
        return int.from_bytes(self.__transport.execute_cmd(b'c', args=[config, 0]), byteorder="big")

    def __write_memory(self, address: int, data: bytes) -> None:
        self.__transport.execute_cmd(
            cmd=b'M', args=[address, len(data)], data=data)

    def __read_memory(self, address: int, length: int):
        return self.__transport.execute_cmd(cmd=b'm', args=[address, length])

    def __erase_flash_block(self, address: int):
        self.__set_config(self.__CFG_ID_FLASH_ERASE_BLOCK, address)

    def __erase_flash(self, address: int, length: int):
        if (address < self.__FLASH_ADDRESS):
            raise ValueError
        if (address + length >= self.__FLASH_ADDRESS + self.__FLASH_LENGTH):
            raise ValueError
        if (address % self.__FLASH_ERASE_SIZE != 0):
            raise ValueError
        if (length % self.__FLASH_ERASE_SIZE != 0):
            raise ValueError
        for offset in range(address, address + length, self.__FLASH_ERASE_SIZE):
            self.__erase_flash_block(offset)

    def set_boot_mode(self, boot_mode: str) -> None:
        value = self.__BOOT_MODE.index(boot_mode)
        self.__set_config(self.__CFG_ID_BOOT_MODE, value)

    def set_save_type(self, save_type: str) -> None:
        value = self.__SAVE_TYPE.index(save_type)
        self.__set_config(self.__CFG_ID_SAVE_TYPE, value)

    def set_dd_mode(self, dd_mode: str) -> None:
        value = self.__DD_MODE.index(dd_mode)
        self.__set_config(self.__CFG_ID_DD_MODE, value)

    def set_dd_drive_type(self, dd_drive_type: str) -> None:
        value = self.__DD_DRIVE_TYPE.index(dd_drive_type)
        self.__set_config(self.__CFG_ID_DD_DRIVE_TYPE, value)

    def set_dd_disk_state(self, dd_disk_state: str) -> None:
        value = self.__DD_DISK_STATE.index(dd_disk_state)
        self.__set_config(self.__CFG_ID_DD_DISK_STATE, value)

    def upload_rom(self, data: bytes):
        if (len(data) > self.__SDRAM_LENGTH):
            raise ValueError
        self.__write_memory(self.__SDRAM_ADDRESS, data)

    def upload_ddipl(self, data: bytes) -> None:
        if (len(data) > self.__DDIPL_LENGTH):
            raise ValueError
        self.__write_memory(self.__DDIPL_ADDRESS, data)

    def upload_save(self, data: bytes) -> None:
        save_type = self.__get_config(self.__CFG_ID_SAVE_TYPE)
        if (save_type < 0 or save_type >= len(self.__SAVE_TYPE)):
            raise ValueError
        if (len(data) != self.__SAVE_LENGTH[save_type]):
            raise ValueError
        self.__write_memory(self.__SAVE_ADDRESS[save_type], data)

    def upload_bootloader(self, data: bytes) -> None:
        if (len(data) > self.__BOOTLOADER_LENGTH):
            raise ValueError
        self.__erase_flash(self.__BOOTLOADER_ADDRESS, self.__BOOTLOADER_LENGTH)
        self.__write_memory(self.__BOOTLOADER_ADDRESS, data)

    def __handle_dd_packet(self, dd: SixtyFourDiskDrive, data: bytes) -> None:
        CMD_READ_BLOCK = 1
        CMD_WRITE_BLOCK = 2
        cmd = self.__get_int(data[0:])
        address = self.__get_int(data[4:])
        track_head_block = self.__get_int(data[8:])
        track = (track_head_block >> 2) & 0xFFF
        head = (track_head_block >> 1) & 0x1
        block = track_head_block & 0x1
        try:
            if (cmd == CMD_READ_BLOCK):
                block_data = dd.read_block(track, head, block)
                self.__transport.execute_cmd(cmd=b'D', args=[address, len(block_data)], data=block_data)
            elif (cmd == CMD_WRITE_BLOCK):
                dd.write_block(track, head, block, data[12:])
                self.__transport.execute_cmd(cmd=b'd', args=[0, 0])
            else:
                self.__transport.execute_cmd(cmd=b'd', args=[-1, 0])
        except BadBlockError:
            self.__transport.execute_cmd(cmd=b'd', args=[1, 0])

    def __handle_isv_packet(self, data: bytes) -> None:
        print(data.decode("EUC-JP", errors="backslashreplace"), end="")

    def debug_server(self, disk_path: str) -> None:
        dd = SixtyFourDiskDrive()
        dd.load(disk_path)
        self.set_dd_drive_type(dd.get_drive_type())
        self.set_dd_disk_state('inserted')

        self.__set_config(self.__CFG_ID_ROM_WRITE_ENABLE, 1)
        self.__set_config(self.__CFG_ID_ISV_ENABLE, 1)
        self.__set_config(self.__CFG_ID_TV_TYPE, 1)

        while (True):
            packet = self.__transport.get_packet()
            if (packet != None):
                (cmd, data) = packet
                if (cmd == b'D'):
                    self.__handle_dd_packet(dd, data)
                if (cmd == b'I'):
                    self.__handle_isv_packet(data)


if __name__ == "__main__":
    sc64 = SC64()

    sc64.set_boot_mode('rom')
    print('set boot mode')

    sc64.set_dd_mode('none')
    print('set dd mode')

    with open('S:/n64/roms/ZELOOTD.z64', 'rb') as f:
        sc64.upload_rom(f.read())
    print('uploaded rom')

    # with open('S:/n64/64dd/ipl/NDDJ2.n64', 'rb') as f:
    #     sc64.upload_ddipl(f.read())
    # print('uploaded ddipl')

    sc64.set_save_type('sram')
    print('set save type')

    # with open('S:/n64/saves/SM64.eep', 'rb') as f:
    #     sc64.upload_save(f.read())
    # print('uploaded save')

    try:
        print('starting debug server')
        sc64.debug_server(disk_path='S:/n64/64dd/rtl/Mario Artist Polygon Studio.ndd')
    except KeyboardInterrupt:
        pass
