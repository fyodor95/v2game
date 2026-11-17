"""GUI-проверка только окна принадлежащего испытанию процесса, с DPI-awareness."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import time
import struct
import zlib
from tools.chronology_import.engine_acceptance import require,work_path


def _owned_session(environment, session, owned):
    environment = work_path(environment)
    current = json.loads((environment/'session.json').read_text(encoding='utf-8'))
    require(current['token'] == session['token'] and current['pid'] == owned.pid and owned.handle,
            'Не подтверждено владение окном')
    require(owned.kernel.WaitForSingleObject(owned.handle, 0) == 258, 'Процесс завершён')


def _input_api():
    user = ctypes.WinDLL('user32', use_last_error=True)
    user.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
    user.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    user.SetThreadDpiAwarenessContext.argtypes = [ctypes.c_void_p]
    user.SetThreadDpiAwarenessContext.restype = ctypes.c_void_p
    require(user.SetThreadDpiAwarenessContext(ctypes.c_void_p(-4)), 'Не установлен DPI-awareness v2')
    user.GetForegroundWindow.restype = wintypes.HWND
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    user.GetCursorPos.argtypes = [ctypes.POINTER(wintypes.POINT)]
    user.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
    user.WindowFromPoint.argtypes = [wintypes.POINT]
    user.WindowFromPoint.restype = wintypes.HWND
    user.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
    user.GetAncestor.restype = wintypes.HWND
    user.mouse_event.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                                wintypes.DWORD, ctypes.c_size_t]
    user.keybd_event.argtypes = [wintypes.BYTE, wintypes.BYTE, wintypes.DWORD, ctypes.c_size_t]
    return user


def _foreground(user, owned, expected=None):
    window = user.GetForegroundWindow()
    pid = wintypes.DWORD()
    user.GetWindowThreadProcessId(window, ctypes.byref(pid))
    require(pid.value == owned.pid, 'Потерян фокус; ввод остановлен')
    require(expected is None or window == expected, 'Сменилось окно игры; ввод остановлен')
    name = ctypes.create_unicode_buffer(128)
    user.GetClassNameW(window, name, len(name))
    require(name.value == 'Victoria 2', 'Фокус находится вне главного окна игры')
    return window


def _point_owned(user, owned, window, point, require_cursor=False):
    """Foreground не исключает перекрытие отдельным always-on-top окном."""
    _foreground(user, owned, window)
    hit = user.WindowFromPoint(point)
    pid = wintypes.DWORD()
    user.GetWindowThreadProcessId(hit, ctypes.byref(pid))
    require(hit and pid.value == owned.pid and user.GetAncestor(hit, 2) == window,
            'Точка ввода перекрыта другим окном; ввод остановлен')
    if require_cursor:
        current = wintypes.POINT()
        require(user.GetCursorPos(ctypes.byref(current)) and
                (current.x, current.y) == (point.x, point.y),
                'Указатель перемещён после проверки; ввод остановлен')


def _input_point(environment, session, owned, x, y):
    _owned_session(environment, session, owned)
    user = _input_api()
    window = _foreground(user, owned)
    rectangle = wintypes.RECT()
    require(user.GetClientRect(window, ctypes.byref(rectangle)), 'Нет клиентских границ')
    require(0 <= x < rectangle.right and 0 <= y < rectangle.bottom, 'Клик вне игрового окна')
    point = wintypes.POINT(x, y)
    require(user.ClientToScreen(window, ctypes.byref(point)), 'Нет физических координат')
    _point_owned(user, owned, window, point)
    require(user.SetCursorPos(point.x, point.y), 'Не перемещён указатель')
    # Инъекция движения нужна и опрашивающему DirectInput обработчику игры.
    width, height = user.GetSystemMetrics(0), user.GetSystemMetrics(1)
    require(0 <= point.x < width and 0 <= point.y < height, 'Клик вне основного экрана')
    user.mouse_event(0x8000 | 0x0001, round(point.x*65535/(width-1)),
                     round(point.y*65535/(height-1)), 0, 0)
    time.sleep(.2)
    _point_owned(user, owned, window, point, require_cursor=True)
    return user, window, point


def click_client(environment, session, owned, x, y):
    """Клик по осмотренному снимку; перекрытая точка не получает ввода."""
    user, window, point = _input_point(environment, session, owned, x, y)
    _point_owned(user, owned, window, point, require_cursor=True)
    # DirectInput опрашивает состояние по кадрам: мгновенная пара теряется.
    user.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(.12)
    _point_owned(user, owned, window, point, require_cursor=True)
    user.mouse_event(0x0004, 0, 0, 0, 0)
    return dict(client=[x, y], screen=[point.x, point.y], pid=owned.pid)


def scroll_client(environment, session, owned, x, y, notches):
    """Ограниченная прокрутка; каждый шаг заново проверяет точку и фокус."""
    require(type(notches) is int and 1 <= abs(notches) <= 8, 'Допустимо от 1 до 8 шагов колеса')
    user, window, point = _input_point(environment, session, owned, x, y)
    delta = 120 if notches > 0 else -120
    for _ in range(abs(notches)):
        _point_owned(user, owned, window, point, require_cursor=True)
        user.mouse_event(0x0800, 0, 0, delta & 0xffffffff, 0)
        time.sleep(.2)
    _point_owned(user, owned, window, point, require_cursor=True)
    return dict(client=[x, y], screen=[point.x, point.y], notches=notches, pid=owned.pid)


def press_key(environment, session, owned, key):
    """Клавиша только собственному активному окну, без возврата потерянного фокуса."""
    require(type(key) is int and 1 <= key <= 254, 'Некорректная виртуальная клавиша')
    _owned_session(environment, session, owned)
    user = _input_api()
    window = _foreground(user, owned)
    user.keybd_event(key, 0, 0, 0)
    time.sleep(.12)
    _foreground(user, owned, window)
    user.keybd_event(key, 0, 2, 0)
    return dict(key=key, pid=owned.pid)


def capture_politics(environment,session,owned,destination,open_window=True,activate=True):
    environment=work_path(environment);destination=work_path(destination)
    _owned_session(environment,session,owned)
    require(not destination.exists(),'Снимок уже существует')
    user=_input_api()
    callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    user.EnumWindows.argtypes=[callback,wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
    user.GetClassNameW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
    user.GetForegroundWindow.restype=wintypes.HWND
    user.SetForegroundWindow.argtypes=[wintypes.HWND]
    user.ShowWindow.argtypes=[wintypes.HWND,ctypes.c_int]
    user.GetWindowRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
    windows=[]
    @callback
    def each(window,_):
        pid=wintypes.DWORD();user.GetWindowThreadProcessId(window,ctypes.byref(pid))
        if pid.value==owned.pid:
            name=ctypes.create_unicode_buffer(128);user.GetClassNameW(window,name,128)
            if name.value=='Victoria 2':windows.append(window)
        return True
    require(user.EnumWindows(each,0) and len(windows)==1,'Не найдено единственное окно испытания')
    window=windows[0]
    if activate:
        user.ShowWindow(window,5);user.SetForegroundWindow(window);time.sleep(.3)
    else:
        _foreground(user,owned,window)
    rectangle=wintypes.RECT();area=wintypes.RECT()
    user.SystemParametersInfoW.argtypes=[wintypes.UINT,wintypes.UINT,ctypes.c_void_p,wintypes.UINT]
    user.SetWindowPos.argtypes=[wintypes.HWND,wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.UINT]
    require(user.GetWindowRect(window,ctypes.byref(rectangle)) and
            user.SystemParametersInfoW(0x30,0,ctypes.byref(area),0),'Нет рабочей области экрана')
    width,height=rectangle.right-rectangle.left,rectangle.bottom-rectangle.top
    require(width<=area.right-area.left and height<=area.bottom-area.top,'Окно больше рабочей области')
    if rectangle.left<area.left or rectangle.top<area.top or rectangle.right>area.right or rectangle.bottom>area.bottom:
        require(activate,'Окно сместилось за рабочую область; нужен новый осмотр')
        x=max(area.left,min(area.right-width-100,area.right-width))
        y=max(area.top,min(area.top+100,area.bottom-height))
        require(user.SetWindowPos(window,None,x,y,0,0,0x15),'Окно не перемещено в рабочую область')
        time.sleep(.3)
        require(user.GetWindowRect(window,ctypes.byref(rectangle)) and rectangle.left>=area.left and
                rectangle.top>=area.top and rectangle.right<=area.right and rectangle.bottom<=area.bottom,
                'Окно по-прежнему обрезано границей экрана')
    if activate and user.GetForegroundWindow()!=window:
        kernel=ctypes.WinDLL('kernel32');kernel.GetCurrentThreadId.restype=wintypes.DWORD
        user.AttachThreadInput.argtypes=[wintypes.DWORD,wintypes.DWORD,wintypes.BOOL]
        user.GetWindowThreadProcessId.restype=wintypes.DWORD
        user.BringWindowToTop.argtypes=[wintypes.HWND]
        own_thread=kernel.GetCurrentThreadId();attached=[]
        try:
            for target_thread in {user.GetWindowThreadProcessId(window,None),user.GetWindowThreadProcessId(user.GetForegroundWindow(),None)}:
                if target_thread and target_thread!=own_thread and user.AttachThreadInput(own_thread,target_thread,True):attached.append(target_thread)
            user.ShowWindow(window,9);user.BringWindowToTop(window);user.SetForegroundWindow(window)
        finally:
            for target_thread in reversed(attached):user.AttachThreadInput(own_thread,target_thread,False)
        time.sleep(.3)
    require(user.GetForegroundWindow()==window,'Целевое окно не получило фокус; ввод остановлен')
    if open_window:
        # F4 — штатная клавиша политического окна из interface/topbar.gui.
        press_key(environment,session,owned,0x73)
    time.sleep(.7)
    require(user.GetForegroundWindow()==window,'Потерян фокус; снимок отменён')
    rectangle=wintypes.RECT();require(user.GetWindowRect(window,ctypes.byref(rectangle)),'Нет границ окна')
    box=(rectangle.left,rectangle.top,rectangle.right,rectangle.bottom)
    require(0<box[2]-box[0]<=6400 and 0<box[3]-box[1]<=4000,'Некорректные физические размеры окна')
    width,height=box[2]-box[0],box[3]-box[1]
    pixels=capture_pixels(user,box)
    require(user.GetForegroundWindow()==window,'Потерян фокус; снимок отменён')
    write_png(destination,width,height,pixels)
    client=wintypes.RECT();origin=wintypes.POINT(0,0)
    require(user.GetClientRect(window,ctypes.byref(client)) and
            user.ClientToScreen(window,ctypes.byref(origin)),'Нет клиентских координат снимка')
    return dict(path=str(destination),pid=owned.pid,rectangle=box,size=[width,height],
                client_size=[client.right,client.bottom],client_origin=[origin.x,origin.y],
                dpi_awareness='per-monitor-v2')


class BitmapInfo(ctypes.Structure):
    _fields_=[('size',wintypes.DWORD),('width',wintypes.LONG),('height',wintypes.LONG),
              ('planes',wintypes.WORD),('bits',wintypes.WORD),('compression',wintypes.DWORD),
              ('image_size',wintypes.DWORD),('xppm',wintypes.LONG),('yppm',wintypes.LONG),
              ('colors',wintypes.DWORD),('important',wintypes.DWORD)]


def capture_pixels(user,box):
    """Читает только прямоугольник подтверждённого foreground-окна."""
    width,height=box[2]-box[0],box[3]-box[1]
    gdi=ctypes.WinDLL('gdi32',use_last_error=True)
    user.GetDC.argtypes=[wintypes.HWND];user.GetDC.restype=wintypes.HDC
    user.ReleaseDC.argtypes=[wintypes.HWND,wintypes.HDC]
    gdi.CreateCompatibleDC.argtypes=[wintypes.HDC];gdi.CreateCompatibleDC.restype=wintypes.HDC
    gdi.CreateDIBSection.argtypes=[wintypes.HDC,ctypes.POINTER(BitmapInfo),wintypes.UINT,ctypes.POINTER(ctypes.c_void_p),wintypes.HANDLE,wintypes.DWORD];gdi.CreateDIBSection.restype=wintypes.HANDLE
    gdi.SelectObject.argtypes=[wintypes.HDC,wintypes.HANDLE];gdi.SelectObject.restype=wintypes.HANDLE
    gdi.BitBlt.argtypes=[wintypes.HDC,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.HDC,ctypes.c_int,ctypes.c_int,wintypes.DWORD]
    gdi.DeleteDC.argtypes=[wintypes.HDC];gdi.DeleteObject.argtypes=[wintypes.HANDLE]
    screen=user.GetDC(None);memory=None;bitmap=None;old=None
    try:
        require(screen,'Не получен DC экрана')
        memory=gdi.CreateCompatibleDC(screen);require(memory,'Не получен DC снимка')
        info=BitmapInfo(ctypes.sizeof(BitmapInfo),width,-height,1,32,0,width*height*4,0,0,0,0)
        pointer=ctypes.c_void_p();bitmap=gdi.CreateDIBSection(screen,ctypes.byref(info),0,ctypes.byref(pointer),None,0)
        require(bitmap and pointer.value,'Не выделен буфер снимка')
        old=gdi.SelectObject(memory,bitmap)
        require(gdi.BitBlt(memory,0,0,width,height,screen,box[0],box[1],0x40cc0020),'Не прочитано изображение окна')
        return ctypes.string_at(pointer,width*height*4)
    finally:
        if old:gdi.SelectObject(memory,old)
        if bitmap:gdi.DeleteObject(bitmap)
        if memory:gdi.DeleteDC(memory)
        if screen:user.ReleaseDC(None,screen)


def write_png(path,width,height,bgrx):
    require(len(bgrx)==width*height*4,'Некорректная длина изображения')
    rgb=bytearray(width*height*3);rgb[0::3]=bgrx[2::4];rgb[1::3]=bgrx[1::4];rgb[2::3]=bgrx[0::4]
    rows=b''.join(b'\0'+rgb[y*width*3:(y+1)*width*3] for y in range(height))
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
