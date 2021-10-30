import os
import serial
import time
import filecmp



class SC64:
    __CFG_ID_FLASH_OPERATION = 10
    __CFG_ID_RECONFIGURE = 11


    def __init__(self, port):
        self.__serial = serial.Serial(port, timeout=10.0, write_timeout=10.0)


    def __query_config(self, query, arg=0):
        self.__serial.write(b'CMDQ')
        self.__serial.write(query.to_bytes(4, byteorder='big'))
        self.__serial.write(arg.to_bytes(4, byteorder='big'))
        value = self.__serial.read(4)
        if (self.__serial.read(4).decode() != 'CMPQ'):
            raise Exception('Bad query response')
        return int.from_bytes(value, byteorder='big')


    def __change_config(self, change, arg=0, ignore_response=False):
        self.__serial.write(b'CMDC')
        self.__serial.write(change.to_bytes(4, byteorder='big'))
        self.__serial.write(arg.to_bytes(4, byteorder='big'))
        if (not ignore_response and self.__serial.read(4).decode() != 'CMPC'):
            raise Exception('Bad change response')


    def reconfigure(self):
        magic = self.__query_config(self.__CFG_ID_RECONFIGURE)
        self.__change_config(self.__CFG_ID_RECONFIGURE, magic, ignore_response=True)
        time.sleep(0.2)


    def read_flash(self, file):
        size = self.__query_config(self.__CFG_ID_FLASH_OPERATION)
        print(f'Flash size: {(size / 1024.0):1.1f} kB')
        self.__serial.write(b'CMDR')
        self.__serial.write((0).to_bytes(4, byteorder='big'))
        self.__serial.write((size).to_bytes(4, byteorder='big'))
        flash = self.__serial.read(size)
        response = self.__serial.read(4)
        if (response.decode() == 'CMPR'):
            with open(file, 'wb') as f:
                f.write(flash)
        else:
            raise Exception('There was a problem while reading flash data')


    def program_flash(self, file):
        length = os.path.getsize(file)
        offset = 0
        with open(file, 'rb') as f:
            self.__serial.write(b'CMDW')
            self.__serial.write(offset.to_bytes(4, byteorder='big'))
            self.__serial.write(length.to_bytes(4, byteorder='big'))
            self.__serial.write(f.read())
            response = self.__serial.read(4)
            if (response.decode() != 'CMPW'):
                raise Exception('There was a problem while sending flash data')
        self.__change_config(self.__CFG_ID_FLASH_OPERATION)



file = '../../fw/output_files/SC64_update.bin'
backup_file = 'SC64_backup.bin'
verify_file = 'SC64_update_verify.bin'
port = 'COM7'

sc64 = SC64(port)

print('Making backup...')
sc64.read_flash(backup_file)
print('done\n')

print('Flashing... ')
sc64.program_flash(file)
print('done\n')

print('Reconfiguring... ')
sc64.reconfigure()
print('done\n')

print('Verifying... ')
sc64.read_flash(verify_file)
if (filecmp.cmp(file, verify_file)):
    print('success!\n')
else:
    print('failure.\n')

print('Update done!')
