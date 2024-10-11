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
    AuxData(AuxMessage),
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
            b'X' => Self::AuxData(value.data.try_into()?),
            b'B' => Self::Button,
            b'G' => Self::DataFlushed,
            b'U' => Self::DebugData(value.data.try_into()?),
            b'D' => Self::DiskRequest(value.data.try_into()?),
            b'I' => Self::IsViewer64(value.data),
            b'S' => Self::SaveWriteback(value.data.try_into()?),
            b'F' => Self::UpdateStatus(value.data.try_into()?),
            _ => return Err(Error::new("Unknown data packet code")),
        })
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum AuxMessage {
    Ping,
    Halt,
    Reboot,
    Other(u32),
}

impl From<AuxMessage> for u32 {
    fn from(value: AuxMessage) -> Self {
        match value {
            AuxMessage::Ping => 0xFF000000,
            AuxMessage::Halt => 0xFF000001,
            AuxMessage::Reboot => 0xFF000002,
            AuxMessage::Other(message) => message,
        }
    }
}

impl TryFrom<Vec<u8>> for AuxMessage {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() != 4 {
            return Err(Error::new("Invalid data length for AUX data packet"));
        }
        Ok(match u32::from_be_bytes(value[0..4].try_into().unwrap()) {
            0xFF000000 => AuxMessage::Ping,
            0xFF000001 => AuxMessage::Halt,
            0xFF000002 => AuxMessage::Reboot,
            message => AuxMessage::Other(message),
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

pub enum SdCardOp {
    Deinit,
    Init,
    GetStatus,
    GetInfo(u32),
    ByteSwapOn,
    ByteSwapOff,
}

impl From<SdCardOp> for [u32; 2] {
    fn from(value: SdCardOp) -> Self {
        match value {
            SdCardOp::Deinit => [0, 0],
            SdCardOp::Init => [0, 1],
            SdCardOp::GetStatus => [0, 2],
            SdCardOp::GetInfo(address) => [address, 3],
            SdCardOp::ByteSwapOn => [0, 4],
            SdCardOp::ByteSwapOff => [0, 5],
        }
    }
}

pub enum SdCardResult {
    OK,
    NoCardInSlot,
    NotInitialized,
    InvalidArgument,
    InvalidAddress,
    InvalidOperation,
    Cmd2IO,
    Cmd3IO,
    Cmd6CheckIO,
    Cmd6CheckCRC,
    Cmd6CheckTimeout,
    Cmd6CheckResponse,
    Cmd6SwitchIO,
    Cmd6SwitchCRC,
    Cmd6SwitchTimeout,
    Cmd6SwitchResponse,
    Cmd7IO,
    Cmd8IO,
    Cmd9IO,
    Cmd10IO,
    Cmd18IO,
    Cmd18CRC,
    Cmd18Timeout,
    Cmd25IO,
    Cmd25CRC,
    Cmd25Timeout,
    Acmd6IO,
    Acmd41IO,
    Acmd41OCR,
    Acmd41Timeout,
    Locked,
}

impl Display for SdCardResult {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::OK => "OK",
            Self::NoCardInSlot => "No card in slot",
            Self::NotInitialized => "Not initialized",
            Self::InvalidArgument => "Invalid argument",
            Self::InvalidAddress => "Invalid address",
            Self::InvalidOperation => "Invalid operation",
            Self::Cmd2IO => "CMD2 I/O",
            Self::Cmd3IO => "CMD3 I/O",
            Self::Cmd6CheckIO => "CMD6 I/O (check)",
            Self::Cmd6CheckCRC => "CMD6 CRC (check)",
            Self::Cmd6CheckTimeout => "CMD6 timeout (check)",
            Self::Cmd6CheckResponse => "CMD6 invalid response (check)",
            Self::Cmd6SwitchIO => "CMD6 I/O (switch)",
            Self::Cmd6SwitchCRC => "CMD6 CRC (switch)",
            Self::Cmd6SwitchTimeout => "CMD6 timeout (switch)",
            Self::Cmd6SwitchResponse => "CMD6 invalid response (switch)",
            Self::Cmd7IO => "CMD7 I/O",
            Self::Cmd8IO => "CMD8 I/O",
            Self::Cmd9IO => "CMD9 I/O",
            Self::Cmd10IO => "CMD10 I/O",
            Self::Cmd18IO => "CMD18 I/O",
            Self::Cmd18CRC => "CMD18 CRC",
            Self::Cmd18Timeout => "CMD18 timeout",
            Self::Cmd25IO => "CMD25 I/O",
            Self::Cmd25CRC => "CMD25 CRC",
            Self::Cmd25Timeout => "CMD25 timeout",
            Self::Acmd6IO => "ACMD6 I/O",
            Self::Acmd41IO => "ACMD41 I/O",
            Self::Acmd41OCR => "ACMD41 OCR",
            Self::Acmd41Timeout => "ACMD41 timeout",
            Self::Locked => "SD card is locked by the N64 side",
        })
    }
}

