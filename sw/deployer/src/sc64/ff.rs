use super::{SdCardResult, SC64, SD_CARD_SECTOR_SIZE};
use chrono::{Datelike, Timelike};
#[cfg(unix)]
use fuse_mt::{
    CallbackResult, DirectoryEntry, FileAttr, FileType, FilesystemMT, RequestInfo,
    ResultEmpty, ResultEntry, ResultOpen, ResultReaddir, ResultSlice, ResultStatfs, Statfs
};

#[cfg(unix)]
use std::{
    cell::RefCell,
    collections::HashMap,
    io::{Read, Seek, SeekFrom},
    sync::{Arc, Mutex},
    time::{Duration, SystemTime}
};
#[cfg(unix)]
use log::{debug, error};
#[cfg(unix)]
use chrono::{Utc};

mod fatfs {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(non_upper_case_globals)]
    #![allow(unused)]

    include!(concat!(env!("OUT_DIR"), "/fatfs_bindings.rs"));

    pub type DSTATUS = BYTE;
    pub const DSTATUS_STA_OK: DSTATUS = 0;
    pub const DSTATUS_STA_NOINIT: DSTATUS = 1 << 0;
    pub const DSTATUS_STA_NODISK: DSTATUS = 1 << 2;
    pub const DSTATUS_STA_PROTECT: DSTATUS = 1 << 3;

    pub type DRESULT = std::os::raw::c_uint;
    pub const DRESULT_RES_OK: DRESULT = 0;
    pub const DRESULT_RES_ERROR: DRESULT = 1;
    pub const DRESULT_RES_WRPRT: DRESULT = 2;
    pub const DRESULT_RES_NOTRDY: DRESULT = 3;
    pub const DRESULT_RES_PARERR: DRESULT = 4;

    pub const CTRL_SYNC: BYTE = 0;
    pub const GET_SECTOR_COUNT: BYTE = 1;
    pub const GET_SECTOR_SIZE: BYTE = 2;
    pub const GET_BLOCK_SIZE: BYTE = 3;
    pub const CTRL_TRIM: BYTE = 4;

    pub enum Error {
        DiskErr,
        IntErr,
        NotReady,
        NoFile,
        NoPath,
        InvalidName,
        Denied,
        Exist,
        InvalidObject,
        WriteProtected,
        InvalidDrive,
        NotEnabled,
        NoFilesystem,
        MkfsAborted,
        Timeout,
        Locked,
        NotEnoughCore,
        TooManyOpenFiles,
        InvalidParameter,
        DriverInstalled,
        DriverNotInstalled,
        Unknown,
    }

    impl std::fmt::Display for Error {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            f.write_str(match self {
                Self::DiskErr => "A hard error occurred in the low level disk I/O layer",
                Self::IntErr => "Assertion failed",
                Self::NotReady => "The physical drive cannot work",
                Self::NoFile => "Could not find the file",
                Self::NoPath => "Could not find the path",
                Self::InvalidName => "The path name format is invalid",
                Self::Denied => "Access denied due to prohibited access or directory full",
                Self::Exist => "Access denied due to prohibited access",
                Self::InvalidObject => "The file/directory object is invalid",
                Self::WriteProtected => "The physical drive is write protected",
                Self::InvalidDrive => "The logical drive number is invalid",
                Self::NotEnabled => "The volume has no work area",
                Self::NoFilesystem => "There is no valid FAT volume",
                Self::MkfsAborted => "The f_mkfs() aborted due to any problem",
                Self::Timeout => "Could not get a grant to access the volume within defined period",
                Self::Locked => "The operation is rejected according to the file sharing policy",
                Self::NotEnoughCore => "LFN working buffer could not be allocated",
                Self::TooManyOpenFiles => "Number of open files > FF_FS_LOCK",
                Self::InvalidParameter => "Given parameter is invalid",
                Self::DriverInstalled => "FatFs driver is already installed",
                Self::DriverNotInstalled => "FatFs driver is not installed",
                Self::Unknown => "Unknown error",
            })
        }
    }

    impl From<FRESULT> for Error {
        fn from(value: FRESULT) -> Self {
            match value {
                FRESULT_FR_DISK_ERR => Error::DiskErr,
                FRESULT_FR_INT_ERR => Error::IntErr,
                FRESULT_FR_NOT_READY => Error::NotReady,
                FRESULT_FR_NO_FILE => Error::NoFile,
                FRESULT_FR_NO_PATH => Error::NoPath,
                FRESULT_FR_INVALID_NAME => Error::InvalidName,
                FRESULT_FR_DENIED => Error::Denied,
                FRESULT_FR_EXIST => Error::Exist,
                FRESULT_FR_INVALID_OBJECT => Error::InvalidObject,
                FRESULT_FR_WRITE_PROTECTED => Error::WriteProtected,
                FRESULT_FR_INVALID_DRIVE => Error::InvalidDrive,
                FRESULT_FR_NOT_ENABLED => Error::NotEnabled,
                FRESULT_FR_NO_FILESYSTEM => Error::NoFilesystem,
                FRESULT_FR_MKFS_ABORTED => Error::MkfsAborted,
                FRESULT_FR_TIMEOUT => Error::Timeout,
                FRESULT_FR_LOCKED => Error::Locked,
                FRESULT_FR_NOT_ENOUGH_CORE => Error::NotEnoughCore,
                FRESULT_FR_TOO_MANY_OPEN_FILES => Error::TooManyOpenFiles,
                FRESULT_FR_INVALID_PARAMETER => Error::InvalidParameter,
                _ => Error::Unknown,
            }
        }
    }

    pub fn path<P: AsRef<std::path::Path>>(path: P) -> Result<std::ffi::CString, Error> {
        match path.as_ref().to_str() {
            Some(path) => Ok(std::ffi::CString::new(path).map_err(|_| Error::InvalidParameter)?),
            None => Err(Error::InvalidParameter),
        }
    }
}

