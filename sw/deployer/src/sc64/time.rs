use super::Error;
use chrono::{Datelike, NaiveDateTime, Timelike};

pub fn u8_from_bcd(value: u8) -> u8 {
    (((value & 0xF0) >> 4) * 10) + (value & 0x0F)
}

pub fn bcd_from_u8(value: u8) -> u8 {
    (((value / 10) & 0x0F) << 4) | ((value % 10) & 0x0F)
}

pub fn convert_to_datetime(data: &[u8; 8]) -> Result<NaiveDateTime, Error> {
    let second = u8_from_bcd(data[3]);
    let minute = u8_from_bcd(data[2]);
    let hour = u8_from_bcd(data[1]);
    let day = u8_from_bcd(data[7]);
    let month = u8_from_bcd(data[6]);
    let year = 1900u32 + (data[4] as u32 * 100) + u8_from_bcd(data[5]) as u32;
    NaiveDateTime::parse_from_str(
        &format!("{year:4}-{month:02}-{day:02}T{hour:02}:{minute:02}:{second:02}"),
        "%Y-%m-%dT%H:%M:%S",
    )
    .map_err(|_| Error::new("Couldn't convert from bytes to NaiveDateTime"))
}

pub fn convert_from_datetime(datetime: NaiveDateTime) -> [u32; 2] {
    let second = bcd_from_u8(datetime.second() as u8);
    let minute = bcd_from_u8(datetime.minute() as u8);
    let hour = bcd_from_u8(datetime.hour() as u8);
    let weekday = bcd_from_u8((datetime.weekday() as u8) + 1);
    let day = bcd_from_u8(datetime.day() as u8);
    let month = bcd_from_u8(datetime.month() as u8);
    let year = bcd_from_u8((datetime.year() % 100) as u8);
    let century = ((datetime.year() - 1900) / 100) as u8;
    [
        u32::from_be_bytes([weekday, hour, minute, second]),
        u32::from_be_bytes([century, year, month, day]),
    ]
}
