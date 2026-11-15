"""Снимок и ограниченное управление видимым окном тестовой Victoria II."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import struct
import time
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--environment', type=Path, help='Проверить полный путь процесса по environment.json')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--show', action='store_true', help='Показать скрытое окно тестовой игры')
    parser.add_argument('--click', type=int, nargs=2, metavar=('X', 'Y'))
    parser.add_argument('--input-offset', type=int, nargs=2, default=(0, 0), metavar=('DX', 'DY'),
                        help='Поправка координат ввода после калибровки оконного режима игры')
    parser.add_argument('--key', type=int, help='Код виртуальной клавиши Windows')
    parser.add_argument('--text', help='Ввести ASCII-текст в активное поле тестовой игры')
    parser.add_argument('--replace-text', action='store_true', help='Выделить прежний текст Ctrl+A перед вводом')
    parser.add_argument('--backspaces', type=int, default=0, help='Удалить символы перед курсором в поле игры')
    args = parser.parse_args()
    u = c.WinDLL('user32', use_last_error=True)
    g = c.WinDLL('gdi32', use_last_error=True)
    k = c.WinDLL('kernel32', use_last_error=True)
    u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
    k.OpenProcess.restype = w.HANDLE
    k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
    k.CloseHandle.argtypes = [w.HANDLE]
    process = k.OpenProcess(0x1000, False, args.pid)
    name, size = c.create_unicode_buffer(32768), w.DWORD(32768)
    try:
        if not process or not k.QueryFullProcessImageNameW(process, 0, name, c.byref(size)):
            raise RuntimeError('Не удалось проверить целевой процесс')
        if Path(name.value).name.lower() != 'v2game.exe':
            raise RuntimeError('Разрешено только окно v2game.exe')
        if args.environment:
            metadata = json.loads((args.environment / 'environment.json').read_text(encoding='utf-8'))
            expected = (Path(metadata['game']) / 'v2game.exe').resolve()
            if Path(name.value).resolve() != expected:
                raise RuntimeError('Процесс не принадлежит указанной тестовой среде')
    finally:
        if process:
            k.CloseHandle(process)
    windows = []
    u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
    callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def visit(hwnd, unused):
        pid = w.DWORD()
        u.GetWindowThreadProcessId(hwnd, c.byref(pid))
        class_name = c.create_unicode_buffer(128)
        u.GetClassNameW(hwnd, class_name, len(class_name))
        if pid.value == args.pid and class_name.value == 'Victoria 2' and (u.IsWindowVisible(hwnd) or args.show):
            windows.append(hwnd)
        return True
    u.EnumWindows(callback_type(visit), 0)
    if not windows:
        raise RuntimeError('У процесса нет видимого окна')
    hwnd = max(windows, key=lambda h: window_area(u, h))
    if args.show:
        u.ShowWindow.argtypes = [w.HWND, c.c_int]
        u.ShowWindow(hwnd, 9)
    u.SetForegroundWindow.argtypes = [w.HWND]
    u.GetForegroundWindow.restype = w.HWND
    u.SetForegroundWindow(hwnd)
    time.sleep(.2)
    if u.GetForegroundWindow() != hwnd:
        foreground = u.GetForegroundWindow()
        foreground_thread = u.GetWindowThreadProcessId(foreground, None)
        current_thread = k.GetCurrentThreadId()
        u.AttachThreadInput.argtypes = [w.DWORD, w.DWORD, w.BOOL]
        attached = u.AttachThreadInput(current_thread, foreground_thread, True)
        try:
            if attached:
                u.BringWindowToTop.argtypes = [w.HWND]
                u.BringWindowToTop(hwnd)
                u.SetForegroundWindow(hwnd)
        finally:
            if attached:
                u.AttachThreadInput(current_thread, foreground_thread, False)
        time.sleep(.2)
    if u.GetForegroundWindow() != hwnd:
        raise RuntimeError('Игра не получила фокус; ввод и снимок отменены')
    def require_focus():
        if u.GetForegroundWindow() != hwnd:
            raise RuntimeError('Игра потеряла фокус; ввод отменён')
    def key_event(key, up=False):
        if not up:
            require_focus()
        scan = u.MapVirtualKeyW(key, 4)
        flags = (2 if up else 0) | (1 if scan >> 8 == 0xE0 else 0)
        u.keybd_event(key, scan & 255, flags, 0)
    if not 0 <= args.backspaces <= 256:
        raise ValueError('Число удаляемых символов: 0..256')
    for _ in range(args.backspaces):
        require_focus()
        key_event(0x08)
        time.sleep(.04)
        key_event(0x08, up=True)
        time.sleep(.04)
    if args.text:
        if not args.text.isascii() or any(ord(ch) < 32 for ch in args.text):
            raise ValueError('Для полей стенда разрешён только печатный ASCII-текст')
        if args.replace_text:
            require_focus()
            key_event(0x11)
            try:
                time.sleep(.08)
                key_event(0x41)
                time.sleep(.08)
            finally:
                # Отпускаем уже нажатые клавиши даже при потере фокуса.
                key_event(0x41, up=True)
                key_event(0x11, up=True)
            time.sleep(.1)
        for ch in args.text:
            require_focus()
            if not u.PostMessageW(hwnd, 0x0102, ord(ch), 0):
                raise RuntimeError('Не удалось отправить символ в окно игры')
        time.sleep(.2)
    rect = w.RECT()
    u.GetClientRect(hwnd, c.byref(rect))
    point = w.POINT(0, 0)
    u.ClientToScreen(hwnd, c.byref(point))
    width, height = rect.right, rect.bottom
    if args.click:
        x, y = args.click
        if not 0 <= x < width or not 0 <= y < height:
            raise ValueError('Координаты вне клиентской области игры')
        physical_x, physical_y = x + args.input_offset[0], y + args.input_offset[1]
        if not 0 <= physical_x < width or not 0 <= physical_y < height:
            raise ValueError('Скорректированные координаты вне окна игры')
        u.SetCursorPos(point.x + physical_x, point.y + physical_y)
        time.sleep(.25)
        require_focus()
        u.mouse_event(2, 0, 0, 0, 0)
        time.sleep(.12)
        u.mouse_event(4, 0, 0, 0, 0)
        time.sleep(.5)
    if args.key is not None:
        if not 1 <= args.key <= 254:
            raise ValueError('Неверный код клавиши')
        require_focus()
        key_event(args.key)
        time.sleep(.12)
        key_event(args.key, up=True)
        time.sleep(.5)
    if args.output:
        require_focus()
        u.WindowFromPoint.argtypes = [w.POINT]
        u.WindowFromPoint.restype = w.HWND
        center = w.POINT(point.x + width // 2, point.y + height // 2)
        covered_pid = w.DWORD()
        u.GetWindowThreadProcessId(u.WindowFromPoint(center), c.byref(covered_pid))
        if covered_pid.value != args.pid:
            raise RuntimeError('Центр окна игры перекрыт другим приложением; снимок отменён')
        capture(u, g, point, width, height, args.output)
    print(json.dumps({'pid': args.pid, 'client_pixels': [width, height],
                      'output': str(args.output) if args.output else None}))


def window_area(u, hwnd):
    rect = w.RECT()
    u.GetClientRect(hwnd, c.byref(rect))
    return rect.right * rect.bottom


def capture(u, g, point, width, height, output):
    u.GetDC.restype = w.HDC
    u.ReleaseDC.argtypes = [w.HWND, w.HDC]
    g.CreateCompatibleDC.argtypes = [w.HDC]
    g.CreateCompatibleDC.restype = w.HDC
    g.CreateCompatibleBitmap.argtypes = [w.HDC, c.c_int, c.c_int]
    g.CreateCompatibleBitmap.restype = w.HBITMAP
    g.SelectObject.argtypes = [w.HDC, w.HANDLE]
    g.SelectObject.restype = w.HANDLE
    g.BitBlt.argtypes = [w.HDC, c.c_int, c.c_int, c.c_int, c.c_int, w.HDC, c.c_int, c.c_int, w.DWORD]
    g.GetDIBits.argtypes = [w.HDC, w.HBITMAP, w.UINT, w.UINT, w.LPVOID, w.LPVOID, w.UINT]
    g.DeleteObject.argtypes = [w.HANDLE]
    g.DeleteDC.argtypes = [w.HDC]
    screen = u.GetDC(None)
    memory = g.CreateCompatibleDC(screen)
    bitmap = g.CreateCompatibleBitmap(screen, width, height)
    old = g.SelectObject(memory, bitmap)
    try:
        if not g.BitBlt(memory, 0, 0, width, height, screen, point.x, point.y, 0x00CC0020):
            raise c.WinError(c.get_last_error())
        g.SelectObject(memory, old)
        info = c.create_string_buffer(struct.pack('<IiiHHIIiiII', 40, width, -height, 1, 32, 0, 0, 0, 0, 0, 0))
        pixels = c.create_string_buffer(width * height * 4)
        if g.GetDIBits(screen, bitmap, 0, height, pixels, info, 0) != height:
            raise RuntimeError('Не удалось прочитать пиксели окна')
        rows = bytearray()
        raw = pixels.raw
        for y in range(height):
            row = raw[y * width * 4:(y + 1) * width * 4]
            rgb = bytearray(width * 3)
            rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
            rows.extend(b'\0' + rgb)
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    finally:
        g.SelectObject(memory, old)
        g.DeleteObject(bitmap)
        g.DeleteDC(memory)
        u.ReleaseDC(None, screen)


if __name__ == '__main__':
    main()
