use super::{link::AsynchronousPacket, Error};
use std::fmt::Display;

#[derive(Clone, Copy)]
pub enum ConfigId {
    BootloaderSwitch,
    RomWriteEnable,
    RomShadowEnable,
    DdMode,
    ISViewer,
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
    ISViewer(ISViewer),
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
            ConfigId::ISViewer => 4,
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
            ConfigId::ISViewer => Self::ISViewer(config.try_into()?),
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
            Config::ISViewer(val) => [ConfigId::ISViewer.into(), val.into()],
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

pub enum ISViewer {
    Disabled,
    Enabled(u32),
}

impl Display for ISViewer {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Disabled => f.write_str("Not listening"),
            Self::Enabled(offset) => {
                f.write_fmt(format_args!("Listening at 0x{:08X}", 0x1000_0000 + offset))
            }
        }
    }
}

impl TryFrom<u32> for ISViewer {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Disabled,
            offset => Self::Enabled(offset),
        })
    }
}

impl From<ISViewer> for u32 {
    fn from(value: ISViewer) -> Self {
        match value {
            ISViewer::Disabled => 0,
            ISViewer::Enabled(offset) => offset,
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
            Self::Menu => "Bootloader -> Menu from SD card",
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
    Sram1m,
}

impl Display for SaveType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::None => "None",
            Self::Eeprom4k => "EEPROM 4k",
            Self::Eeprom16k => "EEPROM 16k",
            Self::Sram => "SRAM 256k",
            Self::SramBanked => "SRAM 768k",
            Self::Flashram => "FlashRAM 1M",
            Self::Sram1m => "SRAM 1M",
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
            6 => Self::Sram1m,
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
            SaveType::Sram1m => 6,
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
    Passthrough,
}

impl Display for TvType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::PAL => "PAL",
            Self::NTSC => "NTSC",
            Self::MPAL => "MPAL",
            Self::Passthrough => "Passthrough",
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
            3 => Self::Passthrough,
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
            TvType::Passthrough => 3,
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
    AuxData(u32),
    Button,
    DataFlushed,
    DebugData(DebugPacket),
    DiskRequest(DiskPacket),
    IsViewer64(Vec<u8>),
    SaveWriteback(SaveWriteback),
    UpdateStatus(UpdateStatus),
}

impl TryFrom<AsynchronousPacket> for DataPacket {
    type Error = Error;
    fn try_from(value: AsynchronousPacket) -> Result<Self, Self::Error> {
        Ok(match value.id {
            b'X' => Self::AuxData(u32::from_be_bytes(value.data[0..4].try_into().unwrap())),
            b'B' => Self::Button,
            b'G' => Self::DataFlushed,
            b'U' => Self::DebugData(value.data.try_into()?),
            b'D' => Self::DiskRequest(value.data.try_into()?),
            b'I' => Self::IsViewer64(value.data),
            b'S' => Self::SaveWriteback(value.data.try_into()?),
            b'F' => {
                if value.data.len() != 4 {
                    return Err(Error::new(
                        "Incorrect data length for update status data packet",
                    ));
                }
                Self::UpdateStatus(
                    u32::from_be_bytes(value.data[0..4].try_into().unwrap()).try_into()?,
                )
            }
            _ => return Err(Error::new("Unknown data packet code")),
        })
    }
}

pub enum AuxMessage {
    IOHalt,
    Reboot,
}

impl From<AuxMessage> for u32 {
    fn from(value: AuxMessage) -> Self {
        match value {
            AuxMessage::IOHalt => 0xFF000001,
            AuxMessage::Reboot => 0xFF000002,
        }
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
        let header = u32::from_be_bytes(value[0..4].try_into().unwrap());
        let datatype = ((header >> 24) & 0xFF) as u8;
        let length = (header & 0x00FFFFFF) as usize;
        let data = value[4..].to_vec();
        if data.len() != length {
            return Err(Error::new("Debug packet length did not match"));
        }
        Ok(Self { datatype, data })
    }
}

pub enum DiskPacketKind {
    Read,
    Write,
}

pub struct DiskPacket {
    pub kind: DiskPacketKind,
    pub info: DiskBlock,
}

