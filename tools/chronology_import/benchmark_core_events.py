"""Сравнить старые/новые CoreAddingEvents в изолированной игре через native CLI.

Один процесс на набор событий, повторные загрузки двух неизменных сейвов.
По умолчанию 12 замеров по 100 дней. --dry-run не пишет файлы и не запускает игру.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
from ctypes import wintypes
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/vic2test'))
sys.path.insert(0, str(ROOT))
import cli
import mp_preflight
from mp_session import OwnedProcess
from benchmark_gpc import (BenchmarkError, checked, input_fingerprint, integer_setting,
                           mode_config, process_times, require, safe_path, sha_bytes, utc)
from tools.chronology_import.validate_core_events import parse, reconstruct_map, validate_events
from tools.chronology_import import native_event_probe
from tools.chronology_import.engine_acceptance import Spans
from tools.culture_religion.native_acceptance import wait_initialisation

EVENT = 'events/adding_cores.txt'
CORE_INI = 'z_CreatineCore/creatinecore_settings.ini'
GPC_INI = 'z_CreatineCore/great_power_culture_cores.ini'
OLD_COMMIT = '88c23775'
STARTUP_TIMEOUT = 900


def quote_country_scope_keys(raw):
    """Экранировать PHI/PAN только в ключах блоков, сохранив остальные байты."""
    text = raw.decode('cp1251')
    edits, counts = [], {'PHI': 0, 'PAN': 0}

    def visit(node):
        if not isinstance(node.value, list):
            return
        if node.key in counts and text[node.key_start:node.key_end] == node.key:
            edits.append((node.key_start, node.key_end, '"' + node.key + '"'))
            counts[node.key] += 1
        for child in node.value:
            visit(child)

    for node in Spans(text).roots():
        visit(node)
    for start, end, replacement in sorted(edits, reverse=True):
        text = text[:start] + replacement + text[end:]
    return text.encode('cp1251'), counts


def model_inputs(root):
    """Все исходные определения модели должны совпадать со стендом."""
    result = {}
    for folder in ('common', 'history'):
        for path in (root / folder).rglob('*'):
            if path.is_file():
                result[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    config = root / 'autogen.cfg'
    require(config.is_file(), 'В стенде нет исходной конфигурации autogen.cfg')
    result['autogen.cfg'] = hashlib.sha256(config.read_bytes()).hexdigest()
    return result


def compact(fingerprint):
    """Полный manifest остаётся в памяти, отчёт содержит только проверяемый итог."""
    return {'sha256': fingerprint['fingerprint'], 'entries': len(fingerprint['entries'])}


def differences(baseline, current, replacements=None):
    expected = dict(baseline['entries'])
    for name, data in (replacements or {}).items():
        require(name in expected, f'Неизвестный заменяемый вход: {name}')
        expected[name] = {'kind': 'file', 'size': len(data), 'sha256': sha_bytes(data)}
    actual = current['entries']
    return [{'path': name, 'expected': expected.get(name), 'actual': actual.get(name)}
            for name in sorted(expected.keys() | actual.keys()) if expected.get(name) != actual.get(name)]


def comparison(modes, saves, samples):
    result = {}
    require({mode['name'] for mode in modes} == {'old', 'new'} and len(modes) == 2,
            'Нужны оба разных режима')
    for save in saves:
        values = {}
        for mode in modes:
            selected = [item for item in mode['samples'] if item['save'] == save]
            require(len(selected) == samples and all(item['complete'] for item in selected),
                    'Нет полного набора завершённых выборок')
            values[mode['name']] = {key: statistics.median(item[key] for item in selected)
                                    for key in ('wall_seconds_per_day', 'cpu_seconds_per_day')}
            require(all(value > 0 for value in values[mode['name']].values()), 'Нулевое время замера')
        result[save] = {'medians': values, 'change_percent': {
            key: (values['new'][key] / values['old'][key] - 1) * 100 for key in values['old']}}
    return result


def session(environment):
    path = environment / 'session.json'
    if not path.exists():
        return None
    return json.loads(safe_path(path, environment, regular=True).read_text(encoding='utf-8'))


def require_idle(environment):
    previous = session(environment)
    if not previous:
        return
    pid = previous.get('pid')
    require(type(pid) is int and pid > 0, 'Неверный PID предыдущей session')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x100000, False, pid)
    if not handle:
        require(ctypes.get_last_error() == 87, 'Нельзя подтвердить завершение предыдущего процесса')
        return
    try:
        require(kernel.WaitForSingleObject(handle, 0) == 0,
                'В среде уже есть живой процесс; бенч его не использует и не завершает')
    finally:
        kernel.CloseHandle(handle)


class Benchmark:
    def __init__(self, args):
        self.args = args
        self.environment = args.environment.absolute()
        self.output = args.output.absolute()
        self.artifacts = self.output.with_suffix('.artifacts')
        self.record = dict(schema=1, kind='core_events_same_data_sp', ok=False, complete=False,
                           started_utc=utc(), parameters=vars(args).copy(), modes=[], errors=[])
        self.record['parameters'] = {key: str(value) if isinstance(value, Path) else value
                                     for key, value in self.record['parameters'].items()}
        self.record['parameters'].update(warmup_days=args.warmup_days, startup_timeout=STARTUP_TIMEOUT,
                                         load_timeout=1200, advance_timeout=600, gpc_enabled=0)
        self.originals = {}
        self.baseline = None
        self.stream = None
        self.dates = {}

    def checkpoint(self, stage):
        self.record.update(stage=stage, updated_utc=utc())
        if self.stream:
            self.stream.seek(0)
            json.dump(self.record, self.stream, ensure_ascii=False, indent=2)
            self.stream.write('\n')
            self.stream.truncate()
            self.stream.flush()

    def preflight(self):
        require(os.name == 'nt', 'Нужна Windows с native CLI')
        work = ROOT / '.work'
        safe_path(self.environment, work)
        _, self.game, self.userdata = mp_preflight._environment_paths(self.environment)
        require(not self.output.resolve().is_relative_to(self.game.resolve()) and
                not self.output.resolve().is_relative_to(self.userdata.resolve()), 'Отчёт должен быть вне данных игры')
        require(self.output.suffix == '.json' and self.output.resolve().is_relative_to(work.resolve()),
                'Нужен новый JSON внутри .work')
        safe_path(self.output.parent, work)
        require(not self.output.exists() and not self.artifacts.exists(), 'Отчёт/артефакты уже существуют')
        require_idle(self.environment)
        self.exe = safe_path(self.game / 'v2game.exe', self.game, regular=True)
        require(cli.digest(self.exe) == cli.EXE_HASH, 'Неподдерживаемый EXE')
        for name in (EVENT, CORE_INI, GPC_INI):
            path = safe_path(self.game / name, self.game, regular=True)
            require(path.stat().st_nlink == 1, f'Жёсткая ссылка изменяемого файла: {name}')
            self.originals[name] = path.read_bytes()
        self.disabled_gpc = mode_config(self.originals[GPC_INI], 0)
        for key in ('BUILDING_GOODS_CONSUMPTION', 'PATCH_PARALLEL_EVENTS', 'ENABLE_RNG_PROBE', 'ENABLE_STATE_PROBE'):
            require(integer_setting(self.originals[CORE_INI], key)[0] == 0, f'Требуется {key}=0')
        require(not any(key == 'VIC2_GPC_TEST_ALWAYS' or key == 'VIC2_GPC_MUTATION_ALLOWED' or
                        key.startswith('VIC2_GPC_PROBE_') for key in os.environ), 'Уберите GPC probe/test переменные')
        for directory in (self.game, self.userdata):
            path = safe_path(directory / 'settings.txt', directory, regular=True)
            require(path.stat().st_nlink == 1, 'Настройки не должны быть жёсткой ссылкой')
            data = path.read_bytes()
            require(re.search(rb'(?m)^autosave\s*=\s*"NEVER"\s*$', data), 'Автосохранения должны быть отключены')
            if directory == self.userdata:
                require(re.search(rb'(?m)^fullScreen\s*=\s*no\s*$', data), 'Нужен оконный тестовый режим userdata')
        self.save_paths = {}
        for save in self.args.saves:
            require(re.fullmatch(r'[A-Za-z0-9_-]{1,64}\.v2', save), 'Неверное имя копии сейва')
            self.save_paths[save] = safe_path(self.userdata / 'save games' / save, self.userdata, regular=True)
        require(len(self.save_paths) == 2, 'Нужны два разных сейва')
        if self.args.rebel_save:
            require(self.args.rebel_save in self.save_paths and 0 < self.args.rebel_province <= 99999,
                    'Контролируемая повстанческая провинция должна относиться к одному из сейвов')
        self.save_hashes = {name: cli.digest(path) for name, path in self.save_paths.items()}
        revision = subprocess.run(['git', 'rev-parse', '--verify', getattr(self.args, 'old_commit', OLD_COMMIT) + '^{commit}'],
                                  cwd=ROOT, capture_output=True, check=True).stdout.decode('ascii').strip()
        require(re.fullmatch('[0-9a-f]{40}', revision), 'Неверный baseline commit')
        old = subprocess.run(['git', 'show', revision + ':' + EVENT], cwd=ROOT,
                             capture_output=True, check=True).stdout
        old_source = old
        old_scope_quotes = {'PHI': 0, 'PAN': 0}
        quote_old = getattr(self.args, 'quote_old_country_scopes', False)
        if quote_old:
            old, old_scope_quotes = quote_country_scope_keys(old)
        self.record['old_event_source'] = dict(sha256=sha_bytes(old_source), effective_sha256=sha_bytes(old),
                                              quote_country_scopes=quote_old, replacements=old_scope_quotes,
                                              replacement_count=sum(old_scope_quotes.values()))
        new_path = safe_path(self.args.new_events.absolute(), work, regular=True)
        new = new_path.read_bytes()
        if b'pop_majority_religion' in new:
            from tools.culture_religion.validate import validate as validate_pairs
            require(model_inputs(self.game) == model_inputs(new_path.parents[1]),
                    'Данные модели новой генерации отличаются от игрового стенда')
            pair_result = validate_pairs(new_path.parents[1])
            validation = dict(ok=pair_result['ok'], issues=pair_result['errors'],
                              counts=dict(first=pair_result['first_count'], spread=pair_result['spread_count']),
                              checked_events=pair_result['first_count'] + pair_result['spread_count'])
        else:
            model = reconstruct_map(self.game)
            validation = validate_events(parse(new.decode('cp1251')),
                                         (new_path.parents[1] / 'localisation/adding_cores.csv').read_bytes().decode('cp1251'), model)
        require(validation['ok'], f'Новый выход не прошёл валидатор MTTH4*n: {validation["issues"][:10]}')
        require(old != new, 'Одинаковые наборы событий')
        self.events = {'old': old, 'new': new}
        self.root_ini_hashes = {name: cli.digest(ROOT / name) for name in (CORE_INI, GPC_INI)}
        self.record.update(source_commit=revision, new_event_path=str(new_path), saves=self.save_hashes,
                           event_sets={name: {'sha256': sha_bytes(data), 'events': len(parse(data.decode('cp1251')))}
                                       for name, data in self.events.items()},
                           new_validation={key: validation[key] for key in ('ok', 'counts', 'checked_events')},
                           original_files={name: sha_bytes(data) for name, data in self.originals.items()},
                           root_ini_hashes=self.root_ini_hashes,
                           tool_hashes={str(path.relative_to(ROOT)): cli.digest(path) for path in (
                               Path(__file__), Path(cli.__file__), Path(mp_preflight.__file__),
                               ROOT / 'tools/vic2test/benchmark_gpc.py', ROOT / 'tools/vic2test/mp_session.py',
                               Path(native_event_probe.__file__), Path(native_event_probe.engine.__file__),
                               Path(sys.modules[Spans.__module__].__file__),
                               Path(sys.modules[wait_initialisation.__module__].__file__))})

    def verify_saves(self):
        require({name: cli.digest(path) for name, path in self.save_paths.items()} == self.save_hashes,
                'Изменилась входная копия сейва')

    def verify_inputs(self, event_bytes=None):
        current = input_fingerprint(self.environment)
        replacements = {} if event_bytes is None else {'game/' + EVENT: event_bytes, 'game/' + GPC_INI: self.disabled_gpc}
        delta = differences(self.baseline, current, replacements)
        if delta:
            self.record['unexpected_input_differences'] = delta
        require(not delta, 'Изменились входы помимо разрешённого файла событий/одинакового GPC off')
        self.verify_saves()
        return compact(current)

    def advance(self, days, record):
        command = [sys.executable, '-B', str(Path(cli.__file__)), 'advance', '--environment', str(self.environment),
                   '--days', str(days), '--timeout', '600', '--auto-events']
        record['command'] = command
        completed = subprocess.run(command, capture_output=True, encoding='utf-8',
                                   env=dict(os.environ, PYTHONIOENCODING='utf-8'), timeout=690, check=False)
        record.update(returncode=completed.returncode, stdout=completed.stdout, stderr=completed.stderr)
        response = json.loads(completed.stdout)
        checked(response, 'advance')
        require(completed.returncode == 0 and response.get('completed') is True, 'Не завершён advance')
        return response

    def state(self, process):
        status = checked(cli.request(self.environment, 'status', 30), 'status')
        gpc = checked(cli.request(self.environment, 'culture-cores-status', 30), 'GPC status')
        require(status.get('pid') == process.pid and status.get('multiplayer') is False and
                status.get('campaign_loaded') is True and status.get('paused') is True and
                status.get('advance_target') == 0, 'Не подтверждена пауза собственной SP-кампании')
        require(type(status.get('date_raw')) is int and type(status.get('auto_events_selected')) is int,
                'Неверный протокол даты/опций')
        total, random = status.get('all_province_events'), status.get('random_province_events')
        require(type(total) is int and type(random) is int and 0 < random <= total,
                'Не подтверждён заполненный нативный реестр случайных провинциальных событий')
        require(gpc.get('installed') is True and gpc.get('enabled') == 0 and
                gpc.get('failed') == 0 and gpc.get('busy') == 0, 'GPC не отключён или неисправен')
        return status

    def rebel_control(self, process, save):
        if save != self.args.rebel_save:
            return None
        probe = native_event_probe.capture(self.environment, session(self.environment), process,
                                          self.args.rebel_province, 24001)
        require(probe['complete'], 'Не прочитано состояние повстанческой провинции')
        province = probe['province']
        require(province['owner'] == 'PRU' and province['controller'] == 'REB',
                'Испытательная провинция больше не контролируется повстанцами')
        return {key: province[key] for key in ('id', 'owner', 'controller')} | {
            'date_raw': probe['world']['date_raw'], 'cache_count': province['candidate_vector']['count']}

    def run_mode(self, name):
        require_idle(self.environment)
        mode = dict(name=name, samples=[], complete=False)
        self.record['modes'].append(mode)
        (self.game / EVENT).write_bytes(self.events[name])
        (self.game / GPC_INI).write_bytes(self.disabled_gpc)
        mode['inputs_before'] = self.verify_inputs(self.events[name])
        previous, owned = session(self.environment), None
        self.checkpoint(name + ':start')
        try:
            started_at = time.time()
            try:
                mode['start'] = cli.start(self.environment, hidden=not self.args.visible)  # load: отдельный timeout1200.
            finally:
                current = session(self.environment)
                if current and current.get('token') != (previous or {}).get('token'):
                    require(re.fullmatch('[a-f0-9]{32}', current.get('token', '')) and
                            type(current.get('pid')) is int and current['pid'] > 0 and
                            Path(current.get('game', '')).resolve() == self.game.resolve(), 'Не подтверждена новая session')
                    mode['session'] = current
                    try:
                        owned = OwnedProcess(current['pid'], self.exe)
                    except BaseException:
                        mode['cleanup_unconfirmed'] = True
                        raise
            checked(mode['start'], 'start')
            require(owned is not None, 'Не создан собственный процесс')
            mode['initialisation'] = dict(started_at=started_at, complete=False)
            self.checkpoint(name + ':initialisation')
            began = time.perf_counter()
            try:
                wait_initialisation(self.environment, started_at, STARTUP_TIMEOUT)
                mode['initialisation']['complete'] = True
            finally:
                mode['initialisation']['wait_seconds'] = time.perf_counter() - began
                self.checkpoint(name + ':initialisation-finished')
            for save in self.args.saves:
                for index in range(self.args.samples):
                    sample = dict(save=save, index=index + 1, complete=False, warmup={}, advance={})
                    if mode['samples']:
                        checked(cli.return_to_menu(self.environment, 1200), 'return-menu')
                    mode['samples'].append(sample)
                    self.checkpoint(f'{name}:{save}:{index + 1}:load')
                    self.verify_saves()
                    checked(cli.load_campaign(self.environment, save, 1200), 'load')
                    require(session(self.environment)['token'] == mode['session']['token'], 'Сменилась session')
                    if not self.args.visible:
                        cli.hide_windows(owned.pid)
                    self.advance(self.args.warmup_days, sample['warmup'])
                    start = self.state(owned)
                    sample['rebel_control_start'] = self.rebel_control(owned, save)
                    self.dates.setdefault(save, start['date_raw'])
                    require(start['date_raw'] == self.dates[save], 'Неодинаковый начальный календарный интервал')
                    sample.update(start=start, cpu_start=process_times(owned))
                    self.checkpoint(f'{name}:{save}:{index + 1}:advance')
                    began = time.perf_counter()
                    try:
                        advanced = self.advance(self.args.days, sample['advance'])
                    finally:
                        sample['wall_seconds'] = time.perf_counter() - began
                    sample['cpu_end'] = process_times(owned)
                    end = self.state(owned)
                    sample['rebel_control_end'] = self.rebel_control(owned, save)
                    require(end['date_raw'] - start['date_raw'] == self.args.days * 24 and
                            advanced.get('date_raw') == end['date_raw'] and
                            advanced.get('requested_target_date_raw') == end['date_raw'], 'Неверный интервал advance')
                    require(sample['cpu_start']['creation_ticks'] == sample['cpu_end']['creation_ticks'], 'Сменился процесс')
                    cpu = sum(sample['cpu_end'][key] - sample['cpu_start'][key] for key in ('kernel_seconds', 'user_seconds'))
                    require(cpu >= 0, 'Уменьшился CPU-счётчик')
                    sample.update(end=end, actual_days=self.args.days, cpu_seconds=cpu,
                                  cpu_seconds_per_day=cpu / self.args.days,
                                  wall_seconds_per_day=sample['wall_seconds'] / self.args.days,
                                  auto_event_choices=end['auto_events_selected'], complete=True)
                    self.verify_saves()
                    self.checkpoint(f'{name}:{save}:{index + 1}:complete')
                    print(f'{name} {save} {index + 1}/{self.args.samples}: {sample["wall_seconds"]:.3f} с, '
                          f'CPU {cpu:.3f} с, опций {sample["auto_event_choices"]}', file=sys.stderr, flush=True)
            mode['complete'] = True
        finally:
            if owned is not None:
                try:
                    owned.stop()
                    mode['process_stopped'] = True
                except BaseException as error:
                    self.record['errors'].append({'stage': 'stop', 'mode': name, 'error': str(error)})
            self.checkpoint(name + ':cleanup')
        require(mode.get('process_stopped') is True, 'Не подтверждено завершение процесса')
        mode['logs'] = {}
        for name_in_logs in ('v2dll.log', 'v2dll_oos.log'):
            source = self.game / 'z_CreatineCore/Logs' / name_in_logs
            if source.exists():
                data = safe_path(source, self.game, regular=True).read_bytes()
                target = self.artifacts / (name + '-' + name_in_logs)
                with target.open('xb') as stream:
                    stream.write(data)
                mode['logs'][name_in_logs] = {'path': str(target), 'sha256': sha_bytes(data)}
        mode['inputs_after'] = self.verify_inputs(self.events[name])

    def run(self):
        self.preflight()
        if self.args.dry_run:
            return {'ok': True, 'dry_run': True, **{key: self.record[key] for key in
                    ('parameters', 'source_commit', 'event_sets', 'old_event_source', 'new_validation', 'saves')}}, 0
        with self.output.open('x', encoding='utf-8', newline='\n') as self.stream:
            try:
                self.artifacts.mkdir(exist_ok=False)
                for name, data in self.originals.items():
                    (self.artifacts / Path(name).name).write_bytes(data)
                for name, data in self.events.items():
                    (self.artifacts / (name + '-adding_cores.txt')).write_bytes(data)
                cli.mute_test_settings(self.environment)
                self.baseline = input_fingerprint(self.environment)
                self.record['inputs_original'] = compact(self.baseline)
                self.checkpoint('prepared')
                for name in self.args.order.split('-'):
                    self.run_mode(name)
                require(not self.record['errors'], 'Есть ошибки завершения процесса')
                self.record.update(comparison=comparison(self.record['modes'], self.args.saves, self.args.samples),
                                   ok=True, complete=True)
            except BaseException as error:
                self.record.update(ok=False, complete=False)
                self.record['errors'].append({'stage': self.record.get('stage'), 'type': type(error).__name__, 'error': str(error)})
            finally:
                restored = {}
                for name, data in self.originals.items():
                    try:
                        path = safe_path(self.game / name, self.game, regular=True)
                        require(path.stat().st_nlink == 1, 'Появилась жёсткая ссылка')
                        path.write_bytes(data)
                        require(path.read_bytes() == data, 'Не восстановлены исходные байты')
                        restored[name] = sha_bytes(data)
                    except BaseException as error:
                        self.record['errors'].append({'stage': 'restore', 'file': name, 'error': str(error)})
                self.record['restored_files'] = restored
                try:
                    self.verify_saves()
                    require({name: cli.digest(ROOT / name) for name in self.root_ini_hashes} == self.root_ini_hashes,
                            'Изменились INI основной сборки')
                    if self.baseline:
                        self.record['inputs_restored'] = self.verify_inputs()
                except BaseException as error:
                    self.record['errors'].append({'stage': 'final_integrity', 'error': str(error)})
                if self.record['errors']:
                    self.record.update(ok=False, complete=False)
                self.record.update(finished_utc=utc(), boundary_uncertainty='GPC off в обоих режимах; конец дневной фазы не квалифицируется')
                self.checkpoint('finished')
        return {key: self.record[key] for key in ('ok', 'complete', 'errors', 'event_sets')}, 0 if self.record['ok'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, default=ROOT / '.work/chronology-cores/engine')
    parser.add_argument('--old-commit', default=OLD_COMMIT, help='Закреплённая контрольная ревизия событий')
    parser.add_argument('--quote-old-country-scopes', action='store_true',
                        help='Явно экранировать PHI/PAN scope keys старого набора без изменения механики')
    parser.add_argument('--new-events', type=Path, default=ROOT / '.work/chronology-cores/generated-game/events/adding_cores.txt')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--saves', nargs=2, default=['gpc36_start.v2', 'chr_new.v2'])
    parser.add_argument('--days', type=int, default=100)
    parser.add_argument('--samples', type=int, default=3)
    parser.add_argument('--warmup-days', type=int, default=33,
                        help='Одинаковый прогрев до замера;33 дня включают следующую месячную перестройку')
    parser.add_argument('--rebel-save', help='Для этого сейва проверять PRU/REB в начале и конце каждой выборки')
    parser.add_argument('--rebel-province', type=int, default=546)
    parser.add_argument('--order', choices=('old-new', 'new-old'), default='old-new')
    parser.add_argument('--visible', action='store_true', help='Явно оставить собственное тестовое окно видимым; ввод остаётся через CLI')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    try:
        require(1 <= args.days <= 365 and 3 <= args.samples <= 9, 'Нужны 1..365 дней и 3..9 выборок')
        require(1 <= args.warmup_days <= 365, 'Нужны 1..365 дней прогрева')
        result, code = Benchmark(args).run()
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return code
    except (OSError, ValueError, BenchmarkError, subprocess.SubprocessError) as error:
        print(json.dumps({'ok': False, 'error': str(error)}, ensure_ascii=False), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