pub type Error = fatfs::Error;

static mut DRIVER: std::sync::Mutex<Option<Box<dyn FFDriver>>> = std::sync::Mutex::new(None);

fn install_driver(driver: impl FFDriver + 'static) -> Result<(), Error> {
    let mut d = unsafe { DRIVER.lock().unwrap() };
    if d.is_some() {
        return Err(Error::DriverInstalled);
    }
    d.replace(Box::new(driver));
    Ok(())
}

fn uninstall_driver() -> Result<(), Error> {
    let mut d = unsafe { DRIVER.lock().unwrap() };
    if d.is_none() {
        return Err(Error::DriverNotInstalled);
    }
    d.take().unwrap().deinit();
    Ok(())
}

#[cfg(unix)]
struct HandleMap<T> {
    mutex: Mutex<u32>,
    counter: RefCell<u64>,
    map: RefCell<HashMap<u64, Arc<Mutex<T>>>>,
}

#[cfg(unix)]
impl<T> HandleMap<T> {
    pub fn new(start_counter: u64) -> Self {
        HandleMap {
            mutex: Mutex::new(0),
            counter: RefCell::new(start_counter),
            map: RefCell::new(HashMap::new()),
        }
    }

    pub fn insert(&self, item: T) -> u64 {
        let _lock = self.mutex.lock();
        let mut counter = self.counter.borrow_mut();
        *counter += 1;
        self.map.borrow_mut().insert(*counter, Arc::new(Mutex::new(item)));
        *counter
    }

    pub fn get(&self, handle: u64) -> Result<Arc<Mutex<T>>, libc::c_int> {
        let _lock = self.mutex.lock();
        let map = self.map.borrow();
        map.get(&handle)
            .map(|item| Arc::clone(item))
            .ok_or(libc::EEXIST)
    }

    pub fn release(&self, handle: u64) {
        let _lock = self.mutex.lock();
        self.map.borrow_mut().retain(|&k, _| k != handle);
    }
}

#[cfg(unix)]
pub struct FatFsHelper {
    dirs: HandleMap<Directory>,
    files: HandleMap<File>,
    stat_cache: StatCache<Entry>,
}

#[cfg(unix)]
pub struct StatCache<T> {
    mutex: Mutex<u64>,
    cache: RefCell<HashMap<std::path::PathBuf, T>>,
}

#[cfg(unix)]
impl<T: Clone> StatCache<T> {
    pub fn new() -> Self {
        StatCache {
            mutex: Mutex::new(0),
            cache: RefCell::new(HashMap::new()),
        }
    }

    /// Inserts an entry into the cache, returning the previous entry if it existed.
    pub fn insert(&self, path: &std::path::Path, entry: T) -> Option<T> {
        let _lock = self.mutex.lock();
        let mut cache = self.cache.borrow_mut();
        cache.insert(path.to_path_buf(), entry)
    }

    /// Removes an entry from the cache, if it exists.
    pub fn remove(&self, path: &std::path::PathBuf) {
        let _lock = self.mutex.lock();
        let mut cache = self.cache.borrow_mut();
        cache.remove(path);
    }

