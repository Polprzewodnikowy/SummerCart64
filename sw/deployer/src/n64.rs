use std::io::{Error, Read, Seek};

pub enum SaveType {
    None,
    Eeprom4k,
    Eeprom16k,
    Sram,
    SramBanked,
    Flashram,
}

pub fn guess_save_type<T: Read + Seek>(reader: &mut T) -> Result<SaveType, Error> {
    let mut header = vec![0u8; 0x40];

    reader.rewind()?;
    reader.read_exact(&mut header)?;

    let pi_config = &header[0..4];
    match pi_config {
        [0x37, 0x80, 0x40, 0x12] => {
            header.chunks_exact_mut(2).for_each(|c| c.swap(0, 1));
        }
        [0x40, 0x12, 0x37, 0x80] => {
            header.chunks_exact_mut(4).for_each(|c| {
                c.swap(0, 3);
                c.swap(1, 2);
            });
        }
        _ => {}
    }

    let rom_id = &header[0x3B..0x3E];
    // let region = header[0x3E];
    // let revision = header[0x3F];

    // TODO: fix this mess

    if let Some(save_type) = match rom_id {
        b"NTW" | b"NHF" | b"NOS" | b"NTC" | b"NER" | b"NAG" | b"NAB" | b"NS3" | b"NTN" | b"NBN"
        | b"NBK" | b"NFH" | b"NMU" | b"NBC" | b"NBH" | b"NHA" | b"NBM" | b"NBV" | b"NBD"
        | b"NCT" | b"NCH" | b"NCG" | b"NP2" | b"NXO" | b"NCU" | b"NCX" | b"NDY" | b"NDQ"
        | b"NDR" | b"NN6" | b"NDU" | b"NJM" | b"NFW" | b"NF2" | b"NKA" | b"NFG" | b"NGL"
        | b"NGV" | b"NGE" | b"NHP" | b"NPG" | b"NIJ" | b"NIC" | b"NFY" | b"NKI" | b"NLL"
        | b"NLR" | b"NKT" | b"CLB" | b"NLB" | b"NMW" | b"NML" | b"NTM" | b"NMI" | b"NMG"
        | b"NMO" | b"NMS" | b"NMR" | b"NCR" | b"NEA" | b"NPW" | b"NPY" | b"NPT" | b"NRA"
        | b"NWQ" | b"NSU" | b"NSN" | b"NK2" | b"NSV" | b"NFX" | b"NFP" | b"NS6" | b"NNA"
        | b"NRS" | b"NSW" | b"NSC" | b"NSA" | b"NB6" | b"NSS" | b"NTX" | b"NT6" | b"NTP"
        | b"NTJ" | b"NRC" | b"NTR" | b"NTB" | b"NGU" | b"NIR" | b"NVL" | b"NVY" | b"NWC"
        | b"NAD" | b"NWU" | b"NYK" | b"NMZ" | b"NSM" | b"NWR" => Some(SaveType::Eeprom4k),
        b"NB7" | b"NGT" | b"NFU" | b"NCW" | b"NCZ" | b"ND6" | b"NDO" | b"ND2" | b"N3D" | b"NMX"
        | b"NGC" | b"NIM" | b"NNB" | b"NMV" | b"NM8" | b"NEV" | b"NPP" | b"NUB" | b"NPD"
        | b"NRZ" | b"NR7" | b"NEP" | b"NYS" => Some(SaveType::Eeprom16k),
        b"NTE" | b"NVB" | b"NB5" | b"CFZ" | b"NFZ" | b"NSI" | b"NG6" | b"NGP" | b"NYW" | b"NHY"
        | b"NIB" | b"NPS" | b"NPA" | b"NP4" | b"NJ5" | b"NP6" | b"NPE" | b"NJG" | b"CZL"
        | b"NZL" | b"NKG" | b"NMF" | b"NRI" | b"NUT" | b"NUM" | b"NOB" | b"CPS" | b"NPM"
        | b"NRE" | b"NAL" | b"NT3" | b"NS4" | b"NA2" | b"NVP" | b"NWL" | b"NW2" | b"NWX" => {
            Some(SaveType::Sram)
        }
        b"CDZ" => Some(SaveType::SramBanked),
        b"NCC" | b"NDA" | b"NAF" | b"NJF" | b"NKJ" | b"NZS" | b"NM6" | b"NCK" | b"NMQ" | b"NPN"
        | b"NPF" | b"NPO" | b"CP2" | b"NP3" | b"NRH" | b"NSQ" | b"NT9" | b"NW4" | b"NDP" => {
            Some(SaveType::Flashram)
        }
        _ => None,
    } {
        return Ok(save_type);
    }

    Ok(SaveType::None)
}
