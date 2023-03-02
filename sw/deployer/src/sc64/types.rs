use super::{link::Packet, utils::u32_from_vec, Error};
use encoding_rs::EUC_JP;
use std::fmt::Display;

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
    BootloaderSwitch(Switch),
    RomWriteEnable(Switch),
    RomShadowEnable(Switch),
    DdMode(DdMode),
    IsvAddress(u32),
    BootMode(BootMode),
    SaveType(SaveType),
    CicSeed(CicSeed),
    TvType(TvType),
    DdSdEnable(Switch),
    DdDriveType(DdDriveType),
    DdDiskState(DdDiskState),
    ButtonState(ButtonState),
    ButtonMode(ButtonMode),
    RomExtendedEnable(Switch),
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
            ConfigId::BootloaderSwitch => Self::BootloaderSwitch(config.try_into()?),
            ConfigId::RomWriteEnable => Self::RomWriteEnable(config.try_into()?),
            ConfigId::RomShadowEnable => Self::RomShadowEnable(config.try_into()?),
            ConfigId::DdMode => Self::DdMode(config.try_into()?),
            ConfigId::IsvAddress => Self::IsvAddress(config),
            ConfigId::BootMode => Self::BootMode(config.try_into()?),
            ConfigId::SaveType => Self::SaveType(config.try_into()?),
            ConfigId::CicSeed => Self::CicSeed(config.try_into()?),
            ConfigId::TvType => Self::TvType(config.try_into()?),
            ConfigId::DdSdEnable => Self::DdSdEnable(config.try_into()?),
            ConfigId::DdDriveType => Self::DdDriveType(config.try_into()?),
            ConfigId::DdDiskState => Self::DdDiskState(config.try_into()?),
            ConfigId::ButtonState => Self::ButtonState(config.try_into()?),
            ConfigId::ButtonMode => Self::ButtonMode(config.try_into()?),
            ConfigId::RomExtendedEnable => Self::RomExtendedEnable(config.try_into()?),
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

#[derive(Copy, Clone)]
pub enum Switch {
    Off,
    On,
}

impl Display for Switch {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Switch::Off => "Disabled",
            Switch::On => "Enabled",
        })
    }
}

impl TryFrom<u32> for Switch {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Off,
            _ => Self::On,
        })
    }
}

impl From<Switch> for u32 {
    fn from(value: Switch) -> Self {
        match value {
            Switch::Off => 0,
            Switch::On => 1,
        }
    }
}

impl From<bool> for Switch {
    fn from(value: bool) -> Self {
        match value {
            false => Self::Off,
            true => Self::On,
        }
    }
}

impl From<Switch> for bool {
    fn from(value: Switch) -> Self {
        match value {
            Switch::Off => false,
            Switch::On => true,
        }
    }
}

pub enum DdMode {
    None,
    Regs,
    DdIpl,
    Full,
}

impl Display for DdMode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            DdMode::None => "Disabled",
            DdMode::Regs => "Only registers",
            DdMode::DdIpl => "Only 64DD IPL",
            DdMode::Full => "Registers + 64DD IPL",
        })
    }
}

impl TryFrom<u32> for DdMode {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::None,
            1 => Self::Regs,
            2 => Self::DdIpl,
            3 => Self::Full,
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

pub enum BootMode {
    Menu,
    Rom,
    DdIpl,
    DirectRom,
    DirectDdIpl,
}

impl Display for BootMode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::Menu => "Menu",
            Self::Rom => "Bootloader -> ROM",
            Self::DdIpl => "Bootloader -> 64DD IPL",
            Self::DirectRom => "ROM (direct)",
            Self::DirectDdIpl => "64DD IPL (direct)",
        })
    }
}

impl TryFrom<u32> for BootMode {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Menu,
            1 => Self::Rom,
            2 => Self::DdIpl,
            3 => Self::DirectRom,
            4 => Self::DirectDdIpl,
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

pub enum SaveType {
    None,
    Eeprom4k,
    Eeprom16k,
    Sram,
    Flashram,
    SramBanked,
}

impl Display for SaveType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::None => "None",
            Self::Eeprom4k => "EEPROM 4k",
            Self::Eeprom16k => "EEPROM 16k",
            Self::Sram => "SRAM",
            Self::SramBanked => "SRAM banked",
            Self::Flashram => "FlashRAM",
        })
    }
}

impl TryFrom<u32> for SaveType {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::None,
            1 => Self::Eeprom4k,
            2 => Self::Eeprom16k,
            3 => Self::Sram,
            4 => Self::Flashram,
            5 => Self::SramBanked,
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

pub enum CicSeed {
    Seed(u8),
    Auto,
}

impl Display for CicSeed {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if let CicSeed::Seed(seed) = self {
            f.write_fmt(format_args!("0x{seed:02X}"))
        } else {
            f.write_str("Auto")
        }
    }
}

