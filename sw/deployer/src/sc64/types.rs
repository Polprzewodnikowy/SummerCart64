use super::{link::Packet, utils::u32_from_vec, Error};
use encoding_rs::EUC_JP;

#[derive(Clone, Copy)]
pub enum ConfigId {
    BootloaderSwitch,
    RomWriteEnable,
    RomShadowEnable,
    DdMode,
    IsvAddress,
    BootMode,
    SaveType,
    CicSeed,
    TvType,
    DdSdEnable,
    DdDriveType,
    DdDiskState,
    ButtonState,
    ButtonMode,
    RomExtendedEnable,
}

pub enum Config {
    BootloaderSwitch(bool),
    RomWriteEnable(bool),
    RomShadowEnable(bool),
    DdMode(DdMode),
    IsvAddress(u32),
    BootMode(BootMode),
    SaveType(SaveType),
    CicSeed(CicSeed),
    TvType(TvType),
    DdSdEnable(bool),
    DdDriveType(DdDriveType),
    DdDiskState(DdDiskState),
    ButtonState(bool),
    ButtonMode(ButtonMode),
    RomExtendedEnable(bool),
}

impl From<ConfigId> for u32 {
    fn from(value: ConfigId) -> Self {
        match value {
            ConfigId::BootloaderSwitch => 0,
            ConfigId::RomWriteEnable => 1,
            ConfigId::RomShadowEnable => 2,
            ConfigId::DdMode => 3,
            ConfigId::IsvAddress => 4,
            ConfigId::BootMode => 5,
            ConfigId::SaveType => 6,
            ConfigId::CicSeed => 7,
            ConfigId::TvType => 8,
            ConfigId::DdSdEnable => 9,
            ConfigId::DdDriveType => 10,
            ConfigId::DdDiskState => 11,
            ConfigId::ButtonState => 12,
            ConfigId::ButtonMode => 13,
            ConfigId::RomExtendedEnable => 14,
        }
    }
}

impl TryFrom<(ConfigId, u32)> for Config {
    type Error = Error;
    fn try_from(value: (ConfigId, u32)) -> Result<Self, Self::Error> {
        let (id, config) = value;
        Ok(match id {
            ConfigId::BootloaderSwitch => Config::BootloaderSwitch(config != 0),
            ConfigId::RomWriteEnable => Config::RomWriteEnable(config != 0),
            ConfigId::RomShadowEnable => Config::RomShadowEnable(config != 0),
            ConfigId::DdMode => Config::DdMode(config.try_into()?),
            ConfigId::IsvAddress => Config::IsvAddress(config),
            ConfigId::BootMode => Config::BootMode(config.try_into()?),
            ConfigId::SaveType => Config::SaveType(config.try_into()?),
            ConfigId::CicSeed => Config::CicSeed(config.try_into()?),
            ConfigId::TvType => Config::TvType(config.try_into()?),
            ConfigId::DdSdEnable => Config::DdSdEnable(config != 0),
            ConfigId::DdDriveType => Config::DdDriveType(config.try_into()?),
            ConfigId::DdDiskState => Config::DdDiskState(config.try_into()?),
            ConfigId::ButtonState => Config::ButtonState(config != 0),
            ConfigId::ButtonMode => Config::ButtonMode(config.try_into()?),
            ConfigId::RomExtendedEnable => Config::RomExtendedEnable(config != 0),
        })
    }
}

