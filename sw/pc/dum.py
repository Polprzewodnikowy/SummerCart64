from serial import Serial, SerialException
from serial.tools import list_ports
import argparse
import os
import sys
import time



class SC64Exception(Exception):
    pass



class SC64:
    __CFG_ID_SCR = 0
    __CFG_ID_SDRAM_SWITCH = 1
    __CFG_ID_SDRAM_WRITABLE = 2
    __CFG_ID_DD_ENABLE = 3
    __CFG_ID_SAVE_TYPE = 4
    __CFG_ID_CIC_SEED = 5
    __CFG_ID_TV_TYPE = 6
    __CFG_ID_SAVE_OFFEST = 7
    __CFG_ID_DD_OFFEST = 8
    __CFG_ID_SKIP_BOOTLOADER = 9
    __CFG_ID_FLASH_OPERATION = 10
    __CFG_ID_RECONFIGURE = 11

    __SC64_VERSION_V2 = 0x53437632

    __UPDATE_OFFSET = 0x03B60000

    __CHUNK_SIZE = 1024 * 1024

    __MIN_ROM_LENGTH = 0x101000
    __DDIPL_ROM_LENGTH = 0x400000

    __serial = None


    def __init__(self) -> None:
        self.__find_sc64()


    def __del__(self) -> None:
        if (self.__serial):
            self.__serial.close()


    def __align(self, value: int, align: int) -> int:
        return (value + (align - 1)) & ~(align - 1)


    def __read(self, bytes: int) -> bytes:
        return self.__serial.read(bytes)


    def __write(self, data: bytes) -> None:
        self.__serial.write(data)

    
    def __write_dummy(self, length: int) -> None:
        self.__write(bytes(length))


    def __read_int(self) -> int:
        return int.from_bytes(self.__read(4), byteorder="big")


    def __write_int(self, value: int) -> None:
        self.__write(value.to_bytes(4, byteorder="big"))


    def __read_cmd_status(self, cmd: str) -> None:
        if (self.__read(4).decode() != f"CMP{cmd[0]}"):
            raise SC64Exception("Wrong command response")


    def __write_cmd(self, cmd: str, arg1: int, arg2: int) -> None:
        self.__write(f"CMD{cmd[0]}".encode())
        self.__write_int(arg1)
        self.__write_int(arg2)


    def __find_sc64(self) -> None:
        ports = list_ports.comports()
        device_found = False

        for p in ports:
            if (p.vid == 0x0403 and p.pid == 0x6014 and p.serial_number.startswith("SC64")):
                try:
                    self.__serial = Serial(p.device, timeout=5.0, write_timeout=5.0)
                    self.__probe_device()
                except SerialException or SC64Exception:
                    if (self.__serial):
                        self.__serial.close()
                    continue
                device_found = True
                break

        if (not device_found):
            raise SC64Exception("No SummerCart64 device was found")


    def __probe_device(self) -> None:
        self.__write_cmd("V", 0, 0)
        version = self.__read_int()
        self.__read_cmd_status("V")
        if (version != self.__SC64_VERSION_V2):
            raise SC64Exception(f"Unknown hardware version: {hex(version)}")


    def __query_config(self, id: int, arg: int = 0) -> int:
        self.__write_cmd("Q", id, arg)
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
            self.__write_cmd("R", offset, transfer_size)
            current_position = 0
            while (current_position < length):
                chunk_size = min(self.__CHUNK_SIZE, length - current_position)
                f.write(self.__read(chunk_size))
                current_position += chunk_size
            if (transfer_size != length):
                self.__read(transfer_size - length)
            self.__read_cmd_status("R")


    def __write_file_to_sdram(self, file: str, offset: int, align: int = 2, min_length: int = 0) -> None:
        with open(file, "rb") as f:
            length = os.fstat(f.fileno()).st_size
            transfer_size = self.__align(max(length, min_length), align)
            self.__write_cmd("W", offset, transfer_size)
            while (f.tell() < length):
                self.__write(f.read(min(self.__CHUNK_SIZE, length - f.tell())))
            if (transfer_size != length):
                self.__write_dummy(transfer_size - length)
            self.__read_cmd_status("W")


    def __get_save_length(self) -> None:
        save_type = self.__query_config(self.__CFG_ID_SAVE_TYPE)
        return {
            0: 0,
            1: 512,
            2: 2048,
            3: (32 * 1024),
            4: (128 * 1024),
            5: (3 * 32 * 1024),
            6: (128 * 1024),
            7: 0
        }[save_type]


    def __reconfigure(self) -> None:
        magic = self.__query_config(self.__CFG_ID_RECONFIGURE)
        self.__change_config(self.__CFG_ID_RECONFIGURE, magic, ignore_response=True)
        time.sleep(0.2)


    def update_firmware(self, file: str, backup_file: str = None) -> None:
        if (backup_file != None):
            backup_length = self.__query_config(self.__CFG_ID_FLASH_OPERATION, self.__UPDATE_OFFSET)
            self.__read_file_from_sdram(backup_file, self.__UPDATE_OFFSET, backup_length)
        self.__write_file_to_sdram(file, self.__UPDATE_OFFSET)
        saved_timeout = self.__serial.timeout
        self.__serial.timeout = 20.0
        self.__change_config(self.__CFG_ID_FLASH_OPERATION, self.__UPDATE_OFFSET)
        self.__serial.timeout = saved_timeout
        self.__reconfigure()
        self.__probe_device()


    def set_skip_bootloader(self, enable: bool) -> None:
        self.__change_config(self.__CFG_ID_SKIP_BOOTLOADER, 1 if enable else 0)


    def download_rom(self, file: str, length: int, offset: int = 0) -> None:
        self.__read_file_from_sdram(file, offset, length)


    def upload_rom(self, file: str, offset: int = 0) -> None:
        self.__write_file_to_sdram(file, offset, min_length=self.__MIN_ROM_LENGTH)


    def set_save_type(self, type: int) -> None:
        if (type >= 0 and type <= 6):
            self.__change_config(self.__CFG_ID_SAVE_TYPE, type)
        else:
            raise SC64Exception("Save type outside of supported values")


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
        dd_ipl_offset = self.__query_config(self.__CFG_ID_DD_OFFEST)
        self.__read_file_from_sdram(file, dd_ipl_offset, length=self.__DDIPL_ROM_LENGTH)


    def upload_dd_ipl(self, file: str, offset: int = None) -> None:
        if (offset != None):
            self.__change_config(self.__CFG_ID_DD_OFFEST, offset)
        dd_ipl_offset = self.__query_config(self.__CFG_ID_DD_OFFEST)
        self.__write_file_to_sdram(file, dd_ipl_offset, min_length=self.__DDIPL_ROM_LENGTH)



if __name__ == "__main__":
    try:
        sc64 = SC64()
        sc64.update_firmware("../../fw/output_files/SC64_update.bin", "SC64_backup.bin")
        sc64.set_skip_bootloader(False)
        sc64.set_save_type(1)
        sc64.set_dd_enable(True)
        sc64.upload_rom("rom.z64")
        sc64.upload_dd_ipl("ddipl.z64")
        sc64.upload_save("saves/save.eep")
        sc64.download_rom("rom.bin", 0x1000)
        sc64.download_save("save.bin")
        sc64.download_dd_ipl("dd_ipl.bin")
    except SC64Exception as e:
        print(e)
