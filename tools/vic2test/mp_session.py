"""Завершение только созданных контроллером тестовых процессов по удерживаемому handle."""
from __future__ import annotations

import ctypes
from ctypes import wintypes
from pathlib import Path


class OwnedProcess:
    def __init__(self, pid: int, executable: Path):
        self.pid = pid
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        self.kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
        self.kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.handle = self.kernel.OpenProcess(0x100001 | 0x1000, False, pid)
        if not self.handle:
            raise OSError(ctypes.get_last_error(), 'Нет доступа к созданному тестовому процессу')
        try:
            length = wintypes.DWORD(32768)
            name = ctypes.create_unicode_buffer(length.value)
            if not self.kernel.QueryFullProcessImageNameW(self.handle, 0, name, ctypes.byref(length)):
                raise OSError(ctypes.get_last_error(), 'Не определён путь созданного процесса')
            if Path(name.value).resolve() != executable.resolve():
                raise ValueError('Путь созданного процесса отличается от тестовой среды')
        except BaseException:
            self.kernel.CloseHandle(self.handle)
            self.handle = None
            raise

    def stop(self):
        if not self.handle:
            return
        try:
            if self.kernel.WaitForSingleObject(self.handle, 0) == 258:
                if not self.kernel.TerminateProcess(self.handle, 0):
                    raise OSError(ctypes.get_last_error(), 'Не завершён тестовый процесс')
                if self.kernel.WaitForSingleObject(self.handle, 30000) != 0:
                    raise TimeoutError('Тестовый процесс не завершился')
        finally:
            self.kernel.CloseHandle(self.handle)
            self.handle = None

    def release(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None
