#!/usr/bin/env python3

import argparse
import progressbar
import struct
import sys
import time
from pyftdi.spi import SpiController



class SummerBanger64:

    # Command ids

    __CMD_STATUS            = 0x00
    __CMD_CONFIG            = 0x10
    __CMD_ADDR              = 0x20
    __CMD_READ_LENGTH       = 0x30
    __CMD_WRITE             = 0x40
    __CMD_READ              = 0x50
    __CMD_CART_RESET        = 0xFC
    __CMD_FLUSH_WRITE       = 0xFD
    __CMD_FLUSH_READ        = 0xFE
    __CMD_SPI_RESET         = 0xFF

    # Size declarations

    __FIFO_SIZE             = 1024
    __FIFO_SIZE_BYTES       = __FIFO_SIZE * 4
    __FLASH_PAGE_SIZE       = 256

    # Cart addresses

    __SDRAM_ADDRESS         = 0x10000000
    __FLASH_ADDRESS         = 0x18000000
    __FLASH_CFG_ADDRESS     = 0x1C000000
    __CART_CONFIG_ADDRESS   = 0x1E000000
    __CIC_TYPE_ADDRESS      = 0x1E000004

    # Cart config register bits

    __FLASH_ENABLE          = (1 << 0)
    __SDRAM_ENABLE          = (1 << 1)


    def __init__(self, config = {}):
        self.__spi_controller = SpiController(cs_count=1)
        self.__spi_controller.configure('ftdi://ftdi:2232h/1')
        self.__spi = self.__spi_controller.get_port(0, float(config['spi_speed']))

        self.__update_progress = config['update_progress']

        if (not config['no_init']):
            self.__reset()
            self.__set_config(n64_disable=True, address_increment=True)
            self.__write_word(self.__CART_CONFIG_ADDRESS, (self.__SDRAM_ENABLE | self.__FLASH_ENABLE))

        self.__config = config


    def __del__(self):
        if (not self.__config['no_init']):
            self.__reset()
        self.__spi_controller.terminate()


    def __get_status(self, field=None):
        status_word = struct.unpack('>I', self.__spi.exchange([self.__CMD_STATUS], 4))[0]
        status = {
            'status': hex(status_word),
            'control_byte': (status_word & (0xFF << 24)) >> 24,
            'address_increment': (status_word & (0x1 << 23)) >> 23,
            'n64_disabled': (status_word & (0x1 << 22)) >> 22,
            'write_used': (status_word & (0x7FF << 11)) >> 11,
            'read_used': (status_word & (0x7FF << 0)) >> 0,
        }
        return status.get(field) if field else status


    def __set_config(self, n64_disable=False, address_increment=True):
        config = (
            ((1 if address_increment else 0) << 1) |
            ((1 if n64_disable else 0) << 0)
        )
        self.__spi.write(bytearray([self.__CMD_CONFIG]) + struct.pack('>I', int(config)))


    def __set_address(self, address):
        self.__spi.write(bytearray([self.__CMD_ADDR]) + struct.pack('>I', int(address)))


    def __set_read_length(self, length):
        self.__spi.write(bytearray([self.__CMD_READ_LENGTH]) + struct.pack('>I', int(length - 1)))


    def __read_data(self, length):
        return self.__spi.exchange([self.__CMD_READ], int(length * 4))


    def __write_data(self, data):
        self.__spi.write(bytearray([self.__CMD_WRITE]) + data)


    def __cart_reset(self):
        self.__spi.write([self.__CMD_CART_RESET, 0x00, 0x00, 0x00, 0x00])


    def __spi_reset(self):
        self.__spi.write([self.__CMD_SPI_RESET])


    def __reset(self):
        self.__spi_reset()
        self.__cart_reset()


    def __wait_for_read_fill(self, length):
        while (self.__get_status('read_used') < int(length)):
            pass


    def __wait_for_write_space(self, length):
        while ((self.__FIFO_SIZE - self.__get_status('write_used')) < int(length)):
            pass


    def __wait_for_write_completion(self):
        while (self.__get_status('write_used') > 0):
            pass


    def __read_word(self, address):
        self.__set_address(address)
        self.__set_read_length(1)
        self.__wait_for_read_fill(1)
        return struct.unpack('>I', self.__read_data(1))[0]


    def __write_word(self, address, data):
        self.__set_address(address)
        self.__wait_for_write_space(1)
        self.__write_data(struct.pack('>I', data))


    def __flash_operation(self, byte, mode='s', direction='w', select=False, cfg_mode=True):
        return int(
                ((1 if cfg_mode else 0) << 12) |
                ((1 if mode == 'q' else 0) << 11) |
                ((1 if direction == 'w' else 0) << 9) |
                ((1 if select else 0) << 8) |
                (byte & 0xFF)
            )


    def __flash_create_operation(self, byte, mode='s', direction='w', select=False, cfg_mode=True):
        return bytearray(struct.pack('>I', self.__flash_operation(byte, mode, direction, select, cfg_mode)))


    def __flash_issue_operation(self, byte, mode='s', direction='w', select=False, cfg_mode=True):
        self.__write_word(self.__FLASH_CFG_ADDRESS, self.__flash_operation(byte, mode, direction, select, cfg_mode))
        return ((self.__read_word(self.__FLASH_CFG_ADDRESS) & 0xFF) if direction == 'r' else 0x00).to_bytes(1, 'big')


    def __flash_exchange(self, command, address=None, data=[], length=0, address_mode='s', write_mode='s', read_mode='s', cfg_mode=True):
        operations = bytearray([])

        operations += self.__flash_create_operation(command)
        
        if (address != None):
            for byte in bytearray(struct.pack('>I', address))[1:4]:
                operations += self.__flash_create_operation(byte, mode=address_mode)
        
        for byte in data:
            operations += self.__flash_create_operation(byte, mode=write_mode)

        if (not length):
            operations += self.__flash_create_operation(0x00, select=True, cfg_mode=cfg_mode)

        self.__set_address(self.__FLASH_CFG_ADDRESS)
        self.__write_data(operations)
        self.__wait_for_write_completion()

        data_read = bytearray([])

        for _ in range(length):
            data_read += self.__flash_issue_operation(0x00, mode=read_mode, direction='r')

        if (length > 0):
            self.__flash_issue_operation(0x00, select=True, cfg_mode=cfg_mode)

        return data_read


    def __flash_exit_xip(self):
        self.__flash_exchange(0xFF, data=[0xFF, 0xFF])


    def __flash_enter_xip(self):
        self.__flash_exchange(
            0xEB,
            address=0,
            data=[0xA0, 0x00, 0x00],
            length=1,
            address_mode='q',
            write_mode='q',
            read_mode='q',
            cfg_mode=False
        )


    def __flash_read_status(self):
        return self.__flash_exchange(0x05, length=1)[0]

    
    def __flash_write_enable(self):
        self.__flash_exchange(0x06)


    def __flash_write_disable(self):
        self.__flash_exchange(0x04)


    def __flash_sector_erase(self, address):
        self.__flash_exchange(0x20, address=address)


    def __flash_chip_erase(self):
        self.__flash_exchange(0xC7)
        self.__flash_exchange(0x60)


    def __flash_program_page(self, address, data):
        self.__flash_exchange(0x32, address=address, data=data, address_mode='s', write_mode='q')


    def __flash_wait_for_not_busy(self):
        while (self.__flash_read_status() & 0x01):
            pass


    def __flash_wait_for_write_enable_latch(self):
        while (not (self.__flash_read_status() & 0x02)):
            pass


    def __calculate_length_in_words(self, length):
        return int((length + 3) / 4)


    def __get_chunk_iterator(self, data, chunk_size):
        for i in range(0, len(data), chunk_size):
            yield (data[i:i + chunk_size], i)


    def print_status(self):
        print(self.__get_status())


    def read_rom(self, length, from_flash=False):
        length_in_words = self.__calculate_length_in_words(length)
        chunk_size = int((self.__FIFO_SIZE_BYTES * 3) / 4)
        data = bytearray([])

        self.__set_address(self.__FLASH_ADDRESS if from_flash else self.__SDRAM_ADDRESS)
        self.__set_read_length(length_in_words)

        while (length > 0):
            current_chunk_size = min(length, chunk_size)
            read_length_in_words = self.__calculate_length_in_words(current_chunk_size)
            self.__wait_for_read_fill(read_length_in_words)
            data += self.__read_data(read_length_in_words)
            length -= current_chunk_size
            if (self.__update_progress): self.__update_progress(len(data))

        return data


    def write_sdram(self, data):
        length = len(data)
        chunk_size = int((self.__FIFO_SIZE_BYTES * 3) / 4)

        self.__set_address(self.__SDRAM_ADDRESS)

        for (chunk, offset) in self.__get_chunk_iterator(data, chunk_size):
            current_chunk_size = min(chunk_size, length - offset)
            self.__wait_for_write_space(self.__calculate_length_in_words(current_chunk_size))
            self.__write_data(chunk)
            if (self.__update_progress): self.__update_progress(offset)

        if (self.__update_progress): self.__update_progress(length)

        self.__wait_for_write_completion()


    def write_flash(self, data):
        self.__set_config(n64_disable=True, address_increment=False)

        self.__flash_exit_xip()

        print('\rErasing Flash, this may take a while...', end=' ')

        self.__flash_write_enable()
        self.__flash_wait_for_write_enable_latch()
        self.__flash_chip_erase()
        self.__flash_wait_for_not_busy()

        print('Done')

        for (page, offset) in self.__get_chunk_iterator(data, self.__FLASH_PAGE_SIZE):
            self.__flash_write_enable()
            self.__flash_wait_for_write_enable_latch()
            self.__flash_program_page(offset, page)
            self.__flash_wait_for_not_busy()
            if (self.__update_progress): self.__update_progress(offset)

        self.__flash_write_disable()

        self.__flash_enter_xip()

        self.__set_config(n64_disable=True, address_increment=True)


    def set_cic_type(self, cic_type=0):
        cic_lut = {
            6101: 0x11,
            6102: 0x12,
            6103: 0x13,
            6105: 0x14,
            6106: 0x15,
            7101: 0x02,
            7102: 0x01,
            7103: 0x03,
            7105: 0x04,
            7106: 0x05,
        }
        self.__write_word(self.__CIC_TYPE_ADDRESS, int(cic_lut.get(cic_type) or 0))



