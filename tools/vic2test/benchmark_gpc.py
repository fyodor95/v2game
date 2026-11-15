"""Сравнить обычные дни GPC off/on в одной изолированной SP-среде.

Запуск игр выполняется только из main. Вывод резервируется исключительно новым
файлом; существующие сохранения не записываются. Для каждого режима сохраняется
один OwnedProcess, между выборками используются штатные return/load.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time

import cli
import mp_preflight
from mp_session import OwnedProcess

sys.path.insert(0, str(cli.ROOT / 'tools/creatinecore/gpc'))
from make_fixture import Spans
import make_schedule_fixture as schedule


class BenchmarkError(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise BenchmarkError(message)


def utc():
    return datetime.now(timezone.utc).isoformat()


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def integer_setting(data: bytes, name: str):
    pattern = re.compile(rb'(?m)^([ \t]*' + re.escape(name.encode('ascii')) +
                         rb'[ \t]*=[ \t]*)([0-9]+)([ \t]*(?:[;#][^\r\n]*)?\r?)$')
    found = list(pattern.finditer(data))
    require(len(found) == 1, f'Требуется ровно одно целое значение {name}')
    return int(found[0][2]), found[0].span(2)


def mode_config(original: bytes, enabled: int):
    require(enabled in (0, 1), 'Недопустимый режим GPC')
    _, (start, end) = integer_setting(original, 'ENABLE_GREAT_POWER_CULTURE_CORES')
    return original[:start] + str(enabled).encode('ascii') + original[end:]


def safe_path(path: Path, boundary: Path, *, regular=False):
    path = path.absolute()
    require(path.resolve().is_relative_to(boundary.resolve()), f'Путь выходит из стенда: {path}')
    current = path
    while current != boundary:
        require(current != current.parent, 'Неверная граница пути')
        info = current.lstat()
        require(not (getattr(info, 'st_file_attributes', 0) & 0x400),
                f'Reparse point запрещён в контролируемом пути: {current}')
        current = current.parent
    if regular:
        require(path.is_file(), f'Нет обычного файла: {path}')
    return path


def read_schedule(path: Path):
    date, flags = None, None
    # Парсер существующих fixtures сохраняет только один корневой AST за раз.
    for node in Spans(path.read_bytes().decode('cp1251')).roots():
        if node.key == 'date':
            require(date is None and isinstance(node.value, str), 'Повторная/неверная дата')
            date = node.value
        elif node.key == 'flags':
            require(flags is None and isinstance(node.value, list), 'Повторный/неверный flags')
            flags = {}
            for child in node.value:
                if child.key and child.key.lower().startswith('cc_gpc_'):
                    key = child.key.lower()
                    require(key not in flags and child.value in ('yes', 'no'), 'Неверный GPC-флаг')
                    flags[key] = child.value
        if date is not None and flags is not None:
            break
    require(date is not None and flags is not None, 'Нет корневых date/flags')
    schedule.validate_existing(flags)
    active = {key for key, value in flags.items() if value == 'yes'}
    require(len(active) == 193, 'Ожидалось 193 активных флага готового расписания')
    state = {field: sum(1 << bit for bit in range(32)
                       if f'cc_gpc_v1_{field}_b{bit:02d}_1' in active)
             for field in schedule.FIELDS}
    day = schedule.day_from_date(date)
    require(state['last_day'] == day, 'Сейв не завершает дневную фазу GPC')
    return {'date': date, 'raw_date': schedule.EPOCH + day * 24,
            'active_flags': len(active), 'state': state, 'checksum': schedule.checksum(state)}


def predict(state: dict, days: int):
    """Python-прогноз обычных тиков; не вызывает код DLL и не меняет сейв."""
    result = dict(state)
    draws, due = 0, []
    limit = schedule.MASK - schedule.MASK % 1_000_000
    for day in range(state['last_day'] + 1, state['last_day'] + days + 1):
        require(day <= schedule.MAX_DAY, 'Дата прогноза за пределами планировщика')
        if day & 1:
            while True:
                result['rng'] = schedule.xorshift32(result['rng'])
                draws += 1
                sample = result['rng'] - 1
                if sample < limit:
                    break
            if sample % 1_000_000 < 555:
                due.append(day)
                result['pass_sequence'] += 1
        result['last_day'] = day
    return {'state': result, 'checksum': schedule.checksum(result), 'draws': draws, 'due_days': due}


def process_times(process: OwnedProcess):
    kernel = process.kernel
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    kernel.GetProcessTimes.restype = wintypes.BOOL
    created, exited, system, user = (wintypes.FILETIME() for _ in range(4))
    if not kernel.GetProcessTimes(process.handle, ctypes.byref(created), ctypes.byref(exited),
                                  ctypes.byref(system), ctypes.byref(user)):
        raise OSError(ctypes.get_last_error(), 'GetProcessTimes не выполнен')
    ticks = lambda value: (value.dwHighDateTime << 32) | value.dwLowDateTime
    return {'creation_ticks': ticks(created), 'kernel_seconds': ticks(system) / 10_000_000,
            'user_seconds': ticks(user) / 10_000_000}


def artifact_manifest(game: Path):
    paths = [path for path in game.rglob('*')
             if path.suffix.lower() in ('.dll', '.ini', '.exe') and path.is_file()]
    return {path.relative_to(game).as_posix(): {'sha256': cli.digest(safe_path(path, game, regular=True)),
                                              'bytes': path.stat().st_size}
            for path in sorted(paths)}


def input_fingerprint(environment: Path):
    """Входы игры плюс настройки, которые MP-сравнение намеренно исключает."""
    source = mp_preflight.fingerprint_environment(environment)
    entries = {'game/' + name: entry for name, entry in source['entries'].items()}
    extras = [('game', name) for name in mp_preflight.EXCLUDED_FILES]
    extras += [('userdata', name) for name in ('settings.txt', 'messagetypes_custom.txt')]
    for scope, name in extras:
        directory = Path(source[scope])
        path = directory / name
        key = scope + '/' + name
        try:
            path.lstat()
        except FileNotFoundError:
            # Появление ранее отсутствовавшего override тоже меняет входы.
            entries[key] = {'kind': 'missing'}
            continue
        mp_preflight._checked_path(path, directory)
        info = mp_preflight._regular_stat(path)
        require(path.is_file(), f'Ожидался файл настроек: {path}')
        entries[key] = {'kind': 'file', 'size': info.st_size,
                        'sha256': mp_preflight._sha256(path, info)}
    payload = json.dumps({'schema': 1, 'entries': entries}, sort_keys=True,
                         ensure_ascii=False, separators=(',', ':')).encode('utf-8')
    return {'schema': 1, 'fingerprint': sha_bytes(payload), 'entries': entries,
            'excluded': [item for item in source['excluded']
                         if item['path'].casefold() not in mp_preflight.EXCLUDED_FILES]}


def input_differences(baseline: dict, current: dict, *, config_path=None, config_bytes=None):
    expected = dict(baseline['entries'])
    if config_path is not None:
        require(config_bytes is not None and config_path in expected, 'Неизвестный INI режима')
        expected[config_path] = {'kind': 'file', 'size': len(config_bytes), 'sha256': sha_bytes(config_bytes)}
    actual = current['entries']
    return [{'path': name, 'before': expected.get(name), 'after': actual.get(name)}
            for name in sorted(expected.keys() | actual.keys()) if expected.get(name) != actual.get(name)]


def checked(result: dict, action: str):
    require(isinstance(result, dict) and result.get('ok') is True, f'{action}: {result}')
    return result


class Benchmark:
    def __init__(self, args, stream, output):
        self.args, self.stream, self.output = args, stream, output
        self.artifacts = output.with_name(output.stem + '.artifacts')
        self.environment = args.environment.absolute()
        self.record = {'schema': 1, 'kind': 'gpc_ordinary_days_sp', 'ok': False, 'complete': False,
                       'started_utc': utc(), 'environment': str(self.environment),
                       'parameters': {'days': args.days, 'samples': args.samples, 'warmup_days': 1,
                                      'order': args.order, 'timeout': args.timeout,
                                      'transition_timeout': args.transition_timeout, 'save': args.save,
                                      'cpu_times': not args.no_cpu, 'limit_percent': 5.0},
                       'modes': [], 'errors': [], 'warnings': []}
        self.original_ini = None
        self.config = None
        self.save = None
        self.game = None
        self.common_start = None
        self.input_baseline = None
        self.save_report()

    def save_report(self):
        self.stream.seek(0)
        json.dump(self.record, self.stream, ensure_ascii=False, indent=2)
        self.stream.write('\n')
        self.stream.truncate()
        self.stream.flush()

    def artifact(self, name, data):
        path = self.artifacts / name
        with path.open('xb') as stream:
            stream.write(data)
        return {'path': str(path), 'sha256': sha_bytes(data), 'bytes': len(data)}

    def preflight(self):
        require(os.name == 'nt', 'Нужна Windows с нативным CLI-мостом')
        work = (cli.ROOT / '.work').resolve()
        require(self.environment.resolve().is_relative_to(work), 'Среда должна находиться внутри .work')
        safe_path(self.environment, work)
        metadata_path = safe_path(self.environment / 'environment.json', self.environment, regular=True)
        metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
        self.game, userdata = self.environment / 'game', self.environment / 'userdata'
        require(Path(metadata['game']).resolve() == self.game.resolve() and
                Path(metadata['userdata']).resolve() == userdata.resolve(), 'metadata указывает на другую среду')
        safe_path(self.game, self.environment)
        safe_path(userdata, self.environment)
        require(not self.output.resolve().is_relative_to(self.game.resolve()) and
                not self.output.resolve().is_relative_to(userdata.resolve()),
                'Отчёт и артефакты должны находиться вне игровых данных и userdata')
        redirect = safe_path(self.game / 'userdir.txt', self.game, regular=True).read_text(encoding='ascii').strip()
        require(Path(redirect).resolve() == userdata.resolve(), 'Не подтверждён userdir тестовой среды')
        require(re.fullmatch(r'[A-Za-z0-9_-]{1,64}\.v2', self.args.save), 'Неверное имя копии сейва')
        self.save = safe_path(userdata / 'save games' / self.args.save, userdata, regular=True)
        self.config = safe_path(self.game / 'z_CreatineCore/great_power_culture_cores.ini', self.game, regular=True)
        require(self.config.stat().st_nlink == 1, 'Изменяемый INI не должен иметь жёстких ссылок')
        original = self.config.read_bytes()
        require(integer_setting(original, 'GREAT_POWER_CULTURE_CORES_MTTH_MONTHS')[0] == 60,
                'Для этого бенча требуется производственный MTTH 60')
        mode_config(original, 0)
        settings_path = safe_path(self.game / 'z_CreatineCore/creatinecore_settings.ini', self.game, regular=True)
        settings = settings_path.read_bytes()
        require(integer_setting(settings, 'ENABLE_RNG_PROBE')[0] == 0, 'Нужно ENABLE_RNG_PROBE=0')
        require(integer_setting(settings, 'BUILDING_GOODS_CONSUMPTION')[0] == 0,
                'Нужно сохранить baseline BUILDING_GOODS_CONSUMPTION=0')
        require(not any(key == 'VIC2_GPC_TEST_ALWAYS' or key == 'VIC2_GPC_MUTATION_ALLOWED' or
                        key.startswith('VIC2_GPC_PROBE_') for key in os.environ),
                'Удалите TEST_ALWAYS и переменные квалификационного probe из окружения')
        root_settings = cli.ROOT / 'z_CreatineCore/creatinecore_settings.ini'
        require(integer_setting(root_settings.read_bytes(), 'BUILDING_GOODS_CONSUMPTION')[0] == 0,
                'Основной baseline BUILDING_GOODS_CONSUMPTION должен оставаться 0')
        user_settings = safe_path(userdata / 'settings.txt', userdata, regular=True).read_bytes()
        require(re.search(rb'(?m)^autosave\s*=\s*"NEVER"\s*$', user_settings), 'Автосохранения должны быть отключены')
        require(re.search(rb'(?m)^fullScreen\s*=\s*no\s*$', user_settings), 'Требуется оконный тестовый режим')
        require(cli.digest(self.game / 'v2game.exe') == cli.EXE_HASH, 'Неподдерживаемый EXE')
        inventory = artifact_manifest(self.game)
        require('z_CreatineCore/dll_Submods/ccore_great_power_culture_cores.dll' in inventory,
                'Нет производственной GPC DLL в тестовой среде')
        require(not any('qualification' in name.lower() or 'probe' in Path(name).stem.lower()
                        for name in inventory if name.lower().endswith('.dll')),
                'Квалификационные/probe DLL нельзя включать в бенч')
        source = read_schedule(self.save)
        warmup = predict(source['state'], 1)
        whole = predict(source['state'], self.args.days + 1)
        require(not whole['due_days'], f'В интервале есть полный проход GPC: {whole["due_days"]}')
        self.artifacts.mkdir(exist_ok=False)
        self.original_ini = original
        self.record.update(metadata=metadata, source_save={'path': str(self.save), 'sha256': cli.digest(self.save),
                           'bytes': self.save.stat().st_size, **source}, forecast={'warmup': warmup, 'end': whole},
                           initial_artifacts=inventory, original_ini=self.artifact('gpc-original.ini', original),
                           baseline_root_config={'path': str(root_settings), 'sha256': cli.digest(root_settings)},
                           test_core_config={'path': str(settings_path), 'sha256': cli.digest(settings_path)},
                           process_environment={key: value for key, value in os.environ.items() if key.startswith('VIC2_')})
        self.record['tool_sources'] = {str(path.relative_to(cli.ROOT)): cli.digest(path)
                                       for path in (Path(__file__), Path(cli.__file__), Path(mp_preflight.__file__),
                                                    Path(schedule.__file__), cli.ROOT / 'tools/vic2test/mp_session.py')}
        # start делает то же самое. Нормализация ДО baseline учитывает реальные
        # настройки измерения и не маскирует последующие изменения этих файлов.
        for directory in (self.game, userdata):
            settings_file = directory / 'settings.txt'
            if settings_file.exists() or settings_file.is_symlink():
                safe_path(settings_file, directory, regular=True)
        cli.mute_test_settings(self.environment)
        self.input_baseline = input_fingerprint(self.environment)
        self.record['input_fingerprint'] = self.input_baseline
        self.save_report()

    def session(self):
        path = self.environment / 'session.json'
        if not path.exists():
            return None
        return json.loads(safe_path(path, self.environment, regular=True).read_text(encoding='utf-8'))

    def advance(self, days, output):
        command = [sys.executable, '-B', str(Path(cli.__file__)), 'advance', '--environment', str(self.environment),
                   '--days', str(days), '--timeout', str(self.args.timeout), '--auto-events']
        output['command'] = command
        child_env = dict(os.environ, PYTHONIOENCODING='utf-8')
        completed = subprocess.run(command, capture_output=True, encoding='utf-8', env=child_env,
                                   timeout=self.args.timeout + 90, check=False)
        output.update(returncode=completed.returncode, stdout=completed.stdout, stderr=completed.stderr)
        response = json.loads(completed.stdout)
        output['response'] = response
        checked(response, 'advance')
        require(completed.returncode == 0 and response.get('completed') is True, 'advance не завершился')
        return response

    def state(self, enabled, process, expected):
        status = checked(cli.request(self.environment, 'status', 30), 'status')
        gpc = checked(cli.request(self.environment, 'culture-cores-status', 30), 'culture-cores-status')
        require(status.get('pid') == process.pid and status.get('multiplayer') is False and
                status.get('campaign_loaded') is True and status.get('paused') is True and
                status.get('advance_target') == 0, 'Не подтверждена пауза собственной SP-кампании')
        require(type(status.get('date_raw')) is int and type(status.get('auto_events_selected')) is int,
                'Неверный протокол календаря/auto-events')
        require(gpc.get('installed') is True and gpc.get('version') == 1 and
                gpc.get('enabled') == enabled and gpc.get('failed') == 0 and gpc.get('busy') == 0,
                'Не подтверждён исправный runtime нужного режима')
        expected_date = schedule.EPOCH + expected['state']['last_day'] * 24
        require(status['date_raw'] == expected_date, 'Дата отличается от одинакового интервала сейва')
        if enabled:
            require(gpc.get('has_schedule') == 1 and gpc.get('completed_raw_date') == expected_date and
                    gpc.get('callback_raw_date') == expected_date, 'Не завершена дневная фаза GPC')
            require(gpc.get('pass') == expected['state']['pass_sequence'] and
                    gpc.get('rng') == expected['state']['rng'] and
                    gpc.get('state_digest') == expected['checksum'] and gpc.get('core_digest') == 0,
                    'Реальное расписание отличается от прогноза обычных дней')
        else:
            require(all(gpc.get(field) == 0 for field in ('has_schedule', 'completed_raw_date', 'callback_raw_date',
                        'pass', 'rng', 'state_digest', 'core_digest')), 'Выключенный runtime изменил status')
        return {'status': status, 'gpc': gpc}

    def collect_logs(self, mode):
        mode['logs'] = {}
        for name in ('v2dll.log', 'v2dll_oos.log'):
            path = self.game / 'z_CreatineCore/Logs' / name
            if path.exists():
                safe_path(path, self.game, regular=True)
                mode['logs'][name] = self.artifact(mode['name'] + '-' + name, path.read_bytes())
        require('v2dll.log' in mode['logs'], 'Нет runtime-журнала созданного процесса')

    def run_mode(self, enabled):
        name = 'on' if enabled else 'off'
        print(f'GPC {name}: запуск и {self.args.samples} выборки по {self.args.days} дней', file=sys.stderr, flush=True)
        mode = {'name': name, 'enabled': enabled, 'samples': [], 'complete': False}
        self.record['modes'].append(mode)
        require(cli.digest(self.save) == self.record['source_save']['sha256'], 'Исходная копия сейва изменилась')
        self.config.write_bytes(mode_config(self.original_ini, enabled))
        mode['ini'] = self.artifact(name + '-gpc.ini', self.config.read_bytes())
        mode['artifact_hashes'] = artifact_manifest(self.game)
        expected_artifacts = dict(self.record['initial_artifacts'])
        expected_artifacts[self.config.relative_to(self.game).as_posix()] = {
            'sha256': mode['ini']['sha256'], 'bytes': mode['ini']['bytes']}
        require(mode['artifact_hashes'] == expected_artifacts, 'Другие DLL/config/EXE изменились до запуска режима')
        mode['input_fingerprint'] = input_fingerprint(self.environment)
        mode['input_differences'] = input_differences(
            self.input_baseline, mode['input_fingerprint'],
            config_path='game/' + self.config.relative_to(self.game).as_posix(),
            config_bytes=mode_config(self.original_ini, enabled))
        require(not mode['input_differences'], 'Изменились входы стенда до запуска режима: ' +
                ', '.join(item['path'] for item in mode['input_differences'][:20]))
        previous = self.session()
        owned = None
        loaded_signature = None
        try:
            try:
                mode['start'] = cli.start(self.environment, hidden=True, save=self.args.save)
            finally:
                # start может отказать ПОСЛЕ Popen/записи session и до возврата PID.
                current = self.session()
                if current and current.get('token') != (previous or {}).get('token'):
                    require(re.fullmatch(r'[a-f0-9]{32}', current.get('token', '')) and
                            type(current.get('pid')) is int and current['pid'] > 0 and
                            Path(current.get('game', '')).resolve() == self.game.resolve(),
                            'Новая session не подтверждает собственный процесс')
                    mode['session'] = current
                    try:
                        owned = OwnedProcess(current['pid'], self.game / 'v2game.exe')
                    except Exception as error:
                        mode['cleanup_unconfirmed'] = True
                        mode['ownership_error'] = str(error)
                        raise
            checked(mode['start'], 'start')
            require(owned is not None, 'start не создал новую session; чужой процесс не используется')
            mode['pid'] = owned.pid
            for index in range(self.args.samples):
                sample = {'index': index + 1, 'complete': False, 'warmup': {}, 'advance': {}}
                mode['samples'].append(sample)
                self.save_report()
                if index:
                    sample['return_menu'] = checked(cli.return_to_menu(self.environment, self.args.transition_timeout), 'return-menu')
                    sample['load'] = checked(cli.load_campaign(self.environment, self.args.save, self.args.transition_timeout), 'load')
                require(self.session()['token'] == mode['session']['token'] and self.session()['pid'] == owned.pid,
                        'Между выборками сменился процесс/session')
                self.advance(1, sample['warmup'])
                start = self.state(enabled, owned, self.record['forecast']['warmup'])
                sample['start'] = start
                common = {key: start['status'][key] for key in ('date_raw', 'event_windows', 'auto_events_selected')}
                signature = {'common': common, 'gpc': start['gpc']}
                if loaded_signature is None:
                    loaded_signature = signature
                require(signature == loaded_signature, 'Состояние после повторной загрузки/прогрева отличается')
                if self.common_start is None:
                    self.common_start = common
                require(common == self.common_start, 'Начальное observable-состояние off/on отличается')
                sample['cpu_start'] = process_times(owned) if not self.args.no_cpu else None
                began = time.perf_counter()
                try:
                    advanced = self.advance(self.args.days, sample['advance'])
                finally:
                    sample['wall_seconds'] = time.perf_counter() - began
                sample['cpu_end'] = process_times(owned) if not self.args.no_cpu else None
                end = self.state(enabled, owned, self.record['forecast']['end'])
                sample['end'] = end
                delta = end['status']['date_raw'] - start['status']['date_raw']
                require(delta > 0 and delta % 24 == 0, 'Неверная фактическая длительность')
                actual = delta // 24
                require(actual == self.args.days and advanced.get('date_raw') == end['status']['date_raw'] and
                        advanced.get('requested_target_date_raw') == end['status']['date_raw'], 'Продвинут неверный интервал')
                require(not enabled or start['gpc']['pass'] == end['gpc']['pass'], 'Полный проход попал в ordinary-бенч')
                sample.update(actual_days=actual, wall_seconds_per_day=sample['wall_seconds'] / actual,
                              auto_event_choices={'warmup': start['status']['auto_events_selected'],
                                                  'timed': end['status']['auto_events_selected']})
                if sample['cpu_start'] is not None:
                    require(sample['cpu_start']['creation_ticks'] == sample['cpu_end']['creation_ticks'], 'Сменился объект процесса')
                    cpu = sum(sample['cpu_end'][key] - sample['cpu_start'][key]
                              for key in ('kernel_seconds', 'user_seconds'))
                    require(cpu >= 0, 'Счётчик CPU уменьшился')
                    sample.update(cpu_seconds=cpu, cpu_seconds_per_day=cpu / actual)
                require(cli.digest(self.save) == self.record['source_save']['sha256'], 'Бенч изменил копию сейва')
                sample['complete'] = True
                print(f'GPC {name}, {index + 1}: {sample["wall_seconds"]:.3f} с / {actual} дней; '
                      f'auto-events={sample["auto_event_choices"]}', file=sys.stderr, flush=True)
                self.save_report()
            mode['complete'] = True
        finally:
            if owned is not None:
                try:
                    owned.stop()
                    mode['process_stopped'] = True
                except Exception as error:
                    self.record['errors'].append({'stage': 'process_cleanup', 'mode': name, 'error': str(error)})
                    mode['process_stopped'] = False
                try:
                    self.collect_logs(mode)
                except Exception as error:
                    self.record['errors'].append({'stage': 'collect_logs', 'mode': name, 'error': str(error)})
            self.save_report()
        require(not self.record['errors'], 'Ошибка завершения процесса или сбора журналов')

    def summarize(self):
        medians = {}
        for mode in self.record['modes']:
            require(mode['complete'] and len(mode['samples']) == self.args.samples and
                    all(sample['complete'] for sample in mode['samples']), 'Нет полного набора выборок')
            medians[mode['name']] = statistics.median(sample['wall_seconds_per_day'] for sample in mode['samples'])
            mode['median_wall_seconds_per_day'] = medians[mode['name']]
            if not self.args.no_cpu:
                mode['median_cpu_seconds_per_day'] = statistics.median(sample['cpu_seconds_per_day'] for sample in mode['samples'])
            choices = [sample['auto_event_choices']['timed'] for sample in mode['samples']]
            if any(choices):
                self.record['warnings'].append(f'{mode["name"]}: нативные первые опции фоновых событий: {choices}')
        require(medians['off'] > 0, 'Нулевой baseline недопустим')
        overhead = (medians['on'] / medians['off'] - 1) * 100
        self.record['comparison'] = {'median_wall_seconds_per_day': medians,
                                     'overhead_percent': overhead, 'limit_percent': 5.0,
                                     'within_limit': overhead <= 5.0}
        self.record.update(complete=True, ok=overhead <= 5.0)
        self.record['boundary_uncertainty'] = {'direction': 'unknown', 'numeric_correction_applied': False}
        self.record['warnings'].append('Off не подтверждает конец дня после прогрева и в конце замера; '
                                       'on ждёт GPC-барьер. Остатки этих дней могут сдвигать отношение '
                                       'on/off в обе стороны; численная поправка не применяется.')

    def run(self):
        try:
            self.preflight()
            for enabled in ((0, 1) if self.args.order == 'off-on' else (1, 0)):
                self.run_mode(enabled)
            self.summarize()
        except BaseException as error:
            self.record.update(ok=False, complete=False)
            self.record['errors'].append({'stage': 'benchmark', 'type': type(error).__name__, 'error': str(error)})
        finally:
            if self.original_ini is not None:
                try:
                    safe_path(self.config, self.game, regular=True)
                    require(self.config.stat().st_nlink == 1, 'У INI появились жёсткие ссылки')
                    self.config.write_bytes(self.original_ini)
                    require(self.config.read_bytes() == self.original_ini, 'INI не восстановлен побайтно')
                    self.record['ini_restored'] = True
                    for field in ('baseline_root_config', 'test_core_config'):
                        expected = self.record[field]
                        require(cli.digest(Path(expected['path'])) == expected['sha256'], f'Изменился {field}')
                    require(cli.digest(self.save) == self.record['source_save']['sha256'], 'Изменился исходный сейв')
                    require(artifact_manifest(self.game) == self.record['initial_artifacts'], 'DLL/config/EXE изменились во время бенча')
                    if self.input_baseline is not None:
                        final = input_fingerprint(self.environment)
                        differences = input_differences(self.input_baseline, final)
                        self.record['final_input_fingerprint'] = final
                        self.record['final_input_differences'] = differences
                        require(not differences, 'Входы игры или настройки изменились во время бенча: ' +
                                ', '.join(item['path'] for item in differences[:20]))
                except Exception as error:
                    self.record.update(ok=False, complete=False)
                    self.record['errors'].append({'stage': 'restore_or_integrity', 'error': str(error)})
            self.record['finished_utc'] = utc()
            self.save_report()
        return 0 if self.record['ok'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='Новый JSON внутри .work; рядом создаётся новый .artifacts')
    parser.add_argument('--save', default='gpc31_empty.v2')
    parser.add_argument('--days', type=int, default=100)
    parser.add_argument('--samples', type=int, default=3)
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--transition-timeout', type=float, default=600)
    parser.add_argument('--order', choices=('off-on', 'on-off'), default='off-on')
    parser.add_argument('--no-cpu', action='store_true')
    args = parser.parse_args()
    try:
        require(1 <= args.days <= 365 and 3 <= args.samples <= 9, 'Нужно 1..365 дней и 3..9 выборок')
        require(30 <= args.timeout <= 3600 and 30 <= args.transition_timeout <= 1800, 'Сроки вне допустимых границ')
        output = args.output.absolute()
        work = (cli.ROOT / '.work').resolve()
        require(output.resolve().is_relative_to(work) and output.suffix.lower() == '.json', 'Отчёт должен быть новым JSON внутри .work')
        safe_path(output.parent, work)
        with output.open('x', encoding='utf-8', newline='\n') as stream:
            benchmark = Benchmark(args, stream, output)
            code = benchmark.run()
            print(json.dumps({'ok': benchmark.record['ok'], 'complete': benchmark.record['complete'],
                              'report': str(output), 'comparison': benchmark.record.get('comparison'),
                              'errors': benchmark.record['errors']}, ensure_ascii=False, indent=2))
            return code
    except (OSError, ValueError, BenchmarkError) as error:
        print(json.dumps({'ok': False, 'error': str(error)}, ensure_ascii=False), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
