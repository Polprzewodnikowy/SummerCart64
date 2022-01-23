from io import TextIOWrapper
import contextlib
import os.path
import platform



@contextlib.contextmanager
def lock_volume(volume: TextIOWrapper):
    if (os.path.ismount(volume.name)):
        if (platform.system().startswith("Windows")):
            import msvcrt
            import win32file
            import winioctlcon
            handle = msvcrt.get_osfhandle(volume.fileno())
            win32file.DeviceIoControl(handle, winioctlcon.FSCTL_LOCK_VOLUME, None, None)
            try:
                yield volume
            finally:
                try:
                    volume.flush()
                finally:
                    win32file.DeviceIoControl(handle, winioctlcon.FSCTL_UNLOCK_VOLUME, None, None)