impl From<Config> for [u32; 2] {
    fn from(value: Config) -> Self {
        match value {
            Config::BootloaderSwitch(val) => [ConfigId::BootloaderSwitch.into(), val.into()],
            Config::RomWriteEnable(val) => [ConfigId::RomWriteEnable.into(), val.into()],
            Config::RomShadowEnable(val) => [ConfigId::RomShadowEnable.into(), val.into()],
            Config::DdMode(val) => [ConfigId::DdMode.into(), val.into()],
            Config::IsvAddress(val) => [ConfigId::IsvAddress.into(), val.into()],
            Config::BootMode(val) => [ConfigId::BootMode.into(), val.into()],
            Config::SaveType(val) => [ConfigId::SaveType.into(), val.into()],
            Config::CicSeed(val) => [ConfigId::CicSeed.into(), val.into()],
            Config::TvType(val) => [ConfigId::TvType.into(), val.into()],
            Config::DdSdEnable(val) => [ConfigId::DdSdEnable.into(), val.into()],
            Config::DdDriveType(val) => [ConfigId::DdDriveType.into(), val.into()],
            Config::DdDiskState(val) => [ConfigId::DdDiskState.into(), val.into()],
            Config::ButtonState(val) => [ConfigId::ButtonState.into(), val.into()],
            Config::ButtonMode(val) => [ConfigId::ButtonMode.into(), val.into()],
            Config::RomExtendedEnable(val) => [ConfigId::RomExtendedEnable.into(), val.into()],
        }
    }
}

#[derive(Debug)]
pub enum DdMode {
    None,
    Regs,
    DdIpl,
    Full,
}

impl TryFrom<u32> for DdMode {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => DdMode::None,
            1 => DdMode::Regs,
            2 => DdMode::DdIpl,
            3 => DdMode::Full,
            _ => return Err(Error::new("Unknown 64DD mode code")),
        })
    }
}

impl From<DdMode> for u32 {
    fn from(value: DdMode) -> Self {
        match value {
            DdMode::None => 0,
            DdMode::Regs => 1,
            DdMode::DdIpl => 2,
            DdMode::Full => 3,
        }
    }
}

#[derive(Copy, Clone, Debug)]
pub enum BootMode {
    Menu,
    Rom,
    DdIpl,
    DirectRom,
    DirectDdIpl,
}

impl TryFrom<u32> for BootMode {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => BootMode::Menu,
            1 => BootMode::Rom,
            2 => BootMode::DdIpl,
            3 => BootMode::DirectRom,
            4 => BootMode::DirectDdIpl,
            _ => return Err(Error::new("Unknown boot mode code")),
        })
    }
}

impl From<BootMode> for u32 {
    fn from(value: BootMode) -> Self {
        match value {
            BootMode::Menu => 0,
            BootMode::Rom => 1,
            BootMode::DdIpl => 2,
            BootMode::DirectRom => 3,
            BootMode::DirectDdIpl => 4,
        }
    }
}

#[derive(Debug)]
pub enum SaveType {
    None,
    Eeprom4k,
    Eeprom16k,
    Sram,
    Flashram,
    SramBanked,
}

impl TryFrom<u32> for SaveType {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => SaveType::None,
            1 => SaveType::Eeprom4k,
            2 => SaveType::Eeprom16k,
            3 => SaveType::Sram,
            4 => SaveType::Flashram,
            5 => SaveType::SramBanked,
            _ => return Err(Error::new("Unknown save type code")),
        })
    }
}

impl From<SaveType> for u32 {
    fn from(value: SaveType) -> Self {
        match value {
            SaveType::None => 0,
            SaveType::Eeprom4k => 1,
            SaveType::Eeprom16k => 2,
            SaveType::Sram => 3,
            SaveType::Flashram => 4,
            SaveType::SramBanked => 5,
        }
    }
}

#[derive(Debug)]
pub enum CicSeed {
    Seed(u8),
    Auto,
}

impl TryFrom<u32> for CicSeed {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(if value <= 0xFF {
            CicSeed::Seed(value as u8)
        } else if value == 0xFFFF {
            CicSeed::Auto
        } else {
            return Err(Error::new("Unknown CIC seed code"));
        })
    }
}

impl From<CicSeed> for u32 {
    fn from(value: CicSeed) -> Self {
        match value {
            CicSeed::Seed(seed) => seed.into(),
            CicSeed::Auto => 0xFFFF,
        }
    }
}