    /// Retrieves an entry from the cache, returning None if it doesn't exist.
    pub fn get(&self, path: &std::path::Path) -> Option<T> {
        let _lock = self.mutex.lock();
        let cache = self.cache.borrow();
        cache.get(path).cloned() // Clone the entry if it exists
    }
}

#[cfg(unix)]
impl FatFsHelper {
    pub fn new() -> Self {
        FatFsHelper {
            dirs: HandleMap::new(0),
            files: HandleMap::new(u64::MAX / 2),
            stat_cache: StatCache::new(),
        }
    }
}

pub struct FatFs {
    fs: Box<fatfs::FATFS>,
    #[cfg(unix)]
    helper: FatFsHelper,
}

impl FatFs {
    pub fn new(driver: impl FFDriver + 'static) -> Result<Self, Error> {
        install_driver(driver)?;
        let mut ff = Self {
            fs: Box::new(unsafe { std::mem::zeroed() }),
            #[cfg(unix)]
            helper: FatFsHelper::new(),
        };
        ff.mount(false)?;
        Ok(ff)
    }

    fn mount(&mut self, force: bool) -> Result<(), Error> {
        let opt = if force { 1 } else { 0 };
        match unsafe { fatfs::f_mount(&mut *self.fs, fatfs::path("")?.as_ptr(), opt) } {
            fatfs::FRESULT_FR_OK => Ok(()),
            error => Err(error.into()),
        }
    }

    fn unmount(&mut self) -> Result<(), Error> {
        match unsafe { fatfs::f_mount(std::ptr::null_mut(), fatfs::path("")?.as_ptr(), 0) } {
            fatfs::FRESULT_FR_OK => Ok(()),
            error => Err(error.into()),
        }
    }

    pub fn open<P: AsRef<std::path::Path>>(&self, path: P) -> Result<File, Error> {
        File::open(
            path,
            fatfs::FA_OPEN_EXISTING | fatfs::FA_READ | fatfs::FA_WRITE,
        )
    }

    pub fn create<P: AsRef<std::path::Path>>(&mut self, path: P) -> Result<File, Error> {
        File::open(
            path,
            fatfs::FA_CREATE_ALWAYS | fatfs::FA_READ | fatfs::FA_WRITE,
        )
    }

    pub fn stat<P: AsRef<std::path::Path>>(&self, path: P) -> Result<Entry, Error> {
        let mut fno = unsafe { std::mem::zeroed() };
        match unsafe { fatfs::f_stat(fatfs::path(path)?.as_ptr(), &mut fno) } {
            fatfs::FRESULT_FR_OK => Ok(fno.into()),
            error => Err(error.into()),
        }
    }

    pub fn opendir<P: AsRef<std::path::Path>>(&self, path: P) -> Result<Directory, Error> {
        Directory::open(path)
    }

    pub fn list<P: AsRef<std::path::Path>>(&mut self, path: P) -> Result<Vec<Entry>, Error> {
        let mut dir = self.opendir(path)?;

        let mut list = vec![];

        while let Some(entry) = dir.read()? {
            list.push(entry);
        }

        list.sort_by_key(|k| (k.info, k.name.to_lowercase()));

        Ok(list)
    }

    pub fn mkdir<P: AsRef<std::path::Path>>(&mut self, path: P) -> Result<(), Error> {
        match unsafe { fatfs::f_mkdir(fatfs::path(path)?.as_ptr()) } {
            fatfs::FRESULT_FR_OK => Ok(()),
            error => Err(error.into()),
        }
    }

    pub fn delete<P: AsRef<std::path::Path>>(&mut self, path: P) -> Result<(), Error> {
        match unsafe { fatfs::f_unlink(fatfs::path(path)?.as_ptr()) } {
            fatfs::FRESULT_FR_OK => Ok(()),
            error => Err(error.into()),
        }
    }

    pub fn rename<P: AsRef<std::path::Path>>(&mut self, old: P, new: P) -> Result<(), Error> {
        match unsafe { fatfs::f_rename(fatfs::path(old)?.as_ptr(), fatfs::path(new)?.as_ptr()) } {
            fatfs::FRESULT_FR_OK => Ok(()),
            error => Err(error.into()),
        }
    }

    pub fn mkfs(&mut self) -> Result<(), Error> {
        let mut work = [0u8; 16 * 1024];
        match unsafe {
            fatfs::f_mkfs(
                fatfs::path("")?.as_ptr(),
                std::ptr::null(),
                work.as_mut_ptr().cast(),
                size_of_val(&work) as u32,
            )
        } {
            fatfs::FRESULT_FR_OK => Ok(()),
            error => Err(error.into()),
        }
    }
}