impl TryFrom<u32> for CicSeed {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(if value <= 0xFF {
            Self::Seed(value as u8)
        } else if value == 0xFFFF {
            Self::Auto
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

pub enum TvType {
    PAL,
    NTSC,
    MPAL,
    Auto,
}

impl Display for TvType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::PAL => "PAL",
            Self::NTSC => "NTSC",
            Self::MPAL => "MPAL",
            Self::Auto => "Auto",
        })
    }
}

impl TryFrom<u32> for TvType {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::PAL,
            1 => Self::NTSC,
            2 => Self::MPAL,
            3 => Self::Auto,
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

pub enum DdDriveType {
    Retail,
    Development,
}

impl Display for DdDriveType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            DdDriveType::Retail => "Retail",
            DdDriveType::Development => "Development",
        })
    }
}

impl TryFrom<u32> for DdDriveType {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Retail,
            1 => Self::Development,
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

pub enum DdDiskState {
    Ejected,
    Inserted,
    Changed,
}

impl Display for DdDiskState {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            DdDiskState::Ejected => "Ejected",
            DdDiskState::Inserted => "Inserted",
            DdDiskState::Changed => "Changed",
        })
    }
}

impl TryFrom<u32> for DdDiskState {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Ejected,
            1 => Self::Inserted,
            2 => Self::Changed,
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

pub enum ButtonState {
    NotPressed,
    Pressed,
}

impl Display for ButtonState {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            ButtonState::NotPressed => "Not pressed",
            ButtonState::Pressed => "Pressed",
        })
    }
}

impl TryFrom<u32> for ButtonState {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::NotPressed,
            _ => Self::Pressed,
        })
    }
}

impl From<ButtonState> for u32 {
    fn from(value: ButtonState) -> Self {
        match value {
            ButtonState::NotPressed => 0,
            ButtonState::Pressed => 1,
        }
    }
}

impl From<bool> for ButtonState {
    fn from(value: bool) -> Self {
        match value {
            false => Self::NotPressed,
            true => Self::Pressed,
        }
    }
}

impl From<ButtonState> for bool {
    fn from(value: ButtonState) -> Self {
        match value {
            ButtonState::NotPressed => false,
            ButtonState::Pressed => true,
        }
    }
}

pub enum ButtonMode {
    None,
    N64Irq,
    UsbPacket,
    DdDiskSwap,
}

impl Display for ButtonMode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            ButtonMode::None => "User",
            ButtonMode::N64Irq => "N64 IRQ trigger",
            ButtonMode::UsbPacket => "Send USB packet",
            ButtonMode::DdDiskSwap => "Swap 64DD disk",
        })
    }
}

impl TryFrom<u32> for ButtonMode {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::None,
            1 => Self::N64Irq,
            2 => Self::UsbPacket,
            3 => Self::DdDiskSwap,
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
    LedEnable(Switch),
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
            SettingId::LedEnable => Self::LedEnable(setting.try_into()?),
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

pub enum FirmwareStatus {
    Ok,
    ErrToken,
    ErrChecksum,
    ErrSize,
    ErrUnknownChunk,
    ErrRead,
}

impl Display for FirmwareStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            FirmwareStatus::Ok => "OK",
            FirmwareStatus::ErrToken => "Invalid firmware header",
            FirmwareStatus::ErrChecksum => "Invalid chunk checksum",
            FirmwareStatus::ErrSize => "Invalid firmware size",
            FirmwareStatus::ErrUnknownChunk => "Unknown chunk in firmware",
            FirmwareStatus::ErrRead => "Firmware read error",
        })
    }
}

impl TryFrom<u32> for FirmwareStatus {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Ok,
            1 => Self::ErrToken,
            2 => Self::ErrChecksum,
            3 => Self::ErrSize,
            4 => Self::ErrUnknownChunk,
            5 => Self::ErrRead,
            _ => return Err(Error::new("Unknown firmware status code")),
        })
    }
}

pub enum UpdateStatus {
    MCU,
    FPGA,
    Bootloader,
    Done,
    Err,
}

impl Display for UpdateStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            UpdateStatus::MCU => "Microcontroller",
            UpdateStatus::FPGA => "FPGA",
            UpdateStatus::Bootloader => "Bootloader",
            UpdateStatus::Done => "Done",
            UpdateStatus::Err => "Error",
        })
    }
}

impl TryFrom<u32> for UpdateStatus {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            1 => Self::MCU,
            2 => Self::FPGA,
            3 => Self::Bootloader,
            0x80 => Self::Done,
            0xFF => Self::Err,
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
        // Note: remove 'allow(irrefutable_let_patterns)' below when more settings are added
        #[allow(irrefutable_let_patterns)]
        if let Setting::$setting(value) = $sc64.command_setting_get(SettingId::$setting)? {
            Ok(value)
        } else {
            Err(Error::new("Unexpected setting type"))
        }
    }};
}

pub(crate) use get_config;
pub(crate) use get_setting;
