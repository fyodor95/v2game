"""Ограниченный read-only снимок настоящего события в собственном тестовом процессе."""
from __future__ import annotations

import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import re
import struct

from tools.chronology_import import engine_acceptance as engine


class ModuleEntry32W(ctypes.Structure):
    _fields_ = [('dwSize', wintypes.DWORD), ('th32ModuleID', wintypes.DWORD),
                ('th32ProcessID', wintypes.DWORD), ('GlblcntUsage', wintypes.DWORD),
                ('ProccntUsage', wintypes.DWORD), ('modBaseAddr', ctypes.POINTER(wintypes.BYTE)),
                ('modBaseSize', wintypes.DWORD), ('hModule', wintypes.HMODULE),
                ('szModule', wintypes.WCHAR * 256), ('szExePath', wintypes.WCHAR * 260)]


def pe_identity(read, actual_base, executable_bytes):
    """Исходный ImageBase берётся из квалифицированного EXE: загруженный PE может меняться."""
    def header_identity(read_at):
        dos = read_at(0, 64)
        engine.require(len(dos) == 64 and dos[:2] == b'MZ', 'Нет полного MZ header')
        offset = struct.unpack_from('<I', dos, 0x3C)[0]
        engine.require(64 <= offset <= 0x100000, 'Некорректный PE offset')
        header = read_at(offset, 56)
        engine.require(len(header) == 56 and header[:4] == b'PE\0\0' and
                       struct.unpack_from('<H', header, 4)[0] == 0x14C and
                       struct.unpack_from('<H', header, 24)[0] == 0x10B, 'Не совпал PE32/x86 header')
        return offset, struct.unpack_from('<I', header, 52)[0]

    file_offset, preferred = header_identity(lambda offset, size: executable_bytes[offset:offset + size])
    loaded_offset, loaded_base = header_identity(lambda offset, size: read(actual_base + offset, size))
    return {'actual_base': actual_base, 'preferred_base': preferred,
            'preferred_base_source': 'qualified_executable_file', 'loaded_header_base': loaded_base,
            'relocated': actual_base != preferred, 'pe_offset': loaded_offset,
            'file_pe_offset': file_offset, 'verified': True}