// TODO, see https://github.com/wfraser/fuse-mt/issues/29
#[cfg(unix)]
unsafe impl Send for FatFs {}
#[cfg(unix)]
unsafe impl Sync for FatFs {}
#[cfg(unix)]
const TTL: Duration = Duration::from_secs(u64::MAX); // TTL can be high because currently it's RO anyway

#[cfg(unix)]
fn stat_to_fuse(stat: &Entry) -> FileAttr {
    let time = |secs: i64, nanos: i64|
        SystemTime::UNIX_EPOCH + Duration::new(secs as u64, nanos as u32);

    FileAttr {
        size: match stat.info {
            EntryInfo::Directory => { 0 }
            EntryInfo::File { size } => { size }
        },
        blocks: match stat.info {
            EntryInfo::Directory => { 0 }
            EntryInfo::File { size } => { (size / 4096) + 1 } // https://github.com/wfraser/fuse-mt/blob/ee9d91d9003e5aa72877ca39b614ff7d84149f02/src/fusemt.rs#L49
        },
        atime: time(stat.datetime.and_utc().timestamp(), 0),
        mtime: time(stat.datetime.and_utc().timestamp(), 0),
        ctime: time(stat.datetime.and_utc().timestamp(), 0),
        crtime: SystemTime::UNIX_EPOCH,
        kind: match stat.info {
            EntryInfo::Directory => { FileType::Directory }
            EntryInfo::File { .. } => { FileType::RegularFile }
        },
        perm: 0o755, // TODO?
        nlink: match stat.info {
            EntryInfo::Directory => { 2 }
            EntryInfo::File { .. } => { 1 }
        },
        // TODO ?!?!
        uid: 501,
        gid: 20,
        rdev: 0,
        flags: 0,
    }
}


#[cfg(unix)]
impl FilesystemMT for FatFs {
    fn init(&self, _req: RequestInfo) -> ResultEmpty {
        debug!("init");
        Ok(())
    }

    fn destroy(&self) {
        debug!("destroy");
    }

    fn getattr(&self, _req: RequestInfo, path: &std::path::Path, _fh: Option<u64>) -> ResultEntry {
        debug!("getattr: {:?}", path);

        if let Some(entry) = self.helper.stat_cache.get(path) {
            return Ok((TTL, stat_to_fuse(&entry)));
        }

        debug!("getattr: {:?}, no cache", path);


        if path.as_os_str() == "/" {
            let entry = Entry {
                name: "name".to_string(),
                datetime: Utc::now().naive_utc(),
                info: EntryInfo::Directory,
            };

            self.helper.stat_cache.insert(path, entry.clone());
            return Ok((TTL, stat_to_fuse(&entry)));
        }

        match self.stat(path) {
            Ok(entry) => {
                self.helper.stat_cache.insert(path, entry.clone());
                Ok((TTL, stat_to_fuse(&entry)))
            }
            Err(e) => Err(e.into())
        }
    }


    fn open(&self, _req: RequestInfo, path: &std::path::Path, flags: u32) -> ResultOpen {
        debug!("open: {:?} flags={:#x}", path, flags);

        match FatFs::open(self, path) {
            Ok(fh) => {
                Ok((self.helper.files.insert(fh), 0))
            }
            Err(e) => {
                error!("open({:?}) failed: {}", path, libc::EEXIST);
                Err(e.into())
            }
        }
    }