impl TryFrom<Vec<u8>> for SdCardResult {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() < 4 {
            return Err(Error::new(
                "Incorrect data length for SD card result data packet",
            ));
        }
        Ok(match u32::from_be_bytes(value[0..4].try_into().unwrap()) {
            0 => Self::OK,
            1 => Self::NoCardInSlot,
            2 => Self::NotInitialized,
            3 => Self::InvalidArgument,
            4 => Self::InvalidAddress,
            5 => Self::InvalidOperation,
            6 => Self::Cmd2IO,
            7 => Self::Cmd3IO,
            8 => Self::Cmd6CheckIO,
            9 => Self::Cmd6CheckCRC,
            10 => Self::Cmd6CheckTimeout,
            11 => Self::Cmd6CheckResponse,
            12 => Self::Cmd6SwitchIO,
            13 => Self::Cmd6SwitchCRC,
            14 => Self::Cmd6SwitchTimeout,
            15 => Self::Cmd6SwitchResponse,
            16 => Self::Cmd7IO,
            17 => Self::Cmd8IO,
            18 => Self::Cmd9IO,
            19 => Self::Cmd10IO,
            20 => Self::Cmd18IO,
            21 => Self::Cmd18CRC,
            22 => Self::Cmd18Timeout,
            23 => Self::Cmd25IO,
            24 => Self::Cmd25CRC,
            25 => Self::Cmd25Timeout,
            26 => Self::Acmd6IO,
            27 => Self::Acmd41IO,
            28 => Self::Acmd41OCR,
            29 => Self::Acmd41Timeout,
            30 => Self::Locked,
            _ => return Err(Error::new("Unknown SD card result code")),
        })
    }
}

pub struct SdCardStatus {
    pub byte_swap: bool,
    pub clock_mode_50mhz: bool,
    pub card_type_block: bool,
    pub card_initialized: bool,
    pub card_inserted: bool,
}

impl Display for SdCardStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.card_initialized {
            f.write_str("Initialized, ")?;
            f.write_str(if self.clock_mode_50mhz {
                "50 MHz"
            } else {
                "25 MHz"
            })?;
            if !self.card_type_block {
                f.write_str(", byte addressed")?;
            }
            if self.byte_swap {
                f.write_str(", byte swap enabled")?;
            }
        } else if self.card_inserted {
            f.write_str("Not initialized")?;
        } else {
            f.write_str("Not inserted")?;
        }
        Ok(())
    }
}

pub struct SdCardOpPacket {
    pub result: SdCardResult,
    pub status: SdCardStatus,
}

impl TryFrom<Vec<u8>> for SdCardStatus {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() != 8 {
            return Err(Error::new(
                "Incorrect data length for SD card status data packet",
            ));
        }
        let status = u32::from_be_bytes(value[4..8].try_into().unwrap());
        return Ok(Self {
            byte_swap: status & (1 << 4) != 0,
            clock_mode_50mhz: status & (1 << 3) != 0,
            card_type_block: status & (1 << 2) != 0,
            card_initialized: status & (1 << 1) != 0,
            card_inserted: status & (1 << 0) != 0,
        });
    }
}

