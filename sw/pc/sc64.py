#!/usr/bin/env python3

from serial import Serial, SerialException
from serial.tools import list_ports
import argparse
import filecmp
import os
import progressbar
import re
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
    __CFG_ID_BOOT_MODE = 9
    __CFG_ID_FLASH_SIZE = 10
    __CFG_ID_FLASH_READ = 11
    __CFG_ID_FLASH_PROGRAM = 12
    __CFG_ID_RECONFIGURE = 13

    __SC64_VERSION_V2 = 0x53437632

    __UPDATE_OFFSET = 0x03B60000

    __CHUNK_SIZE = 256 * 1024

    __MIN_ROM_LENGTH = 0x101000
    __DDIPL_ROM_LENGTH = 0x400000

    __DEBUG_ID_TEXT = 0x01
    __DEBUG_ID_FSD_READ = 0xF1
    __DEBUG_ID_FSD_WRITE = 0xF2


    def __init__(self) -> None:
        self.__serial = None
        self.__progress_init = None
        self.__progress_value = None
        self.__progress_finish = None
        self.__fsd_file = None
        self.__find_sc64()


    def __del__(self) -> None:
        if (self.__serial):
            self.__serial.close()
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


    def __reset_link(self) -> None:
        self.__serial.write(b"\x1BR")


    def __read(self, bytes: int) -> bytes:
        return self.__serial.read(bytes)


    def __write(self, data: bytes) -> None:
        self.__serial.write(self.__escape(data))


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


    def __find_sc64(self) -> None:
        ports = list_ports.comports()
        device_found = False

        if (self.__serial != None and self.__serial.isOpen()):
            self.__serial.close()

        for p in ports:
            if (p.vid == 0x0403 and p.pid == 0x6014 and p.serial_number.startswith("SC64")):
                try:
                    self.__serial = Serial(p.device, timeout=1.0, write_timeout=1.0)
                    self.__serial.flushOutput()
                    self.__reset_link()
                    while (self.__serial.in_waiting):
                        self.__serial.read_all()
                        time.sleep(0.1)
                    self.__probe_device()
                except (SerialException, SC64Exception):
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


    def __query_config(self, id: int) -> int:
        self.__write_cmd("Q", id, 0)
        value = self.__read_int()
        self.__read_cmd_status("Q")
        return value


    def __change_config(self, id: int, arg: int = 0, ignore_response: bool = False) -> None:
        self.__write_cmd("C", id, arg)
        if (not ignore_response):
            self.__read_cmd_status("C")


    def __debug_write(self, type: int, data: bytes) -> None:
        self.__write_cmd("D", type, len(data))
        self.__write(data)


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
        saved_timeout = self.__serial.timeout
        self.__serial.timeout = 20.0
        self.__change_config(self.__CFG_ID_FLASH_PROGRAM, self.__UPDATE_OFFSET)
        self.__serial.timeout = saved_timeout
        self.__reconfigure()
        self.__find_sc64()


    def set_boot_mode(self, mode: int) -> None:
        if (mode >= 0 and mode <= 3):
            self.__change_config(self.__CFG_ID_BOOT_MODE, mode)
        else:
            raise SC64Exception("Boot mode outside of supported values")


    def get_boot_mode_label(self, mode: int) -> None:
        if (mode < 0 or mode > 3):
            return "Unknown"
        return {
            0: "Load menu from SD card",
            1: "Load ROM from SDRAM through bootloader",
            2: "Load DDIPL from SDRAM",
            3: "Load ROM from SDRAM directly without bootloader"
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
        dd_ipl_offset = self.__query_config(self.__CFG_ID_DD_OFFEST)
        self.__read_file_from_sdram(file, dd_ipl_offset, length=self.__DDIPL_ROM_LENGTH)


    def upload_dd_ipl(self, file: str, offset: int = None) -> None:
        if (offset != None):
            self.__change_config(self.__CFG_ID_DD_OFFEST, offset)
        dd_ipl_offset = self.__query_config(self.__CFG_ID_DD_OFFEST)
        self.__write_file_to_sdram(file, dd_ipl_offset, min_length=self.__DDIPL_ROM_LENGTH)


    def __debug_process_fsd_read(self, file: str, id: int, data: bytes) -> None:
        sector = int.from_bytes(data[0:4], byteorder='big')
        count = int.from_bytes(data[4:8], byteorder='big')
        if (file):
            if (self.__fsd_file == None):
                self.__fsd_file = open(file, "rb")
            self.__fsd_file.seek(sector * 512)
            self.__debug_write(id, self.__fsd_file.read(count * 512))
        else:
            self.__debug_write(id, bytes(count * 512))


    def debug_loop(self, file: str = None) -> None:
        print("\033[34m --- Debug server started --- \033[0m")

        self.__serial.timeout = 0.01
        self.__serial.write_timeout = 1

        while (True):
            start_indicator = self.__read_long(4)
            if (start_indicator == b"DMA@"):
                header = self.__read_long(4)
                id = int(header[0])
                length = int.from_bytes(header[1:4], byteorder="big")

                if (length > 0):
                align = self.__align(length, 4) - length
                data = self.__read_long(length)

                    if (id == self.__DEBUG_ID_TEXT):
                        print(data.decode(encoding="ascii", errors="backslashreplace"), end="")
                    elif (id == self.__DEBUG_ID_FSD_READ):
                        pass
                    elif (id == self.__DEBUG_ID_FSD_WRITE):
                        pass
                    else:
                        print(f"Got unknown id: {id}, length: {length}")

                    self.__read_long(align)
                    end_indicator = self.__read_long(4)
                    if (end_indicator != b"CMPH"):
                        print(f"Got unknown end indicator: {end_indicator.decode(encoding='ascii', errors='backslashreplace')}")

                    if (id == self.__DEBUG_ID_FSD_READ):
                        self.__debug_process_fsd_read(file, id, data)

            else:
                print(f"Got unknown start indicator: {start_indicator.decode(encoding='ascii', errors='backslashreplace')}")



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
    parser.add_argument("-b", metavar="boot_mode", default="1", required=False, help="set boot mode (0 - 3)")
    parser.add_argument("-t", metavar="tv_type", default="3", required=False, help="set TV type (0 - 2)")
    parser.add_argument("-c", metavar="cic_seed", default="0xFFFF", required=False, help="set CIC seed")
    parser.add_argument("-s", metavar="save_type", default="0", required=False, help="set save type (0 - 6)")
    parser.add_argument("-d", default=False, action="store_true", required=False, help="enable 64DD emulation")
    parser.add_argument("-r", default=False, action="store_true", required=False, help="perform reading operation instead of writing")
    parser.add_argument("-l", metavar="length", default="0x101000", required=False, help="specify ROM length to read")
    parser.add_argument("-u", metavar="update_path", default=None, required=False, help="path to update file")
    parser.add_argument("-e", metavar="save_path", default=None, required=False, help="path to save file")
    parser.add_argument("-i", metavar="ddipl_path", default=None, required=False, help="path to DDIPL file")
    parser.add_argument("-q", default=None, action="store_true", required=False, help="start debug server")
    parser.add_argument("-f", metavar="sd_path", default=None, required=False, help="path to disk or file for fake SD card emulation")
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
        is_read = args.r
        rom_length = int(args.l, 0)
        update_file = args.u
        save_file = args.e
        dd_ipl_file = args.i
        rom_file = args.rom
        debug_server = args.q
        sd_file = args.f

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
                if (boot_mode != 1):
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

            if (debug_server):
                sc64.debug_loop(sd_file)

    except SC64Exception as e:
        print(f"Error: {e}")
        parser.exit(1)
    except KeyboardInterrupt:
        pass
    finally:
        sys.stdout.write("\033[0m")
