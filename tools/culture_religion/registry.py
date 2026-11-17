"""Сверка загруженных движком событий с файлами собственной тестовой копии."""
from __future__ import annotations
import json
from pathlib import Path
import struct
from tools.culture_religion.snapshot import probe, require, work_path
from tools.vic2test.scripts import parse


def capture(environment, session, owned):
    """Только чтение после Loading Events; владельцем процесса остаётся вызывающий."""
    environment = work_path(environment)
    game = environment / 'game'
    current = json.loads((environment / 'session.json').read_text(encoding='utf-8'))
    require(current['token'] == session['token'] and current['pid'] == owned.pid and owned.handle,
            'Не подтверждено владение тестовой сессией')
    require(Path(current['game']).resolve() == game and owned.kernel.WaitForSingleObject(owned.handle, 0) == 258,
            'Чужая среда или процесс завершён')
    executable = game / 'v2game.exe'
    require(probe.engine.cli.digest(executable) == probe.engine.cli.EXE_HASH, 'Неизвестный EXE')
    expected = {'country_event': {}, 'province_event': {}}
    for path in sorted((game / 'events').glob('*.txt')):
        for event in parse(path.read_bytes().decode('cp1251')):
            if event.key not in expected:
                continue
            number = int(next(n.value for n in event.children if n.key == 'id'))
            require(number not in expected[event.key], 'Повтор ID в файлах: ' + str(number))
            expected[event.key][number] = path.name
    process = probe.ReadOnlyProcess(owned.pid, executable)
    try:
        base = process.main_module()['base']
        identity = probe.pe_identity(process.read, base, executable.read_bytes())
        u32 = lambda at: struct.unpack('<I', process.read(at, 4))[0]
        manager = u32(base + 0xf19b80)
        require(manager, 'Нет реестра событий')
        result = dict(read_only=True, pid=owned.pid, session_token=session['token'], pe=identity, registries={})
        for kind, offset in (('country_event', 0), ('province_event', 0x20)):
            begin, end = struct.unpack('<II', process.read(manager + offset, 8))
            require(end >= begin and (end - begin) % 4 == 0 and end - begin <= 4000000,
                    'Повреждённый диапазон реестра')
            objects = struct.unpack('<' + 'I' * ((end - begin) // 4), process.read(begin, end - begin)) if end > begin else []
            require(all(objects), 'Пустой указатель события')
            ids = [u32(obj + 12) for obj in objects]
            require(len(ids) == len(set(ids)), 'Повтор ID в нативном реестре')
            missing = sorted(set(expected[kind]) - set(ids))
            extra = sorted(set(ids) - set(expected[kind]))
            result['registries'][kind] = dict(expected=len(expected[kind]), actual=len(ids), missing=missing,
                                              extra=extra, missing_files={str(i): expected[kind][i] for i in missing})
        result['ok'] = all(not r['missing'] and not r['extra'] for r in result['registries'].values())
        return result
    finally:
        process.close()
