use std::io::{Error, Read, Seek, SeekFrom};

pub enum SaveType {
    None,
    Eeprom4k,
    Eeprom16k,
    Sram,
    SramBanked,
    Flashram,
    Sram128kB,
}

const HASH_CHUNK_LENGTH: usize = 1 * 1024 * 1024;

pub fn guess_save_type<T: Read + Seek>(
    reader: &mut T,
) -> Result<(SaveType, Option<String>), Error> {
    let mut ed64_header = vec![0u8; 4];

    reader.seek(SeekFrom::Start(0x3C))?;
    reader.read_exact(&mut ed64_header)?;

    if &ed64_header[0..2] == b"ED" {
        return Ok((
            match ed64_header[3] >> 4 {
                1 => SaveType::Eeprom4k,
                2 => SaveType::Eeprom16k,
                3 => SaveType::Sram,
                4 => SaveType::SramBanked,
                5 => SaveType::Flashram,
                6 => SaveType::Sram128kB,
                _ => SaveType::None,
            },
            None,
        ));
    }

    let mut pi_config = vec![0u8; 4];

    reader.rewind()?;
    reader.read_exact(&mut pi_config)?;

    let endian_swapper = match &pi_config[0..4] {
        [0x37, 0x80, 0x40, 0x12] => |b: &mut [u8]| b.chunks_exact_mut(2).for_each(|c| c.swap(0, 1)),
        [0x40, 0x12, 0x37, 0x80] => |b: &mut [u8]| {
            b.chunks_exact_mut(4).for_each(|c| {
                c.swap(0, 3);
                c.swap(1, 2)
            })
        },
        _ => |_: &mut [u8]| {},
    };

    let mut hasher = md5::Context::new();
    let mut buffer = vec![0u8; HASH_CHUNK_LENGTH];

    reader.rewind()?;
    loop {
        let chunk = reader.read(&mut buffer)?;
        if chunk > 0 {
            endian_swapper(&mut buffer[0..chunk]);
            hasher.consume(&buffer[0..chunk]);
        } else {
            break;
        }
    }

    let hash = hex::encode_upper(hasher.compute().0);

    let database_ini = include_str!("../data/mupen64plus.ini");
    let database = ini::Ini::load_from_str(database_ini)
        .expect("Error during mupen64plus.ini parse operation");
    if let Some(section) = database.section(Some(hash)) {
        let save_type = section.get("SaveType").map_or(SaveType::None, |t| match t {
            "Eeprom 4KB" => SaveType::Eeprom4k,
            "Eeprom 16KB" => SaveType::Eeprom16k,
            "SRAM" => SaveType::Sram,
            "Flash RAM" => SaveType::Flashram,
            _ => SaveType::None,
        });
        let title = section.get("GoodName").map(|s| s.to_string());
        return Ok((save_type, title));
    }

    Ok((SaveType::None, None))
}
