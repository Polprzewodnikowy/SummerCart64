use crate::sc64::Error;
use std::{
    collections::HashMap,
    fs::File,
    io::{Read, Seek, SeekFrom, Write},
};

const BLOCKS_PER_TRACK: usize = 2;
const SECTORS_PER_BLOCK: usize = 85;
const SYSTEM_SECTOR_LENGTH: usize = 232;
const BAD_TRACKS_PER_ZONE: usize = 12;

#[derive(Clone, Copy, PartialEq)]
pub enum Format {
    Retail,
    Development,
}

struct SystemAreaInfo<'a> {
    format: Format,
    sector_length: usize,
    sys_lba: &'a [usize],
    bad_lba: &'a [usize],
}

const SYSTEM_AREA: [SystemAreaInfo; 2] = [
    SystemAreaInfo {
        format: Format::Retail,
        sector_length: 232,
        sys_lba: &[9, 8, 1, 0],
        bad_lba: &[2, 3, 10, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23],
    },
    SystemAreaInfo {
        format: Format::Development,
        sector_length: 192,
        sys_lba: &[11, 10, 3, 2],
        bad_lba: &[0, 1, 8, 9, 16, 17, 18, 19, 20, 21, 22, 23],
    },
];

const ID_LBAS: [usize; 2] = [15, 14];

struct DiskZone {
    head: usize,
    sector_length: usize,
    tracks: usize,
    track_offset: usize,
}

macro_rules! zone {
    ($h:expr, $l:expr, $t:expr, $o:expr) => {
        DiskZone {
            head: $h,
            sector_length: $l,
            tracks: $t,
            track_offset: $o,
        }
    };
}

const DISK_ZONES: [DiskZone; 16] = [
    zone!(0, 232, 158, 0),
    zone!(0, 216, 158, 158),
    zone!(0, 208, 149, 316),
    zone!(0, 192, 149, 465),
    zone!(0, 176, 149, 614),
    zone!(0, 160, 149, 763),
    zone!(0, 144, 149, 912),
    zone!(0, 128, 114, 1061),
    zone!(1, 216, 158, 157),
    zone!(1, 208, 158, 315),
    zone!(1, 192, 149, 464),
    zone!(1, 176, 149, 613),
    zone!(1, 160, 149, 762),
    zone!(1, 144, 149, 911),
    zone!(1, 128, 149, 1060),
    zone!(1, 112, 114, 1174),
];

const DISK_VZONE_TO_PZONE: [[usize; 16]; 7] = [
    [0, 1, 2, 9, 8, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10],
    [0, 1, 2, 3, 10, 9, 8, 4, 5, 6, 7, 15, 14, 13, 12, 11],
    [0, 1, 2, 3, 4, 11, 10, 9, 8, 5, 6, 7, 15, 14, 13, 12],
    [0, 1, 2, 3, 4, 5, 12, 11, 10, 9, 8, 6, 7, 15, 14, 13],
    [0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7, 15, 14],
    [0, 1, 2, 3, 4, 5, 6, 7, 14, 13, 12, 11, 10, 9, 8, 15],
    [0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8],
];

struct Mapping {
    lba: usize,
    offset: usize,
    length: usize,
    writable: bool,
}

pub struct Disk {
    file: File,
    format: Format,
    mapping: HashMap<usize, Mapping>,
}

impl Disk {
    pub fn get_format(&self) -> &Format {
        &self.format
    }

    pub fn get_lba(&self, track: u32, head: u32, block: u32) -> Option<usize> {
        if head == 0 && track < 12 {
            return Some((track << 1 | block ^ (track % 2)) as usize);
        }
        let location = track << 2 | head << 1 | block;
        self.mapping
            .get(&(location as usize))
            .map(|block| block.lba)
    }

    pub fn read_block(
        &mut self,
        track: u32,
        head: u32,
        block: u32,
    ) -> Result<Option<Vec<u8>>, Error> {
        let location = track << 2 | head << 1 | block;
        if let Some(block) = self.mapping.get(&(location as usize)) {
            let mut data = vec![0u8; block.length];
            self.file.seek(SeekFrom::Start(block.offset as u64))?;
            self.file.read_exact(&mut data)?;
            return Ok(Some(data));
        }
        Ok(None)
    }

    pub fn write_block(
        &mut self,
        track: u32,
        head: u32,
        block: u32,
        data: &[u8],
    ) -> Result<Option<()>, Error> {
        let location = track << 2 | head << 1 | block;
        if let Some(block) = self.mapping.get(&(location as usize)) {
            if block.length == data.len() && block.writable {
                self.file.seek(SeekFrom::Start(block.offset as u64))?;
                self.file.write_all(data)?;
                return Ok(Some(()));
            }
        }
        Ok(None)
    }
}

pub fn open(path: &str) -> Result<Disk, Error> {
    let mut file = File::options().read(true).write(true).open(path)?;
    let (format, mapping) = load_ndd(&mut file)?;
    Ok(Disk {
        file,
        format,
        mapping,
    })
}