    fn read(&self, _req: RequestInfo, path: &std::path::Path, fh: u64, offset: u64, size: u32, callback: impl FnOnce(ResultSlice<'_>) -> CallbackResult) -> CallbackResult {
        debug!("read attempt: {:?} {:#x} @ {:#x}", path, size, offset);

        let file_arc = match self.helper.files.get(fh) {
            Ok(dir) => { dir }
            Err(_) => { return callback(Err(libc::EEXIST)); }
        };

        let mut file = file_arc.lock().unwrap();

        if let Err(e) = file.seek(SeekFrom::Start(offset)) {
            error!("seek({:?}, {}) failed: {}", path, offset, e);
            return callback(Err(e.raw_os_error().unwrap()));
        }

        let mut data = vec![0u8; size as usize];
        match file.read(&mut data) {
            Ok(read_size) => {
                data.resize(read_size, 0);
                callback(Ok(&data))
            }
            Err(_) => {
                error!("read: {:?} {:#x} @ {:#x} failed.", path, size, offset);
                callback(Err(libc::EFAULT))
            }
        }
    }

    fn release(&self, _req: RequestInfo, path: &std::path::Path, fh: u64, _flags: u32, _lock_owner: u64, _flush: bool) -> ResultEmpty {
        debug!("release: {:?}", path);
        self.helper.files.release(fh);
        Ok(())
    }

    fn opendir(&self, _req: RequestInfo, path: &std::path::Path, _flags: u32) -> ResultOpen {
        debug!("opendir: {:?} (flags = {:#o})", path, _flags);

        match FatFs::opendir(self, path) {
            Ok(fh) => {
                Ok((self.helper.dirs.insert(fh), 0))
            }
            Err(e) => {
                let new_e = e.into();
                error!("opendir({:?}): failed {}", path, new_e);
                Err(new_e)
            }
        }
    }

    fn readdir(&self, _req: RequestInfo, path: &std::path::Path, fh: u64) -> ResultReaddir {
        debug!("readdir: {:?}", path);
        let mut entries: Vec<DirectoryEntry> = vec![];

        if fh == 0 {
            error!("readdir: missing fh");
            return Err(libc::EINVAL);
        }

        let dir_arc = match self.helper.dirs.get(fh) {
            Ok(dir) => { dir }
            Err(_) => { return Err(libc::EEXIST); }
        };

        let mut dir = dir_arc.lock().unwrap();

        loop {
            match dir.read() {
                Ok(Some(entry)) => {
                    let mut full_path = path.to_path_buf();
                    full_path.push(&entry.name);

                    if self.helper.stat_cache.get(full_path.as_path()).is_none() {
                        self.helper.stat_cache.insert(full_path.as_path(), entry.clone());
                    }

                    let name = entry.name;

                    let filetype = match entry.info {
                        EntryInfo::Directory => { FileType::Directory }
                        EntryInfo::File { .. } => { FileType::RegularFile }
                    };

                    entries.push(DirectoryEntry {
                        name: std::ffi::OsString::from(name),
                        kind: filetype,
                    })
                }
                Ok(None) => { break; }
                Err(e) => {
                    let new_val = e.into();
                    error!("readdir failed: {:?}: {}", path, new_val);
                    return Err(new_val);
                }
            }
        }

        Ok(entries)
    }

    fn releasedir(&self, _req: RequestInfo, path: &std::path::Path, fh: u64, _flags: u32) -> ResultEmpty {
        debug!("releasedir: {:?}", path);
        self.helper.dirs.release(fh);
        Ok(())
    }

    fn statfs(&self, _req: RequestInfo, path: &std::path::Path) -> ResultStatfs {
        debug!("statfs: {:?}", path);
        // TODO which values to actually put here?
        Ok(Statfs {
            blocks: 10000,
            bfree: 1000,
            bavail: 1000,
            files: 5000,
            ffree: 5000,
            bsize: 4096,
            namelen: 255,
            frsize: 4096,
        })
    }
}

#[cfg(unix)]
impl From<fatfs::Error> for libc::c_int {
    fn from(value: fatfs::Error) -> Self {
        // TODO almost all of these values are arbitrary set to EINVAL, correct mapping is needed
        match value {
            Error::DiskErr => { libc::EINVAL }
            Error::IntErr => { libc::EINVAL }
            Error::NotReady => { libc::EINVAL }
            Error::NoFile => { libc::ENOENT }
            Error::NoPath => { libc::ENOENT }
            Error::InvalidName => { libc::ENOENT }
            Error::Denied => { libc::EINVAL }
            Error::Exist => { libc::EEXIST }
            Error::InvalidObject => { libc::EFAULT }
            Error::WriteProtected => { libc::EACCES }
            Error::InvalidDrive => { libc::EINVAL }
            Error::NotEnabled => { libc::EINVAL }
            Error::NoFilesystem => { libc::EINVAL }
            Error::MkfsAborted => { libc::EINVAL }
            Error::Timeout => { libc::EINVAL }
            Error::Locked => { libc::EINVAL }
            Error::NotEnoughCore => { libc::EINVAL }
            Error::TooManyOpenFiles => { libc::EINVAL }
            Error::InvalidParameter => { libc::EINVAL }
            Error::DriverInstalled => { libc::EINVAL }
            Error::DriverNotInstalled => { libc::EINVAL }
            Error::Unknown => { libc::EINVAL }
        }
    }
}


impl Drop for FatFs {
    fn drop(&mut self) {
        self.unmount().ok();
        uninstall_driver().ok();
    }
}

pub enum IOCtl {
    Sync,
    GetSectorCount(fatfs::LBA_t),
    GetSectorSize(fatfs::WORD),
    GetBlockSize(fatfs::DWORD),
    Trim,
}

pub trait FFDriver {
    fn init(&mut self) -> fatfs::DSTATUS;
    fn deinit(&mut self);
    fn status(&mut self) -> fatfs::DSTATUS;
    fn read(&mut self, buffer: &mut [u8], sector: fatfs::LBA_t) -> fatfs::DRESULT;
    fn write(&mut self, buffer: &[u8], sector: fatfs::LBA_t) -> fatfs::DRESULT;
    fn ioctl(&mut self, ioctl: &mut IOCtl) -> fatfs::DRESULT;
}

impl FFDriver for SC64 {
    fn init(&mut self) -> fatfs::DSTATUS {
        if let Ok(SdCardResult::OK) = self.init_sd_card() {
            return fatfs::DSTATUS_STA_OK;
        }
        fatfs::DSTATUS_STA_NOINIT
    }