#[derive(Debug)]
pub enum TvType {
    PAL,
    NTSC,
    MPAL,
    Auto,
}

impl TryFrom<u32> for TvType {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => TvType::PAL,
            1 => TvType::NTSC,
            2 => TvType::MPAL,
            3 => TvType::Auto,
            _ => return Err(Error::new("Unknown TV type code")),
        })
    }
}

impl From<TvType> for u32 {
    fn from(value: TvType) -> Self {
        match value {
            TvType::PAL => 0,
            TvType::NTSC => 1,
            TvType::MPAL => 2,
            TvType::Auto => 3,
        }
    }
}

#[derive(Debug)]
pub enum DdDriveType {
    Retail,
    Development,
}

impl TryFrom<u32> for DdDriveType {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => DdDriveType::Retail,
            1 => DdDriveType::Development,
            _ => return Err(Error::new("Unknown 64DD drive type code")),
        })
    }
}

impl From<DdDriveType> for u32 {
    fn from(value: DdDriveType) -> Self {
        match value {
            DdDriveType::Retail => 0,
            DdDriveType::Development => 1,
        }
    }
}

#[derive(Debug)]
pub enum DdDiskState {
    Ejected,
    Inserted,
    Changed,
}

impl TryFrom<u32> for DdDiskState {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => DdDiskState::Ejected,
            1 => DdDiskState::Inserted,
            2 => DdDiskState::Changed,
            _ => return Err(Error::new("Unknown 64DD disk state code")),
        })
    }
}

impl From<DdDiskState> for u32 {
    fn from(value: DdDiskState) -> Self {
        match value {
            DdDiskState::Ejected => 0,
            DdDiskState::Inserted => 1,
            DdDiskState::Changed => 2,
        }
    }
}

#[derive(Debug)]
pub enum ButtonMode {
    None,
    N64Irq,
    UsbPacket,
    DdDiskSwap,
}

impl TryFrom<u32> for ButtonMode {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => ButtonMode::None,
            1 => ButtonMode::N64Irq,
            2 => ButtonMode::UsbPacket,
            3 => ButtonMode::DdDiskSwap,
            _ => return Err(Error::new("Unknown button mode code")),
        })
    }
}

impl From<ButtonMode> for u32 {
    fn from(value: ButtonMode) -> Self {
        match value {
            ButtonMode::None => 0,
            ButtonMode::N64Irq => 1,
            ButtonMode::UsbPacket => 2,
            ButtonMode::DdDiskSwap => 3,
        }
    }
}

#[derive(Clone, Copy)]
pub enum SettingId {
    LedEnable,
}

pub enum Setting {
    LedEnable(bool),
}

impl From<SettingId> for u32 {
    fn from(value: SettingId) -> Self {
        match value {
            SettingId::LedEnable => 0,
        }
    }
}

impl TryFrom<(SettingId, u32)> for Setting {
    type Error = Error;
    fn try_from(value: (SettingId, u32)) -> Result<Self, Self::Error> {
        let (id, setting) = value;
        Ok(match id {
            SettingId::LedEnable => Setting::LedEnable(setting != 0),
        })
    }
}

impl From<Setting> for [u32; 2] {
    fn from(value: Setting) -> Self {
        match value {
            Setting::LedEnable(val) => [SettingId::LedEnable.into(), val.into()],
        }
    }
}

pub enum DataPacket {
    Button,
    Debug(DebugPacket),
    Disk(DiskPacket),
    IsViewer(String),
    UpdateStatus(UpdateStatus),
}

impl TryFrom<Packet> for DataPacket {
    type Error = Error;
    fn try_from(value: Packet) -> Result<Self, Self::Error> {
        Ok(match value.id {
            b'B' => Self::Button,
            b'U' => Self::Debug(value.data.try_into()?),
            b'D' => Self::Disk(value.data.try_into()?),
            b'I' => Self::IsViewer(EUC_JP.decode(&value.data).0.into()),
            b'F' => Self::UpdateStatus(u32_from_vec(&value.data[0..4])?.try_into()?),
            _ => return Err(Error::new("Unknown data packet code")),
        })
    }
}

