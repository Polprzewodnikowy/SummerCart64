use super::Error;
use std::{
    fmt::Display,
    io::{Cursor, Read, Seek, SeekFrom},
};

enum ChunkId {
    UpdateInfo,
    McuData,
    FpgaData,
    BootloaderData,
    PrimerData,
}

impl TryFrom<u32> for ChunkId {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            1 => Self::UpdateInfo,
            2 => Self::McuData,
            3 => Self::FpgaData,
            4 => Self::BootloaderData,
            5 => Self::PrimerData,
            _ => return Err(Error::new("Unknown chunk id inside firmware update file")),
        })
    }
}

impl Display for ChunkId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            ChunkId::UpdateInfo => "Update info",
            ChunkId::McuData => "MCU data",
            ChunkId::FpgaData => "FPGA data",
            ChunkId::BootloaderData => "Bootloader data",
            ChunkId::PrimerData => "Primer data",
        })
    }
}

pub struct Firmware {
    update_info: Option<String>,
    mcu_data: Option<Vec<u8>>,
    fpga_data: Option<Vec<u8>>,
    bootloader_data: Option<Vec<u8>>,
    primer_data: Option<Vec<u8>>,
}

impl Display for Firmware {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if let Some(update_info) = &self.update_info {
            f.write_fmt(format_args!("{}", &update_info))?;
        } else {
            f.write_str("No update info data included")?;
        }
        if let Some(data) = &self.mcu_data {
            f.write_fmt(format_args!(
                "\nMCU data present, length: 0x{:X}",
                data.len()
            ))?;
        } else {
            f.write_str("\nNo MCU data included")?;
        }
        if let Some(data) = &self.fpga_data {
            f.write_fmt(format_args!(
                "\nFPGA data present, length: 0x{:X}",
                data.len()
            ))?;
        } else {
            f.write_str("\nNo FPGA data included")?;
        }
        if let Some(data) = &self.bootloader_data {
            f.write_fmt(format_args!(
                "\nBootloader data present, length: 0x{:X}",
                data.len()
            ))?;
        } else {
            f.write_str("\nNo bootloader data included")?;
        }
        if let Some(data) = &self.primer_data {
            f.write_fmt(format_args!(
                "\nPrimer data present, length: 0x{:X}",
                data.len()
            ))?;
        } else {
            f.write_str("\nNo primer data included")?;
        }
        Ok(())
    }
}

const SC64_FIRMWARE_UPDATE_TOKEN: &[u8; 16] = b"SC64 Update v2.0";

pub fn verify(data: &[u8]) -> Result<Firmware, Error> {
    let mut buffer = vec![0u8; 16];

    let mut reader = Cursor::new(data);

    reader.read(&mut buffer)?;
    if buffer != SC64_FIRMWARE_UPDATE_TOKEN {
        return Err(Error::new("Invalid firmware update header"));
    }

    let mut firmware = Firmware {
        update_info: None,
        mcu_data: None,
        fpga_data: None,
        bootloader_data: None,
        primer_data: None,
    };

    loop {
        let bytes = reader.read(&mut buffer)?;
        if bytes == 16 {
            let id: ChunkId = u32::from_le_bytes(buffer[0..4].try_into().unwrap()).try_into()?;
            let aligned_length = u32::from_le_bytes(buffer[4..8].try_into().unwrap());
            let checksum = u32::from_le_bytes(buffer[8..12].try_into().unwrap());
            let data_length = u32::from_le_bytes(buffer[12..16].try_into().unwrap());

            let mut data = vec![0u8; data_length as usize];
            reader.read(&mut data)?;

            let align = aligned_length - 4 - 4 - data_length;
            reader.seek(SeekFrom::Current(align as i64))?;

            if crc32fast::hash(&data) != checksum {
                return Err(Error::new(
                    format!("Invalid checksum for chunk [{id}]").as_str(),
                ));
            }

            match id {
                ChunkId::UpdateInfo => {
                    firmware.update_info = Some(String::from_utf8_lossy(&data).to_string())
                }
                ChunkId::McuData => firmware.mcu_data = Some(data),
                ChunkId::FpgaData => firmware.fpga_data = Some(data),
                ChunkId::BootloaderData => firmware.bootloader_data = Some(data),
                ChunkId::PrimerData => firmware.primer_data = Some(data),
            }
        } else if bytes == 0 {
            break;
        } else {
            return Err(Error::new("Unexpected end of data in firmware update"));
        }
    }

    Ok(firmware)
}