pub fn open_multiple(paths: &[String]) -> Result<Vec<Disk>, Error> {
    let mut disks: Vec<Disk> = Vec::new();

    for path in paths {
        let disk = open(path)?;
        disks.push(disk);
    }

    if !disks.windows(2).all(|d| d[0].format == d[1].format) {
        return Err(Error::new("Disk format mismatch"));
    }

    Ok(disks)
}

fn load_ndd(file: &mut File) -> Result<(Format, HashMap<usize, Mapping>), Error> {
    let mut disk_format: Option<Format> = None;
    let mut disk_type: u8 = 0;
    let mut sys_data = vec![0u8; SYSTEM_SECTOR_LENGTH];
    let mut bad_lba: Vec<usize> = Vec::new();

    for info in SYSTEM_AREA {
        bad_lba.clear();
        for &lba in info.sys_lba {
            let data = load_sys_lba(file, lba)?;
            if verify_sys_lba(&data, info.sector_length) {
                if (data[4] != 0x10) || ((data[5] & 0xF0) != 0x10) {
                    bad_lba.push(lba);
                } else {
                    disk_format = Some(info.format);
                    disk_type = data[5] & 0x0F;
                    sys_data = data[0..SYSTEM_SECTOR_LENGTH].to_vec();
                }
            } else {
                bad_lba.push(lba);
            }
        }
        if disk_format.is_some() {
            bad_lba.append(&mut info.bad_lba.to_vec());
            break;
        }
    }

    if disk_format.is_none() {
        return Err(Error::new("Provided 64DD disk file is not valid"));
    }
    if disk_type >= DISK_VZONE_TO_PZONE.len() as u8 {
        return Err(Error::new("Unknown disk type"));
    }

    let mut id_lba_valid = false;
    for lba in ID_LBAS {
        let data = load_sys_lba(file, lba)?;
        let valid = verify_sys_lba(&data, SYSTEM_SECTOR_LENGTH);
        if !valid {
            bad_lba.push(lba);
        }
        id_lba_valid |= valid;
    }

    if !id_lba_valid {
        return Err(Error::new("No valid ID LBA found"));
    }

    let mut zone_bad_tracks: Vec<Vec<usize>> = Vec::new();

    for (zone, info) in DISK_ZONES.iter().enumerate() {
        let mut bad_tracks: Vec<usize> = Vec::new();
        let start = if zone == 0 { 0 } else { sys_data[0x07 + zone] };
        let stop = sys_data[0x07 + zone + 1];
        for offset in start..stop {
            bad_tracks.push(sys_data[0x20 + offset as usize] as usize);
        }
        for track in 0..(BAD_TRACKS_PER_ZONE - bad_tracks.len()) {
            bad_tracks.push(info.tracks - track - 1);
        }
        zone_bad_tracks.push(bad_tracks);
    }

    let mut mapping = HashMap::new();

    let mut current_lba: usize = 0;
    let mut starting_block: usize = 0;
    let mut file_offset: usize = 0;

    for zone in DISK_VZONE_TO_PZONE[disk_type as usize] {
        let DiskZone {
            head,
            sector_length,
            tracks,
            track_offset,
        } = DISK_ZONES[zone];

        let mut track = track_offset;

        for zone_track in 0..tracks {
            let current_zone_track = if head == 0 {
                zone_track
            } else {
                (tracks - 1) - zone_track
            };

            if zone_bad_tracks[zone].contains(&current_zone_track) {
                track = if head == 0 {
                    track + 1
                } else {
                    track.saturating_sub(1)
                };
                continue;
            }

            for block in 0..BLOCKS_PER_TRACK {
                let location = track << 2 | head << 1 | (starting_block ^ block);
                let length = sector_length * SECTORS_PER_BLOCK;
                if !bad_lba.contains(&current_lba) {
                    mapping.insert(
                        location,
                        Mapping {
                            lba: current_lba,
                            offset: file_offset,
                            length,
                            writable: true,
                        },
                    );
                }
                file_offset += length;
                current_lba += 1;
            }

            track = if head == 0 {
                track + 1
            } else {
                track.saturating_sub(1)
            };
            starting_block ^= 1;
        }
    }

    Ok((disk_format.unwrap(), mapping))
}

fn load_sys_lba(file: &mut File, lba: usize) -> Result<Vec<u8>, Error> {
    let length = SYSTEM_SECTOR_LENGTH * SECTORS_PER_BLOCK;
    file.seek(SeekFrom::Start((lba * length) as u64))?;
    let mut data = vec![0u8; length];
    file.read_exact(&mut data)?;
    Ok(data)
}

fn verify_sys_lba(data: &[u8], sector_length: usize) -> bool {
    let sys_data = &data[0..sector_length];
    for sector in 1..SECTORS_PER_BLOCK {
        let offset = sector * sector_length;
        let verify_data = &data[offset..(offset + sector_length)];
        if sys_data != verify_data {
            return false;
        }
    }
    true
}