    fn deinit(&mut self) {
        self.deinit_sd_card().ok();
    }

    fn status(&mut self) -> fatfs::DSTATUS {
        if let Ok(status) = self.get_sd_card_status() {
            if status.card_initialized {
                return fatfs::DSTATUS_STA_OK;
            }
        }
        fatfs::DSTATUS_STA_NOINIT
    }

    fn read(&mut self, buffer: &mut [u8], sector: fatfs::LBA_t) -> fatfs::DRESULT {
        if let Ok(SdCardResult::OK) = self.read_sd_card(buffer, sector) {
            return fatfs::DRESULT_RES_OK;
        }
        fatfs::DRESULT_RES_ERROR
    }

    fn write(&mut self, buffer: &[u8], sector: fatfs::LBA_t) -> fatfs::DRESULT {
        if let Ok(SdCardResult::OK) = self.write_sd_card(buffer, sector) {
            return fatfs::DRESULT_RES_OK;
        }
        fatfs::DRESULT_RES_ERROR
    }

    fn ioctl(&mut self, ioctl: &mut IOCtl) -> fatfs::DRESULT {
        match ioctl {
            IOCtl::Sync => {}
            IOCtl::GetSectorCount(_) => {
                match self.get_sd_card_info() {
                    Ok(info) => *ioctl = IOCtl::GetSectorCount(info.sectors as fatfs::LBA_t),
                    Err(_) => return fatfs::DRESULT_RES_ERROR,
                };
            }
            IOCtl::GetSectorSize(_) => {
                *ioctl = IOCtl::GetSectorSize(SD_CARD_SECTOR_SIZE as fatfs::WORD)
            }
            IOCtl::GetBlockSize(_) => {
                *ioctl = IOCtl::GetBlockSize(1);
            }
            IOCtl::Trim => {}
        }
        fatfs::DRESULT_RES_OK
    }
}

#[no_mangle]
unsafe extern "C" fn disk_status(pdrv: fatfs::BYTE) -> fatfs::DSTATUS {
    if pdrv != 0 {
        return fatfs::DSTATUS_STA_NOINIT;
    }
    if let Some(d) = DRIVER.lock().unwrap().as_mut() {
        return d.status();
    }
    fatfs::DSTATUS_STA_NOINIT
}

#[no_mangle]
unsafe extern "C" fn disk_initialize(pdrv: fatfs::BYTE) -> fatfs::DSTATUS {
    if pdrv != 0 {
        return fatfs::DSTATUS_STA_NOINIT;
    }
    if let Some(d) = DRIVER.lock().unwrap().as_mut() {
        return d.init();
    }
    fatfs::DSTATUS_STA_NOINIT
}

#[no_mangle]
unsafe extern "C" fn disk_read(
    pdrv: fatfs::BYTE,
    buff: *mut fatfs::BYTE,
    sector: fatfs::LBA_t,
    count: fatfs::UINT,
) -> fatfs::DRESULT {
    if pdrv != 0 {
        return fatfs::DRESULT_RES_PARERR;
    }
    if let Some(d) = DRIVER.lock().unwrap().as_mut() {
        return d.read(
            &mut *std::ptr::slice_from_raw_parts_mut(buff, (count as usize) * SD_CARD_SECTOR_SIZE),
            sector,
        );
    }
    fatfs::DRESULT_RES_NOTRDY
}

#[no_mangle]
unsafe extern "C" fn disk_write(
    pdrv: fatfs::BYTE,
    buff: *mut fatfs::BYTE,
    sector: fatfs::LBA_t,
    count: fatfs::UINT,
) -> fatfs::DRESULT {
    if pdrv != 0 {
        return fatfs::DRESULT_RES_PARERR;
    }
    if let Some(d) = DRIVER.lock().unwrap().as_mut() {
        return d.write(
            &*std::ptr::slice_from_raw_parts(buff, (count as usize) * SD_CARD_SECTOR_SIZE),
            sector,
        );
    }
    fatfs::DRESULT_RES_NOTRDY
}

#[no_mangle]
unsafe extern "C" fn disk_ioctl(
    pdrv: fatfs::BYTE,
    cmd: fatfs::BYTE,
    buff: *mut std::os::raw::c_void,
) -> fatfs::DRESULT {
    if pdrv != 0 {
        return fatfs::DRESULT_RES_PARERR;
    }
    let mut ioctl = match cmd {
        fatfs::CTRL_SYNC => IOCtl::Sync,
        fatfs::GET_SECTOR_COUNT => IOCtl::GetSectorCount(0),
        fatfs::GET_SECTOR_SIZE => IOCtl::GetSectorSize(0),
        fatfs::GET_BLOCK_SIZE => IOCtl::GetBlockSize(0),
        fatfs::CTRL_TRIM => IOCtl::Trim,
        _ => return fatfs::DRESULT_RES_PARERR,
    };
    if let Some(d) = DRIVER.lock().unwrap().as_mut() {
        let result = d.ioctl(&mut ioctl);
        if result == fatfs::DRESULT_RES_OK {
            match ioctl {
                IOCtl::GetSectorCount(count) => {
                    buff.copy_from(std::ptr::addr_of!(count).cast(), size_of_val(&count))
                }
                IOCtl::GetSectorSize(size) => {
                    buff.copy_from(std::ptr::addr_of!(size).cast(), size_of_val(&size))
                }
                IOCtl::GetBlockSize(size) => {
                    buff.copy_from(std::ptr::addr_of!(size).cast(), size_of_val(&size))
                }
                _ => {}
            }
        }
        return result;
    }
    fatfs::DRESULT_RES_NOTRDY
}

#[no_mangle]
unsafe extern "C" fn get_fattime() -> fatfs::DWORD {
    let now = chrono::Local::now();
    let year = now.year() as u32;
    let month = now.month();
    let day = now.day();
    let hour = now.hour();
    let minute = now.minute();
    let second = now.second();
    ((year - 1980) << 25)
        | (month << 21)
        | (day << 16)
        | (hour << 11)
        | (minute << 5)
        | (second << 1)
}

#[derive(PartialEq, Eq, PartialOrd, Ord, Clone)]
pub struct Entry {
    pub info: EntryInfo,
    pub name: String,
    pub datetime: chrono::NaiveDateTime,
}

#[derive(Clone, Copy, PartialEq, Eq, Ord)]
pub enum EntryInfo {
    Directory,
    File { size: u64 },
}

impl PartialOrd for EntryInfo {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(match (self, other) {
            (EntryInfo::Directory, EntryInfo::File { size: _ }) => std::cmp::Ordering::Less,
            (EntryInfo::File { size: _ }, EntryInfo::Directory) => std::cmp::Ordering::Greater,
            _ => std::cmp::Ordering::Equal,
        })
    }
}

