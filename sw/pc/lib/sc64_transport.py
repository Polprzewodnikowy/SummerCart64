from serial.tools import list_ports
from threading import Thread
from typing import Optional
import queue
import serial
import time


class ConnectionException(Exception):
    pass


class SC64Transport:
    __disconnect = False
    __serial = None
    __thread_read = None
    __thread_write = None
    __queue_output = queue.Queue()
    __queue_input = queue.Queue()
    __queue_packet = queue.Queue()

    def __del__(self) -> None:
        self.__disconnect = True
        if (self.__thread_read.is_alive()):
            self.__thread_read.join(5)
        if (self.__thread_write.is_alive()):
            self.__thread_write.join(5)
        if (self.__serial != None and self.__serial.is_open):
            self.__serial.close()

    def connect(self) -> None:
        ports = list_ports.comports()
        device_found = False

        if (self.__serial != None and self.__serial.is_open):
            raise ConnectionException("Serial port is already open")

        for p in ports:
            if (p.vid == 0x0403 and p.pid == 0x6014 and p.serial_number.startswith("SC64")):
                try:
                    self.__serial = serial.Serial(
                        p.device, timeout=1.0, write_timeout=5.0)
                    self.__reset_link()
                except (serial.SerialException, ConnectionException):
                    if (self.__serial):
                        self.__serial.close()
                    continue
                device_found = True
                break

        if (not device_found):
            raise ConnectionException("No SummerCart64 device was found")

        self.__thread_read = Thread(
            target=self.__serial_process_input, daemon=True)
        self.__thread_write = Thread(
            target=self.__serial_process_output, daemon=True)

        self.__thread_read.start()
        self.__thread_write.start()

    def __reset_link(self) -> None:
        self.__serial.reset_output_buffer()

        retry_counter = 0
        self.__serial.dtr = 1
        while (self.__serial.dsr == 0):
            time.sleep(0.1)
            retry_counter += 1
            if (retry_counter >= 10):
                raise ConnectionException

        self.__serial.reset_input_buffer()

        retry_counter = 0
        self.__serial.dtr = 0
        while (self.__serial.dsr == 1):
            time.sleep(0.1)
            retry_counter += 1
            if (retry_counter >= 10):
                raise ConnectionException

    def __write(self, data: bytes) -> None:
        try:
            if (self.__disconnect):
                raise ConnectionException
            self.__serial.write(data)
            self.__serial.flush()
        except serial.SerialTimeoutException:
            raise ConnectionException
        except serial.SerialException:
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
        return int.from_bytes(self.__read(4), byteorder="big")

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
                        lenss = self.__read_int()
                        data = self.__read(lenss)
                        self.__queue_packet.put((command, data))
                    elif (identifier == b'CMP' or identifier == b'ERR'):
                        data = self.__read(self.__read_int())
                        success = identifier == b'CMP'
                        self.__queue_input.put((command, data, success))
                    else:
                        raise Exception
            except ConnectionException:
                break

    def __queue_cmd(self, command: bytes, args: list[int] = [], data: bytes = b''):
        packet: bytes = b'CMD'
        packet += command
        for arg in args:
            packet += arg.to_bytes(4, byteorder="big")
        packet += data
        self.__queue_output.put(packet)

    def __pop_response(self, command: bytes) -> bytes:
        try:
            (response_command, data, success) = self.__queue_input.get(timeout=5)
            if (command != response_command or success == False):
                raise ConnectionException
            return data
        except queue.Empty:
            raise ConnectionException

    def execute_cmd(self, cmd: bytes, args: list[int] = [], data: bytes = b'', response: bool = True) -> Optional[bytes]:
        if (len(cmd) != 1):
            raise ValueError("Length of command is different than 1 byte")
        command = cmd[0:1]
        if (len(args) != 2):
            raise ValueError("Number of arguments is different than 2")
        self.__queue_cmd(command, args, data)
        if (response):
            return self.__pop_response(command)
        return None

    def get_packet(self) -> Optional[tuple[bytes, bytes]]:
        try:
            return self.__queue_packet.get(timeout=1)
        except queue.Empty:
            return None