pub struct DebugPacket {
    pub datatype: u8,
    pub data: Vec<u8>,
}

impl TryFrom<Vec<u8>> for DebugPacket {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() < 4 {
            return Err(Error::new("Couldn't extract header from debug packet"));
        }
        let header = u32_from_vec(&value[0..4])?;
        let datatype = ((header >> 24) & 0xFF) as u8;
        let length = (header & 0x00FFFFFF) as usize;
        let data = value[4..].to_vec();
        if data.len() != length {
            return Err(Error::new("Debug packet length did not match"));
        }
        Ok(Self { datatype, data })
    }
}

pub enum DiskPacket {
    ReadBlock(DiskBlock),
    WriteBlock(DiskBlock),
}

impl TryFrom<Vec<u8>> for DiskPacket {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() < 12 {
            return Err(Error::new("Couldn't extract block info from disk packet"));
        }
        let command = u32_from_vec(&value[0..4])?;
        let address = u32_from_vec(&value[4..8])?;
        let track_head_block = u32_from_vec(&value[8..12])?;
        let disk_block = DiskBlock {
            address,
            track: (track_head_block >> 2) & 0xFFF,
            head: (track_head_block >> 1) & 0x01,
            block: track_head_block & 0x01,
            data: value[12..].to_vec(),
        };
        Ok(match command {
            1 => Self::ReadBlock(disk_block),
            2 => Self::WriteBlock(disk_block),
            _ => return Err(Error::new("Unknown disk packet command code")),
        })
    }
}

pub struct DiskBlock {
    pub address: u32,
    pub track: u32,
    pub head: u32,
    pub block: u32,
    pub data: Vec<u8>,
}

impl DiskBlock {
    pub fn set_data(&mut self, data: &[u8]) {
        self.data = data.to_vec();
    }
}

#[derive(Debug)]
pub enum FirmwareStatus {
    Ok,
    ErrToken,
    ErrChecksum,
    ErrSize,
    ErrUnknownChunk,
    ErrRead,
}

impl TryFrom<u32> for FirmwareStatus {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => FirmwareStatus::Ok,
            1 => FirmwareStatus::ErrToken,
            2 => FirmwareStatus::ErrChecksum,
            3 => FirmwareStatus::ErrSize,
            4 => FirmwareStatus::ErrUnknownChunk,
            5 => FirmwareStatus::ErrRead,
            _ => return Err(Error::new("Unknown firmware status code")),
        })
    }
}

#[derive(Debug)]
pub enum UpdateStatus {
    MCU,
    FPGA,
    Bootloader,
    Done,
    Err,
}

impl TryFrom<u32> for UpdateStatus {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            1 => UpdateStatus::MCU,
            2 => UpdateStatus::FPGA,
            3 => UpdateStatus::Bootloader,
            0x80 => UpdateStatus::Done,
            0xFF => UpdateStatus::Err,
            _ => return Err(Error::new("Unknown update status code")),
        })
    }
}

macro_rules! get_config {
    ($sc64:ident, $config:ident) => {{
        if let Config::$config(value) = $sc64.command_config_get(ConfigId::$config)? {
            Ok(value)
        } else {
            Err(Error::new("Unexpected config type"))
        }
    }};
}

macro_rules! get_setting {
    ($sc64:ident, $setting:ident) => {{
        #[allow(irrefutable_let_patterns)] // TODO: is there another way to ignore this warning?
        if let Setting::$setting(value) = $sc64.command_setting_get(SettingId::$setting)? {
            Ok(value)
        } else {
            Err(Error::new("Unexpected setting type"))
        }
    }};
}

pub(crate) use get_config;
pub(crate) use get_setting;