if __name__ == "__main__":
    formatter = lambda prog: argparse.HelpFormatter(prog, max_help_position=30)
    parser = argparse.ArgumentParser(description='Write/Read N64 ROM to SummerCart', formatter_class=formatter)
    parser.add_argument('-s', '--speed', default=30E6, required=False, help='set SPI communication speed (in Hz, 30MHz max)')
    parser.add_argument('-r', '--read', action='store_true', required=False, help='read ROM instead of writing')
    parser.add_argument('-l', '--length', required=False, help='specify ROM length to read (in bytes)', default=2**26)
    parser.add_argument('-f', '--flash', action='store_true', required=False, help='use Flash instead of SDRAM')
    parser.add_argument('-c', '--cic', type=int, required=False, help='set CIC type to use by bootloader')
    parser.add_argument('-q', '--status', action='store_true', required=False, help='just query and print the status word')
    parser.add_argument('rom', help='path to ROM file (only .z64 files are supported)', nargs='?')
    args = parser.parse_args()

    if ((len(sys.argv) == 1) or (not args.rom and (args.read or args.flash))):
        parser.print_help()
        parser.exit()

    bar = progressbar.DataTransferBar()

    def update_progress(length):
        bar.update(length)

    config = {
        'no_init': args.status,
        'spi_speed': args.speed,
        'update_progress': update_progress,
    }

    banger = SummerBanger64(config)

    if (args.cic != None):
        banger.set_cic_type(args.cic)
    elif (args.rom and not args.flash):
        banger.set_cic_type()

    if (args.status):
        banger.print_status()
    elif (args.rom):
        if (args.read):
            with open(args.rom, 'wb') as f:
                length = int(args.length)
                bar.max_value = length
                data = banger.read_rom(length, from_flash=args.flash)
                f.write(data)
        else:
            with open(args.rom, 'rb') as f:
                data = f.read()
                bar.max_value = len(data)
                if (args.flash):
                    banger.write_flash(data)
                else:
                    banger.write_sdram(data)