pub struct SdCardInfo {
    pub sectors: u64,
}

impl TryFrom<Vec<u8>> for SdCardInfo {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() != 32 {
            return Err(Error::new(
                "Incorrect data length for SD card info data packet",
            ));
        }
        let csd = u128::from_be_bytes(value[0..16].try_into().unwrap());
        let csd_structure = (csd >> 126) & 0x3;
        match csd_structure {
            0 => {
                let c_size = ((csd >> 62) & 0xFFF) as u64;
                let c_size_mult = ((csd >> 47) & 0x7) as u32;
                let read_bl_len = ((csd >> 80) & 0xF) as u32;
                Ok(SdCardInfo {
                    sectors: (c_size + 1) * 2u64.pow(c_size_mult + 2) * 2u64.pow(read_bl_len) / 512,
                })
            }
            1 => {
                let c_size = ((csd >> 48) & 0x3FFFFF) as u64;
                Ok(SdCardInfo {
                    sectors: (c_size + 1) * 1024,
                })
            }
            2 => {
                let c_size = ((csd >> 48) & 0xFFFFFFF) as u64;
                Ok(SdCardInfo {
                    sectors: (c_size + 1) * 1024,
                })
            }
            _ => Err(Error::new("Unknown CSD structure value")),
        }
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

impl TryFrom<Vec<u8>> for UpdateStatus {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() != 4 {
            return Err(Error::new(
                "Incorrect data length for update status data packet",
            ));
        }
        Ok(match u32::from_be_bytes(value[0..4].try_into().unwrap()) {
            1 => Self::MCU,
            2 => Self::FPGA,
            3 => Self::Bootloader,
            0x80 => Self::Done,
            0xFF => Self::Err,
            _ => return Err(Error::new("Unknown update status code")),
        })
    }
}

pub enum PiIODirection {
    Read,
    Write,
}

impl Display for PiIODirection {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::Read => "read",
            Self::Write => "written",
        })
    }
}

pub struct PiIOAccess {
    pub address: u32,
    pub count: u32,
    pub direction: PiIODirection,
}

impl From<&[u8; 8]> for PiIOAccess {
    fn from(value: &[u8; 8]) -> Self {
        let address = u32::from_be_bytes(value[0..4].try_into().unwrap());
        let info = u32::from_be_bytes(value[4..8].try_into().unwrap());
        let count = (info >> 8) & 0x1FFFF;
        let direction = if (info & (1 << 25)) == 0 {
            PiIODirection::Read
        } else {
            PiIODirection::Write
        };
        PiIOAccess {
            address,
            count,
            direction,
        }
    }
}

impl Display for PiIOAccess {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!("0x{:08X}", self.address))?;
        if self.count > 0 {
            f.write_fmt(format_args!(" {} bytes {}", self.count * 2, self.direction))?;
        }
        Ok(())
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

impl From<&[u8; 8]> for PiFifoFlags {
    fn from(value: &[u8; 8]) -> Self {
        PiFifoFlags {
            read_fifo_wait: (value[7] & (1 << 0)) != 0,
            read_fifo_failure: (value[7] & (1 << 1)) != 0,
            write_fifo_wait: (value[7] & (1 << 2)) != 0,
            write_fifo_failure: (value[7] & (1 << 3)) != 0,
        }
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

impl From<&[u8; 8]> for CicStep {
    fn from(value: &[u8; 8]) -> Self {
        let cic_step = (value[7] >> 4) & 0x0F;
        match cic_step {
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
        }
    }
}

pub struct FpgaDebugData {
    pub pi_io_access: PiIOAccess,
    pub pi_fifo_flags: PiFifoFlags,
    pub cic_step: CicStep,
}

impl TryFrom<Vec<u8>> for FpgaDebugData {
    type Error = Error;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() != 8 {
            return Err(Error::new("Invalid data length for FPGA debug data"));
        }
        let data: &[u8; 8] = &value[0..8].try_into().unwrap();
        Ok(FpgaDebugData {
            pi_io_access: data.into(),
            pi_fifo_flags: data.into(),
            cic_step: data.into(),
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