impl std::fmt::Display for EntryInfo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Ok(match self {
            EntryInfo::Directory => f.write_fmt(format_args!("d ----",))?,
            EntryInfo::File { size } => {
                const UNITS: [&str; 4] = ["K", "M", "G", "T"];
                let mut reduced_size = *size as f64;
                let mut reduced_unit = "B";
                for unit in UNITS {
                    if reduced_size >= 1000.0 {
                        reduced_size /= 1024.0;
                        reduced_unit = unit;
                    } else {
                        break;
                    }
                }
                let formatted_size = if *size >= 1000 && reduced_size <= 9.9 {
                    format!("{:>1.1}{}", reduced_size, reduced_unit)
                } else {
                    format!("{:>3.0}{}", reduced_size, reduced_unit)
                };
                f.write_fmt(format_args!("f {formatted_size}"))?
            }
        })
    }
}

impl From<fatfs::FILINFO> for Entry {
    fn from(value: fatfs::FILINFO) -> Self {
        let name = unsafe { std::ffi::CStr::from_ptr(value.fname.as_ptr()) }
            .to_string_lossy()
            .to_string();
        let datetime = chrono::NaiveDateTime::new(
            chrono::NaiveDate::from_ymd_opt(
                (1980 + ((value.fdate >> 9) & 0x7F)).into(),
                ((value.fdate >> 5) & 0xF).into(),
                (value.fdate & 0x1F).into(),
            )
            .unwrap_or_default(),
            chrono::NaiveTime::from_hms_opt(
                ((value.ftime >> 11) & 0x1F).into(),
                ((value.ftime >> 5) & 0x3F).into(),
                ((value.ftime & 0x1F) * 2).into(),
            )
            .unwrap_or_default(),
        );
        let info = if (value.fattrib as u32 & fatfs::AM_DIR) == 0 {
            EntryInfo::File { size: value.fsize }
        } else {
            EntryInfo::Directory
        };
        Self {
            name,
            datetime,
            info,
        }
    }
}