impl TryFrom<Vec<u8>> for DiskPacket {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() < 12 {
            return Err(Error::new("Couldn't extract block info from disk packet"));
        }
        let command = u32::from_be_bytes(value[0..4].try_into().unwrap());
        let address = u32::from_be_bytes(value[4..8].try_into().unwrap());
        let track_head_block = u32::from_be_bytes(value[8..12].try_into().unwrap());
        let disk_block = DiskBlock {
            address,
            track: (track_head_block >> 2) & 0xFFF,
            head: (track_head_block >> 1) & 0x01,
            block: track_head_block & 0x01,
            data: value[12..].to_vec(),
        };
        Ok(match command {
            1 => DiskPacket {
                kind: DiskPacketKind::Read,
                info: disk_block,
            },
            2 => DiskPacket {
                kind: DiskPacketKind::Write,
                info: disk_block,
            },
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

pub struct SaveWriteback {
    pub save: SaveType,
    pub data: Vec<u8>,
}

impl TryFrom<Vec<u8>> for SaveWriteback {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() < 4 {
            return Err(Error::new(
                "Couldn't extract save info from save writeback packet",
            ));
        }
        let save: SaveType = u32::from_be_bytes(value[0..4].try_into().unwrap()).try_into()?;
        let data = value[4..].to_vec();
        Ok(SaveWriteback { save, data })
    }
}

pub enum FirmwareStatus {
    Ok,
    ErrToken,
    ErrChecksum,
    ErrSize,
    ErrUnknownChunk,
    ErrRead,
    ErrAddress,
}

impl Display for FirmwareStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            FirmwareStatus::Ok => "OK",
            FirmwareStatus::ErrToken => "Invalid firmware header",
            FirmwareStatus::ErrChecksum => "Invalid chunk checksum",
            FirmwareStatus::ErrSize => "Invalid chunk size",
            FirmwareStatus::ErrUnknownChunk => "Unknown chunk in firmware",
            FirmwareStatus::ErrRead => "Firmware read error",
            FirmwareStatus::ErrAddress => "Invalid address or length provided",
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
            6 => Self::ErrAddress,
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

pub enum CicStep {
    Unavailable,
    PowerOff,
    ConfigLoad,
    Id,
    Seed,
    Checksum,
    InitRam,
    Command,
    Compare,
    X105,
    ResetButton,
    DieDisabled,
    Die64DD,
    DieInvalidRegion,
    DieCommand,
    Unknown,
}

impl Display for CicStep {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::Unavailable => "Unavailable",
            Self::PowerOff => "Power off",
            Self::ConfigLoad => "Load config",
            Self::Id => "ID",
            Self::Seed => "Seed",
            Self::Checksum => "Checksum",
            Self::InitRam => "RAM init",
            Self::Command => "Command wait",
            Self::Compare => "Compare algorithm",
            Self::X105 => "X105 algorithm",
            Self::ResetButton => "Reset button pressed",
            Self::DieDisabled => "Die (disabled)",
            Self::Die64DD => "Die (found another CIC in 64DD mode)",
            Self::DieInvalidRegion => "Die (invalid region)",
            Self::DieCommand => "Die (triggered by command)",
            Self::Unknown => "Unknown",
        })
    }
}

impl TryFrom<u8> for CicStep {
    type Error = Error;
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        Ok(match value {
            0 => Self::Unavailable,
            1 => Self::PowerOff,
            2 => Self::ConfigLoad,
            3 => Self::Id,
            4 => Self::Seed,
            5 => Self::Checksum,
            6 => Self::InitRam,
            7 => Self::Command,
            8 => Self::Compare,
            9 => Self::X105,
            10 => Self::ResetButton,
            11 => Self::DieDisabled,
            12 => Self::Die64DD,
            13 => Self::DieInvalidRegion,
            14 => Self::DieCommand,
            _ => Self::Unknown,
        })
    }
}

pub struct PiFifoFlags {
    pub read_fifo_wait: bool,
    pub read_fifo_failure: bool,
    pub write_fifo_wait: bool,
    pub write_fifo_failure: bool,
}

impl Display for PiFifoFlags {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mapping = vec![
            (self.read_fifo_wait, "Read wait"),
            (self.read_fifo_failure, "Read failure"),
            (self.write_fifo_wait, "Write wait"),
            (self.write_fifo_failure, "Write failure"),
        ];
        let filtered: Vec<&str> = mapping.into_iter().filter(|x| x.0).map(|x| x.1).collect();
        if filtered.len() > 0 {
            f.write_str(filtered.join(", ").as_str())?;
        } else {
            f.write_str("None")?;
        }
        Ok(())
    }
}

