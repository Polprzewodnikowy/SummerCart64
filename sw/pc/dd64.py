#!/usr/bin/env python3

import sys
from io import BufferedReader
from typing import Optional



class BadBlockError(Exception):
    pass


class DD64Image:
    __DISK_HEADS = 2
    __DISK_TRACKS = 1175
    __DISK_BLOCKS_PER_TRACK = 2
    __DISK_SECTORS_PER_BLOCK = 85
    __DISK_BAD_TRACKS_PER_ZONE = 12
    __DISK_SYSTEM_SECTOR_SIZE = 232
    __DISK_ZONES = [
        (0, 232, 158, 0),
        (0, 216, 158, 158),
        (0, 208, 149, 316),
        (0, 192, 149, 465),
        (0, 176, 149, 614),
        (0, 160, 149, 763),
        (0, 144, 149, 912),
        (0, 128, 114, 1061),
        (1, 216, 158, 157),
        (1, 208, 158, 315),
        (1, 192, 149, 464),
        (1, 176, 149, 613),
        (1, 160, 149, 762),
        (1, 144, 149, 911),
        (1, 128, 149, 1060),
        (1, 112, 114, 1174),
    ]
    __DISK_VZONE_TO_PZONE = [
        [0, 1, 2, 9, 8, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10],
        [0, 1, 2, 3, 10, 9, 8, 4, 5, 6, 7, 15, 14, 13, 12, 11],
        [0, 1, 2, 3, 4, 11, 10, 9, 8, 5, 6, 7, 15, 14, 13, 12],
        [0, 1, 2, 3, 4, 5, 12, 11, 10, 9, 8, 6, 7, 15, 14, 13],
        [0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7, 15, 14],
        [0, 1, 2, 3, 4, 5, 6, 7, 14, 13, 12, 11, 10, 9, 8, 15],
        [0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8],
    ]
    __DISK_DRIVE_TYPES = [(
        'development',
        192,
        [11, 10, 3, 2],
        [0, 1, 8, 9, 16, 17, 18, 19, 20, 21, 22, 23],
    ), (
        'retail',
        232,
        [9, 8, 1, 0],
        [2, 3, 10, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23],
    )]

    __file: Optional[BufferedReader]
    __drive_type: Optional[str]
    __block_info_table: list[tuple[int, int]]
    loaded: bool = False

    def __init__(self) -> None:
        self.__file = None
        self.__drive_type = None
        block_info_table_length = self.__DISK_HEADS * self.__DISK_TRACKS * self.__DISK_BLOCKS_PER_TRACK
        self.__block_info_table = [None] * block_info_table_length

    def __del__(self) -> None:
        self.unload()

    def __check_system_block(self, lba: int, sector_size: int, check_disk_type: bool) -> tuple[bool, bytes]:
        self.__file.seek(lba * self.__DISK_SYSTEM_SECTOR_SIZE * self.__DISK_SECTORS_PER_BLOCK)
        system_block_data = self.__file.read(sector_size * self.__DISK_SECTORS_PER_BLOCK)
        system_data = system_block_data[:sector_size]
        for sector in range(1, self.__DISK_SECTORS_PER_BLOCK):
            sector_data = system_block_data[(sector * sector_size):][:sector_size]
            if (system_data != sector_data):
                return (False, None)
        if (check_disk_type):
            if (system_data[4] != 0x10):
                return (False, None)
            if ((system_data[5] & 0xF0) != 0x10):
                return (False, None)
        return (True, system_data)

    def __parse_disk(self) -> None:
        disk_system_data = None
        disk_id_data = None
        disk_bad_lbas = []

        drive_index = 0
        while (disk_system_data == None) and (drive_index < len(self.__DISK_DRIVE_TYPES)):
            (drive_type, system_sector_size, system_data_lbas, bad_lbas) = self.__DISK_DRIVE_TYPES[drive_index]
            disk_bad_lbas.clear()
            disk_bad_lbas.extend(bad_lbas)
            for system_lba in system_data_lbas:
                (valid, system_data) = self.__check_system_block(system_lba, system_sector_size, check_disk_type=True)
                if (valid):
                    self.__drive_type = drive_type
                    disk_system_data = system_data
                else:
                    disk_bad_lbas.append(system_lba)
            drive_index += 1

        for id_lba in [15, 14]:
            (valid, id_data) = self.__check_system_block(id_lba, self.__DISK_SYSTEM_SECTOR_SIZE, check_disk_type=False)
            if (valid):
                disk_id_data = id_data
            else:
                disk_bad_lbas.append(id_lba)

        if not (disk_system_data and disk_id_data):
            raise ValueError('Provided 64DD disk file is not valid')

        disk_zone_bad_tracks = []

        for zone in range(len(self.__DISK_ZONES)):
            zone_bad_tracks = []
            start = 0 if zone == 0 else system_data[0x07 + zone]
            stop = system_data[0x07 + zone + 1]
            for offset in range(start, stop):
                zone_bad_tracks.append(system_data[0x20 + offset])
            for ignored_track in range(self.__DISK_BAD_TRACKS_PER_ZONE - len(zone_bad_tracks)):
                zone_bad_tracks.append(self.__DISK_ZONES[zone][2] - ignored_track - 1)
            disk_zone_bad_tracks.append(zone_bad_tracks)

        disk_type = disk_system_data[5] & 0x0F

        current_lba = 0
        starting_block = 0
        disk_file_offset = 0

        for zone in self.__DISK_VZONE_TO_PZONE[disk_type]:
            (head, sector_size, tracks, track) = self.__DISK_ZONES[zone]

            for zone_track in range(tracks):
                current_zone_track = (
                    (tracks - 1) - zone_track) if head else zone_track

                if (current_zone_track in disk_zone_bad_tracks[zone]):
                    track += (-1) if head else 1
                    continue

                for block in range(self.__DISK_BLOCKS_PER_TRACK):
                    index = (track << 2) | (head << 1) | (starting_block ^ block)
                    if (current_lba not in disk_bad_lbas):
                        self.__block_info_table[index] = (disk_file_offset, sector_size * self.__DISK_SECTORS_PER_BLOCK)
                    else:
                        self.__block_info_table[index] = None
                    disk_file_offset += sector_size * self.__DISK_SECTORS_PER_BLOCK
                    current_lba += 1

                track += (-1) if head else 1
                starting_block ^= 1

    def __check_track_head_block(self, track: int, head: int, block: int) -> None:
        if (track < 0 or track >= self.__DISK_TRACKS):
            raise ValueError('Track outside of possible range')
        if (head < 0 or head >= self.__DISK_HEADS):
            raise ValueError('Head outside of possible range')
        if (block < 0 or block >= self.__DISK_BLOCKS_PER_TRACK):
            raise ValueError('Block outside of possible range')

    def __get_table_index(self, track: int, head: int, block: int) -> int:
        return (track << 2) | (head << 1) | (block)

    def __get_block_info(self, track: int, head: int, block: int) -> Optional[tuple[int, int]]:
        if (self.__file.closed):
            return None
        self.__check_track_head_block(track, head, block)
        index = self.__get_table_index(track, head, block)
        return self.__block_info_table[index]

    def load(self, path: str) -> None:
        self.unload()
        self.__file = open(path, 'rb+')
        self.__parse_disk()
        self.loaded = True

    def unload(self) -> None:
        self.loaded = False
        if (self.__file != None and not self.__file.closed):
            self.__file.close()
        self.__drive_type = None

    def get_drive_type(self) -> str:
        return self.__drive_type

    def read_block(self, track: int, head: int, block: int) -> bytes:
        info = self.__get_block_info(track, head, block)
        if (info == None):
            raise BadBlockError
        (offset, block_size) = info
        self.__file.seek(offset)
        return self.__file.read(block_size)

    def write_block(self, track: int, head: int, block: int, data: bytes) -> None:
        info = self.__get_block_info(track, head, block)
        if (info == None):
            raise BadBlockError
        (offset, block_size) = info
        if (len(data) != block_size):
            raise ValueError(f'Provided data block size is different than expected ({len(data)} != {block_size})')
        self.__file.seek(offset)
        self.__file.write(data)



if __name__ == '__main__':
    id_lba_locations = [
        (7, 0, 1),
        (7, 0, 0)
    ]
    if (len(sys.argv) >= 2):
        dd = DD64Image()
        dd.load(sys.argv[1])
        print(dd.get_drive_type())
        for (track, head, block) in id_lba_locations:
            try:
                print(dd.read_block(track, head, block)[:4])
            except BadBlockError:
                print(f'Bad ID block [track: {track}, head: {head}, block: {block}]')
        dd.unload()
    else:
        print(f'[{sys.argv[0]}]: Expected disk image path as first argument')