#[derive(Clone)]
pub struct Directory {
    dir: fatfs::DIR,
}

impl Directory {
    fn open<P: AsRef<std::path::Path>>(path: P) -> Result<Self, Error> {
        let mut dir = unsafe { std::mem::zeroed() };
        match unsafe { fatfs::f_opendir(&mut dir, fatfs::path(path)?.as_ptr()) } {
            fatfs::FRESULT_FR_OK => Ok(Self { dir }),
            error => Err(error.into()),
        }
    }

    pub fn read(&mut self) -> Result<Option<Entry>, Error> {
        let mut fno = unsafe { std::mem::zeroed() };
        match unsafe { fatfs::f_readdir(&mut self.dir, &mut fno) } {
            fatfs::FRESULT_FR_OK => {
                if fno.fname[0] == 0 {
                    Ok(None)
                } else {
                    Ok(Some(fno.into()))
                }
            }
            error => Err(error.into()),
        }
    }
}

impl Drop for Directory {
    fn drop(&mut self) {
        unsafe { fatfs::f_closedir(&mut self.dir) };
    }
}

pub struct File {
    fil: fatfs::FIL,
}

impl File {
    fn open<P: AsRef<std::path::Path>>(path: P, mode: u32) -> Result<File, Error> {
        let mut fil = unsafe { std::mem::zeroed() };
        match unsafe { fatfs::f_open(&mut fil, fatfs::path(path)?.as_ptr(), mode as u8) } {
            fatfs::FRESULT_FR_OK => Ok(File { fil }),
            error => Err(error.into()),
        }
    }
}

impl std::io::Read for File {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        let mut bytes_read = 0;
        match unsafe {
            fatfs::f_read(
                &mut self.fil,
                buf.as_mut_ptr().cast(),
                buf.len() as fatfs::UINT,
                &mut bytes_read,
            )
        } {
            fatfs::FRESULT_FR_OK => Ok(bytes_read as usize),
            _ => Err(std::io::ErrorKind::BrokenPipe.into()),
        }
    }
}

impl std::io::Write for File {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        let mut bytes_written = 0;
        match unsafe {
            fatfs::f_write(
                &mut self.fil,
                buf.as_ptr().cast(),
                buf.len() as fatfs::UINT,
                &mut bytes_written,
            )
        } {
            fatfs::FRESULT_FR_OK => Ok(bytes_written as usize),
            _ => Err(std::io::ErrorKind::BrokenPipe.into()),
        }
    }

    fn flush(&mut self) -> std::io::Result<()> {
        match unsafe { fatfs::f_sync(&mut self.fil) } {
            fatfs::FRESULT_FR_OK => Ok(()),
            _ => Err(std::io::ErrorKind::BrokenPipe.into()),
        }
    }
}

impl std::io::Seek for File {
    fn seek(&mut self, pos: std::io::SeekFrom) -> std::io::Result<u64> {
        let ofs = match pos {
            std::io::SeekFrom::Current(offset) => self.fil.fptr.saturating_add_signed(offset),
            std::io::SeekFrom::Start(offset) => offset,
            std::io::SeekFrom::End(offset) => self.fil.obj.objsize.saturating_add_signed(offset),
        };
        match unsafe { fatfs::f_lseek(&mut self.fil, ofs) } {
            fatfs::FRESULT_FR_OK => Ok(self.fil.fptr),
            _ => Err(std::io::ErrorKind::BrokenPipe.into()),
        }
    }
}

impl Drop for File {
    fn drop(&mut self) {
        unsafe { fatfs::f_close(&mut self.fil) };
    }
}