impl TryFrom<u8> for PiFifoFlags {
    type Error = Error;
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        Ok(PiFifoFlags {
            read_fifo_wait: (value & (1 << 0)) != 0,
            read_fifo_failure: (value & (1 << 1)) != 0,
            write_fifo_wait: (value & (1 << 2)) != 0,
            write_fifo_failure: (value & (1 << 3)) != 0,
        })
    }
}

pub struct FpgaDebugData {
    pub last_pi_address: u32,
    pub pi_fifo_flags: PiFifoFlags,
    pub cic_step: CicStep,
}

impl TryFrom<Vec<u8>> for FpgaDebugData {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() != 8 {
            return Err(Error::new("Invalid data length for FPGA debug data"));
        }
        Ok(FpgaDebugData {
            last_pi_address: u32::from_be_bytes(value[0..4].try_into().unwrap()),
            pi_fifo_flags: (value[7] & 0x0F).try_into().unwrap(),
            cic_step: ((value[7] >> 4) & 0x0F).try_into().unwrap(),
        })
    }
}

pub struct DiagnosticDataV0 {
    pub cic: u32,
    pub rtc: u32,
    pub led: u32,
    pub gvr: u32,
}

pub struct DiagnosticDataV1 {
    pub voltage: f32,
    pub temperature: f32,
}

pub enum DiagnosticData {
    V0(DiagnosticDataV0),
    V1(DiagnosticDataV1),
    Unknown,
}

impl TryFrom<Vec<u8>> for DiagnosticData {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() < 4 {
            return Err(Error::new("Invalid data length for diagnostic data"));
        }
        let raw_version = u32::from_be_bytes(value[0..4].try_into().unwrap());
        let unversioned = raw_version & (1 << 31) == 0;
        let version = raw_version & !(1 << 31);

        if unversioned {
            if value.len() != 16 {
                return Err(Error::new("Invalid data length for V0 diagnostic data"));
            }
            return Ok(DiagnosticData::V0(DiagnosticDataV0 {
                cic: u32::from_be_bytes(value[0..4].try_into().unwrap()),
                rtc: u32::from_be_bytes(value[4..8].try_into().unwrap()),
                led: u32::from_be_bytes(value[8..12].try_into().unwrap()),
                gvr: u32::from_be_bytes(value[12..16].try_into().unwrap()),
            }));
        }

        match version {
            1 => {
                if value.len() != 16 {
                    return Err(Error::new("Invalid data length for V1 diagnostic data"));
                }
                let raw_voltage = u32::from_be_bytes(value[4..8].try_into().unwrap()) as f32;
                let raw_temperature = u32::from_be_bytes(value[8..12].try_into().unwrap()) as f32;
                Ok(DiagnosticData::V1(DiagnosticDataV1 {
                    voltage: raw_voltage / 1000.0,
                    temperature: raw_temperature / 10.0,
                }))
            }
            _ => Ok(DiagnosticData::Unknown),
        }
    }
}

impl Display for DiagnosticData {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DiagnosticData::V0(d) => {
                if d.cic > 0 {
                    f.write_fmt(format_args!("CIC: {}, ", d.cic))?;
                }
                f.write_fmt(format_args!(
                    "RTC: {}, LED: {}, GVR: {}",
                    d.rtc, d.led, d.gvr
                ))
            }
            DiagnosticData::V1(d) => f.write_fmt(format_args!(
                "{:.03} V / {:.01} °C",
                d.voltage, d.temperature
            )),
            DiagnosticData::Unknown => f.write_str("Unknown"),
        }
    }
}

pub enum SpeedTestDirection {
    Read,
    Write,
}

pub enum MemoryTestPattern {
    OwnAddress(bool),
    AllZeros,
    AllOnes,
    Random,
    Custom(u32),
}

pub struct MemoryTestPatternResult {
    pub first_error: Option<(usize, (u32, u32))>,
    pub all_errors: Vec<(usize, (u32, u32))>,
}

impl Display for MemoryTestPattern {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MemoryTestPattern::OwnAddress(inverted) => f.write_fmt(format_args!(
                "Own address{}",
                if *inverted { "~" } else { "" }
            )),
            MemoryTestPattern::AllZeros => f.write_str("All zeros"),
            MemoryTestPattern::AllOnes => f.write_str("All ones"),
            MemoryTestPattern::Random => f.write_str("Random"),
            MemoryTestPattern::Custom(pattern) => {
                f.write_fmt(format_args!("Pattern 0x{pattern:08X}"))
            }
        }
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
        // NOTE: remove 'allow(irrefutable_let_patterns)' below when more settings are added
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