def inspect_native(read, base, province_id, event_id):
    """Разбор квалифицированного x86 layout; read никогда не выполняет код игры."""
    report = {'base': base, 'province_id': province_id, 'event_id': event_id,
              'read_only': True, 'atomic_snapshot': False, 'errors': []}

    def u32(address):
        return struct.unpack('<I', read(address, 4))[0]

    def vector(address):
        begin, end = struct.unpack('<II', read(address, 8))
        engine.require(end >= begin and (end - begin) % 4 == 0 and
                       end - begin <= 400000 and (begin or end == begin), 'Повреждённый vector header')
        values = list(struct.unpack('<' + 'I' * ((end - begin) // 4), read(begin, end - begin))) if end != begin else []
        return {'begin': begin, 'end': end, 'count': len(values)}, values

    def observe(key, function):
        try:
            report[key] = function()
        except (OSError, ValueError, struct.error) as error:
            report['errors'].append({'section': key, 'type': type(error).__name__, 'message': str(error)})

    def world_state():
        world, manager = u32(base + 0xE588E8), u32(base + 0xE587E4)
        engine.require(world and manager, 'Нет world/country manager')
        count, countries = u32(manager), u32(manager + 4)
        engine.require(0 < count <= 65536 and countries, 'Повреждён country manager')
        begin, end = u32(world + 0xACC), u32(world + 0xAD0)
        engine.require(begin and end >= begin and (end - begin) % 4 == 0 and
                       0 < (end - begin) // 4 <= 1000000, 'Повреждён province vector')
        return {'address': world, 'country_manager': manager, 'countries': countries,
                'country_count': count, 'province_begin': begin, 'province_end': end,
                'province_count': (end - begin) // 4, 'date_raw': u32(world + 0xB0C),
                'world_b20_byte': read(world + 0xB20, 1)[0],
                'world_d11_byte': read(world + 0xD11, 1)[0]}

    def event_state():
        manager = u32(base + 0xE58A98)
        engine.require(manager, 'Нет event hash manager')
        buckets, table = u32(manager + 4), u32(manager + 8)
        engine.require(0 < buckets <= 1000000 and table, 'Повреждён event hash manager')
        bucket = ((0x27 << 16) + event_id) % buckets
        node, seen = u32(table + bucket * 4), set()
        while node:
            engine.require(node not in seen and len(seen) < 100000, 'Цикл или слишком длинная event hash-chain')
            seen.add(node)
            address, following = u32(node), u32(node + 4)
            if address and u32(address + 8) == 0x27 and u32(address + 0xC) == event_id:
                mtth = read(address + 0xC0, 32)
                return {'found': True, 'address': address, 'hash_manager': manager, 'bucket': bucket,
                        'walked': len(seen), 'trigger_address': address + 0x30,
                        'trigger_raw_32': read(address + 0x30, 32).hex(),
                        'event_1a0_byte': read(address + 0x1A0, 1)[0],
                        'event_1a3_byte': read(address + 0x1A3, 1)[0],
                        'mtth_address': address + 0xC0, 'mtth_raw_32': mtth.hex(),
                        'mtth_base_fixed_i32': struct.unpack_from('<i', mtth, 8)[0],
                        'mtth_modifiers_begin': struct.unpack_from('<I', mtth, 12)[0],
                        'mtth_modifiers_end': struct.unpack_from('<I', mtth, 16)[0]}
            node = following
        return {'found': False, 'address': 0, 'hash_manager': manager, 'bucket': bucket, 'walked': len(seen)}

    def registry_state():
        registry = u32(base + 0xF19B80)
        engine.require(registry, 'Нет event registry')
        target = report.get('event', {}).get('address', 0)
        result = {'address': registry}
        for label, offset in (('all_province_events', 0x20), ('random_province_events', 0x30)):
            header, values = vector(registry + offset)
            result[label] = dict(header, target_indices=[i for i, value in enumerate(values) if target and value == target])
        return result

    def province_state():
        world = report.get('world')
        engine.require(world is not None and 0 < province_id < world['province_count'], 'Нет нужной провинции в world')
        province = u32(world['province_begin'] + province_id * 4)
        engine.require(province and u32(province + 0x58) == province_id, 'Не совпал province ID')
        header, values = vector(province + 0x14)
        entries = [{'address': address, 'event_id': u32(address + 0xC) if address else None} for address in values]
        target = report.get('event', {}).get('address', 0)
        return {'address': province, 'id': province_id, 'candidate_vector': header,
                'owner': read(province + 0x128, 3).decode('ascii'),
                'controller': read(province + 0x130, 3).decode('ascii'),
                'target_indices': [i for i, value in enumerate(values) if target and value == target],
                'target_id_indices': [i for i, value in enumerate(entries) if value['event_id'] == event_id],
                'candidates': entries}

    observe('world', world_state)
    observe('event', event_state)
    observe('registry', registry_state)
    observe('province', province_state)
    report['complete'] = not report['errors']
    return report


class ReadOnlyProcess:
    def __init__(self, pid, executable):
        self.pid, self.executable = pid, executable
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        self.kernel.QueryFullProcessImageNameW.restype = wintypes.BOOL
        self.kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                                ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        self.kernel.ReadProcessMemory.restype = wintypes.BOOL
        self.kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        self.kernel.WaitForSingleObject.restype = wintypes.DWORD
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
        self.kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
        self.kernel.Module32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ModuleEntry32W)]
        self.kernel.Module32FirstW.restype = wintypes.BOOL
        self.kernel.Module32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ModuleEntry32W)]
        self.kernel.Module32NextW.restype = wintypes.BOOL
        self.handle = self.kernel.OpenProcess(0x1000 | 0x0010 | 0x100000, False, pid)
        if not self.handle:
            raise OSError(ctypes.get_last_error(), 'Не открыт собственный процесс для чтения')
        try:
            engine.require(self.kernel.WaitForSingleObject(self.handle, 0) == 258, 'Собственный процесс уже завершён')
            size = wintypes.DWORD(32768)
            value = ctypes.create_unicode_buffer(size.value)
            if not self.kernel.QueryFullProcessImageNameW(self.handle, 0, value, ctypes.byref(size)):
                raise OSError(ctypes.get_last_error(), 'Не прочитан путь процесса')
            engine.require(Path(value.value).resolve() == executable, 'Путь процесса не совпал со стендом')
        except BaseException:
            self.close()
            raise

    def main_module(self):
        engine.require(self.kernel.WaitForSingleObject(self.handle, 0) == 258, 'Процесс завершён перед перечислением модулей')
        invalid = ctypes.c_void_p(-1).value
        snapshot = invalid
        for _ in range(3):
            snapshot = self.kernel.CreateToolhelp32Snapshot(0x08 | 0x10, self.pid)
            if snapshot != invalid:
                break
            if ctypes.get_last_error() != 24:  # ERROR_BAD_LENGTH допускает повтор read-only snapshot.
                break
        if snapshot == invalid or not snapshot:
            raise OSError(ctypes.get_last_error(), 'Не создан snapshot модулей собственного процесса')
        try:
            entry = ModuleEntry32W()
            entry.dwSize = ctypes.sizeof(entry)
            if not self.kernel.Module32FirstW(snapshot, ctypes.byref(entry)):
                raise OSError(ctypes.get_last_error(), 'Не прочитан первый модуль собственного процесса')
            for _ in range(4096):
                if Path(entry.szExePath).resolve() == self.executable:
                    base = ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value
                    engine.require(base and 0 < entry.modBaseSize and base + entry.modBaseSize <= 0x100000000,
                                   'Главный модуль вне допустимого x86 диапазона')
                    engine.require(self.kernel.WaitForSingleObject(self.handle, 0) == 258, 'Процесс завершён после перечисления модулей')
                    return {'base': base, 'size': entry.modBaseSize, 'path': entry.szExePath}
                if not self.kernel.Module32NextW(snapshot, ctypes.byref(entry)):
                    engine.require(ctypes.get_last_error() == 18, 'Ошибка перечисления модулей процесса')
                    break
            raise ValueError('Проверенный EXE не найден среди модулей собственного процесса')
        finally:
            self.kernel.CloseHandle(snapshot)

    def read(self, address, size):
        engine.require(0 < address <= 0xFFFFFFFF and 0 < size <= 1048576 and address + size <= 0x100000000,
                       'Чтение вне допустимого x86 диапазона')
        buffer = ctypes.create_string_buffer(size)
        received = ctypes.c_size_t()
        if not self.kernel.ReadProcessMemory(self.handle, address, buffer, size, ctypes.byref(received)) or received.value != size:
            raise OSError(ctypes.get_last_error(), f'Не прочитаны {size} байт по адресу 0x{address:08X}')
        return buffer.raw

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def capture(environment, session, owned, province_id, event_id):
    """Требует удерживаемый OwnedProcess вызывающего runner, произвольный PID не принимает."""
    engine.require(os.name == 'nt', 'ReadProcessMemory требует Windows')
    environment = engine.work_path(environment)
    game = environment / 'game'
    current = json.loads((environment / 'session.json').read_text(encoding='utf-8'))
    engine.require(current['token'] == session['token'] and current['pid'] == session['pid'] == owned.pid and
                   re.fullmatch('[0-9a-f]{32}', current['token']) and owned.handle,
                   'Не подтверждено владение текущей session')
    engine.require(Path(current['game']).resolve() == game and Path(session['game']).resolve() == game,
                   'Session относится к другой среде')
    engine.require(owned.kernel.WaitForSingleObject(owned.handle, 0) == 258, 'Удерживаемый процесс уже завершён')
    executable = (game / 'v2game.exe').resolve()
    engine.require(executable.is_relative_to(environment), 'EXE находится вне стенда')
    executable_bytes = executable.read_bytes()
    engine.require(engine.sha(executable_bytes) == engine.cli.EXE_HASH,
                   'Не подтверждён квалифицированный EXE внутри стенда')
    process = ReadOnlyProcess(owned.pid, executable)
    try:
        module = process.main_module()
        base = module['base']
        pe = pe_identity(process.read, base, executable_bytes)
        report = inspect_native(process.read, base, province_id, event_id)
        report.update(pid=owned.pid, session_token=session['token'], executable=str(executable),
                      executable_sha256=engine.cli.EXE_HASH, pe_verified=True, module=module, pe=pe)
        return report
    finally:
        process.close()
