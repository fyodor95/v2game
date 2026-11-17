"""Подготовка фикстур и явный запуск приёмки CoreAddingEvents главным агентом."""
from __future__ import annotations

import argparse
from collections import Counter
from decimal import Decimal
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/creatinecore/gpc'))
sys.path.insert(0, str(ROOT / 'tools/vic2test'))
from make_fixture import Spans, scalar
from save_oracle import identifiers
import cli
from mp_session import OwnedProcess


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def work_path(path):
    path = Path(path).resolve()
    require(path.is_relative_to((ROOT / '.work').resolve()), 'Путь должен находиться внутри .work')
    return path


def one(node, key):
    result = node.single(key)
    require(result is not None, f'Нет поля {key}')
    return result


def find_events(raw, dynamic_culture=None):
    """Извлечь исходные блоки нового генератора без повторной сериализации."""
    text = raw.decode('cp1251')
    matches = {}
    dynamic = []
    for event in Spans(text).roots():
        if event.key != 'province_event':
            continue
        option = one(event, 'option')
        guard = option.single('if')
        if guard is None:
            continue
        tag = scalar(guard, 'add_core')
        trigger = one(event, 'trigger')
        culture = scalar(trigger, 'culture')
        limit = one(guard, 'limit')
        require(text[trigger.start:trigger.end] == text[limit.start:limit.end] or
                re.sub(r'\s+', '', text[trigger.start:trigger.end]) ==
                re.sub(r'\s+', '', text[limit.start:limit.end]), 'Guard отличается от trigger')
        record = {'id': int(scalar(event, 'id')), 'tag': tag, 'culture': culture,
                  'trigger': text[trigger.start:trigger.end]}
        if (tag, culture) in (('OBO', 'north_german'), ('SAX', 'north_german'), ('GER', 'south_german')):
            require((tag, culture) not in matches, 'Повторная пара первого события')
            matches[tag, culture] = record
        if tag and re.fullmatch(r'D\d\d', tag):
            dynamic.append(record)
    require(len(matches) == 3 and dynamic, 'Нет новых первых событий OBO/SAX/GER/D; генерация ещё не завершена')
    if dynamic_culture is not None:
        dynamic = [record for record in dynamic if record['culture'] == dynamic_culture]
        require(len(dynamic) == 1, 'Не найдено единственное D-событие культуры ' + dynamic_culture)
    return {'new': matches['OBO', 'north_german'], 'old': matches['SAX', 'north_german'],
            'union': matches['GER', 'south_german'], 'dynamic': dynamic[0]}


def inspect_source(raw, game_root):
    text = raw.decode('cp1251')
    cultures = identifiers(game_root / 'common/cultures.txt')
    religions = identifiers(game_root / 'common/religion.txt')
    types = {p.stem for p in (game_root / 'poptypes').glob('*.txt')}
    provinces, sax, selected, foreign = {}, None, None, None
    player = None
    for node in Spans(text).roots():
        if node.key == 'player':
            player = node.value
        if node.key == 'SAX':
            sax = node
        if not node.key or not node.key.isdigit() or not isinstance(node.value, list):
            continue
        owner = scalar(node, 'owner')
        if not owner:
            continue
        info = {'node': node, 'owner': owner, 'controller': scalar(node, 'controller')}
        # Достаточно POP только одной испытательной провинции; остальные AST не удерживаются.
        pops = []
        if selected is None and owner == 'PRU':
            for pop in node.value:
                if pop.key in types and isinstance(pop.value, list):
                    fields = [n for n in pop.value if n.key in cultures and n.value in religions]
                    require(len(fields) == 1, f'Неоднозначная культура POP: {node.key}')
                    pops.append(fields[0])
            if pops:
                selected = int(node.key)
                info['pop_cultures'] = pops
        if foreign is None and owner not in ('PRU', 'SAX', 'REB'):
            foreign = int(node.key)
        info['fields'] = {key: node.single(key) for key in ('owner', 'controller')}
        info['cores'] = node.fields('core')
        info['end'] = node.end
        del info['node']
        provinces[int(node.key)] = info
    require(player == 'PRU', 'Нужен исходный сейв игрока PRU')
    require(selected is not None and foreign is not None and sax is not None, 'Нет PRU/SAX/чужой провинции')
    return text, provinces, selected, foreign, sax


def fixture_cases(events):
    result = []
    for key in ('new', 'old', 'union', 'dynamic'):
        result.append({'name': key, 'event': key, 'allow': True})
    for name in ('controller', 'culture', 'exists', 'foreign', 'accepted', 'guard'):
        result.append({'name': name, 'event': 'old' if name in ('exists', 'accepted') else 'new',
                       'allow': name == 'guard'})
    for case in result:
        case['save'] = 'chr_' + case['name'] + '.v2'
        case['tag'] = events[case['event']]['tag']
        case['culture'] = events[case['event']]['culture']
    return result


def make_save(text, provinces, selected, foreign, sax, case, tags):
    edits = []
    def replace(field, value):
        require(field is not None, 'Отсутствует обязательное поле фикстуры')
        edits.append((field.start, field.end, value))
    for number, province in provinces.items():
        for core in province['cores']:
            if core.value in tags:
                edits.append((core.key_start, core.end, ''))
        if province['owner'] == 'SAX' and case['name'] != 'exists':
            replace(province['fields']['owner'], '"PRU"')
            if province['controller'] == 'SAX':
                replace(province['fields']['controller'], '"PRU"')
        if number == selected:
            replace(province['fields']['controller'], '"PRU"' if case['name'] == 'controller' else '"REB"')
            culture = 'south_german' if case['name'] == 'culture' else case['culture']
            for field in province['pop_cultures']:
                edits.append((field.key_start, field.key_end, culture))
        if number == foreign and case['name'] == 'foreign':
            edits.append((province['end'] - 1, province['end'] - 1, '\n\tcore="OBO"\n'))
    if case['name'] == 'accepted':
        replace(one(sax, 'primary_culture'), '"south_german"')
        accepted = one(sax, 'culture')
        replace(accepted, '{ "north_german" }')
    previous = len(text) + 1
    output = text
    for start, end, value in sorted(edits, reverse=True):
        require(end <= previous and 0 <= start <= end <= len(text), 'Пересекающиеся замены фикстуры')
        output = output[:start] + value + output[end:]
        previous = start
    return output.encode('cp1251'), len(edits)


def prepare(game_root, source_save, output, dynamic_culture=None):
    game_root, source_save, output = Path(game_root).resolve(), Path(source_save).resolve(), work_path(output)
    require(not output.exists(), 'Каталог результата уже существует')
    event_raw = (game_root / 'events/adding_cores.txt').read_bytes()
    events = find_events(event_raw, dynamic_culture)
    raw = source_save.read_bytes()
    text, provinces, selected, foreign, sax = inspect_source(raw, game_root)
    cases = fixture_cases(events)
    tags = {entry['tag'] for entry in events.values()}
    require(len(tags) == 4, 'Нужны четыре разных получателя')
    decisions = ['political_decisions = {']
    loc = []
    for key, entry in events.items():
        decision = 'chr_check_' + key
        entry['decision'] = decision
        decisions.append(f'{decision} = {{ potential = {{ tag = PRU }} allow = {{ {selected} = '
                         f'{entry["trigger"]} }} effect = {{ }} ai_will_do = {{ factor = 0 }} }}')
        loc.append(f'{decision};Проверка первой корки {entry["tag"]};;;;;;;;;;;;;x')
    decisions += [f'chr_guard_mutation = {{ potential = {{ tag = PRU }} allow = {{ always = yes }} '
                  f'effect = {{ {foreign} = {{ add_core = OBO }} }} ai_will_do = {{ factor = 0 }} }}',
                  f'chr_foreign_core = {{ potential = {{ tag = PRU }} allow = {{ {foreign} = {{ is_core = OBO }} }} '
                  'effect = { } ai_will_do = { factor = 0 } }',
                  'chr_barrier_set = { potential = { tag = PRU } allow = { always = yes } '
                  'effect = { set_country_flag = chr_test_barrier } ai_will_do = { factor = 0 } }',
                  'chr_barrier_clear = { potential = { tag = PRU } allow = { always = yes } '
                  'effect = { clr_country_flag = chr_test_barrier } ai_will_do = { factor = 0 } }',
                  'chr_barrier_check = { potential = { tag = PRU } allow = { has_country_flag = chr_test_barrier } '
                  'effect = { } ai_will_do = { factor = 0 } }', '}']
    loc += ['chr_guard_mutation;Проверка изменения условий;;;;;;;;;;;;;x',
            'chr_foreign_core;Проверка корки в чужой провинции;;;;;;;;;;;;;x',
            'chr_barrier_set;Маркер обработки команды;;;;;;;;;;;;;x',
            'chr_barrier_clear;Сброс маркера обработки команды;;;;;;;;;;;;;x',
            'chr_barrier_check;Проверка маркера обработки команды;;;;;;;;;;;;;x']
    outputs = {'decisions/chronology_core_test.txt': ('\r\n'.join(decisions) + '\r\n').encode('cp1251'),
               'localisation/chronology_core_test.csv': ('\r\n'.join(loc) + '\r\n').encode('cp1251')}
    manifest = {'source_save': str(source_save), 'source_sha256': sha(raw), 'game_root': str(game_root),
                'adding_cores_sha256': sha(event_raw), 'province': selected, 'foreign_province': foreign,
                'foreign_owner': provinces[foreign]['owner'], 'events': events, 'cases': cases, 'files': {}}
    if dynamic_culture is not None:
        manifest['requested_dynamic_culture'] = dynamic_culture
    # Все варианты строятся в памяти до появления выходного каталога.
    for case in cases:
        data, edits = make_save(text, provinces, selected, foreign, sax, case, tags)
        outputs['save games/' + case['save']] = data
        case['edits'] = edits
    require(source_save.read_bytes() == raw, 'Исходный сейв изменился во время подготовки')
    output.mkdir(parents=True)
    for name, data in outputs.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(data)
        manifest['files'][name] = {'sha256': sha(data), 'bytes': len(data)}
    with (output / 'manifest.json').open('x', encoding='utf-8') as stream:
        json.dump(manifest, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    return {'ok': True, 'fixtures': str(output), 'province': selected, 'foreign': foreign, 'cases': len(cases)}


def core_pairs(path):
    result = set()
    for node in Spans(path.read_bytes().decode('cp1251')).roots():
        if node.key and node.key.isdigit() and isinstance(node.value, list):
            fields = node.fields('core')
            require(len(fields) == len({f.value for f in fields}), 'Повторная корка в сохранении')
            result.update((int(node.key), field.value) for field in fields)
    return result


def diagnostic_files(manifest):
    """Отдельные булевы операнды с исходной вложенностью province/owner/TAG."""
    decisions, localisation, probes = ['political_decisions = {'], [], {}
    province = manifest['province']
    for key, event in manifest['events'].items():
        tag, culture = event['tag'], event['culture']
        country_scope = lambda condition: f'owner = {{ {tag} = {{ {condition} }} }}'
        parts = {
            'full': ('Полное условие', event['trigger']),
            'rebels': ('Контроль повстанцев', '{ controlled_by_rebels = yes }'),
            'largest': ('Крупнейшая культура', f'{{ culture = {culture} }}'),
            'local_no_core': ('Нет корки в провинции', f'{{ NOT = {{ is_core = {tag} }} }}'),
            'owner_differs': ('Владелец отличается от получателя', f'{{ owner = {{ NOT = {{ tag = {tag} }} }} }}'),
            'scope': ('Доступен scope получателя', '{ ' + country_scope('always = yes') + ' }'),
            'not_exists': ('Получатель не существует: вложенное условие', '{ ' + country_scope('exists = no') + ' }'),
            'no_any_core': ('У получателя нет корок', '{ ' + country_scope('NOT = { any_core = { always = yes } }') + ' }'),
            'primary': ('Основная культура получателя', '{ ' + country_scope(f'primary_culture = {culture}') + ' }'),
            'accepted': ('Признанная культура получателя', '{ ' + country_scope(f'accepted_culture = {culture}') + ' }'),
            'exists_external': ('Получатель не существует: внешнее условие', f'{{ owner = {{ NOT = {{ exists = {tag} }} }} }}'),
        }
        probes[key] = {}
        for name, (label, body) in parts.items():
            decision = f'chr_diag_{key}_{name}'
            decisions.append(f'{decision} = {{ potential = {{ tag = PRU }} allow = {{ {province} = {body} }} '
                             'effect = { } ai_will_do = { factor = 0 } }')
            localisation.append(f'{decision};{label}: {tag};;;;;;;;;;;;;x')
            probes[key][name] = {'decision': decision, 'label': label}
    decisions.append('}')
    return ({'decisions/chronology_core_diagnostics.txt': ('\r\n'.join(decisions) + '\r\n').encode('cp1251'),
             'localisation/chronology_core_diagnostics.csv': ('\r\n'.join(localisation) + '\r\n').encode('cp1251')}, probes)


def prepare_diagnostics(fixtures, output):
    fixtures, output = work_path(fixtures), work_path(output)
    require(not output.exists(), 'Выходной каталог диагностики уже существует')
    raw = (fixtures / 'manifest.json').read_bytes()
    manifest = json.loads(raw.decode('utf-8'))
    outputs, probes = diagnostic_files(manifest)
    metadata = {'fixture_manifest_sha256': sha(raw), 'adding_cores_sha256': manifest['adding_cores_sha256'],
                'probes': probes, 'files': {name: {'sha256': sha(data), 'bytes': len(data)} for name, data in outputs.items()}}
    output.mkdir(parents=True)
    for name, data in outputs.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(data)
    with (output / 'manifest.json').open('x', encoding='utf-8') as stream:
        json.dump(metadata, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    return {'ok': True, 'diagnostics': str(output), 'decisions': sum(map(len, probes.values()))}


def saved_diagnostic_state(path, game, province, tag):
    cultures, religions = identifiers(game / 'common/cultures.txt'), identifiers(game / 'common/religion.txt')
    types = {p.stem for p in (game / 'poptypes').glob('*.txt')}
    result = {'country_block_present': False, 'primary_culture': None, 'accepted_cultures': [],
              'owned_provinces': [], 'core_provinces': [], 'province': None}
    for node in Spans(path.read_bytes().decode('cp1251')).roots():
        if node.key == tag and isinstance(node.value, list):
            result['country_block_present'] = True
            result['primary_culture'] = scalar(node, 'primary_culture')
            accepted = node.single('culture')
            result['accepted_cultures'] = sorted(field.value for field in accepted.value) if accepted else []
        if not node.key or not node.key.isdigit() or not isinstance(node.value, list):
            continue
        number = int(node.key)
        if scalar(node, 'owner') == tag:
            result['owned_provinces'].append(number)
        if any(field.value == tag for field in node.fields('core')):
            result['core_provinces'].append(number)
        if number == province:
            totals = Counter()
            for pop in node.value:
                if pop.key in types and isinstance(pop.value, list):
                    matches = [field for field in pop.value if field.key in cultures and field.value in religions]
                    require(len(matches) == 1, 'Неоднозначная культура POP диагностического сейва')
                    totals[matches[0].key] += Decimal(scalar(pop, 'size', '0'))
            maximum = max(totals.values(), default=0)
            result['province'] = {'id': number, 'owner': scalar(node, 'owner'),
                                  'controller': scalar(node, 'controller'),
                                  'cores': sorted(field.value for field in node.fields('core')),
                                  'population': {key: str(size) for key, size in sorted(totals.items())},
                                  'largest_cultures': sorted(key for key, size in totals.items() if size == maximum)}
    return result


class Runner:
    def __init__(self, environment, fixtures, stream, only, diagnostics=None, visible=False):
        self.environment, self.fixtures = work_path(environment), work_path(fixtures)
        self.stream, self.only = stream, only
        self.diagnostics = work_path(diagnostics) if diagnostics else None
        self.visible = visible
        self.record = {'ok': False, 'complete': False, 'steps': [], 'cases': [], 'errors': [],
                       'save_prefix': 'c' + uuid.uuid4().hex[:4], 'visible_window_requested': visible}
        self.owned = None

    def diagnostic_case(self, case, event, manifest, game, record, prefix, delay=10):
        record['samples'] = []
        for index in range(3):
            if index:
                time.sleep(delay)
            # Сохранение идёт раньше первой проверки allow; ложный операнд не обрывает сбор данных.
            saved = self.save(prefix + f'_d{index}')
            sample = {'save': str(saved), 'sha256': cli.digest(saved),
                      'saved_state': saved_diagnostic_state(saved, game, manifest['province'], case['tag']),
                      'status': self.request('status'), 'conditions': {}}
            record['samples'].append(sample)
            self.checkpoint()
            for name, probe in self.record['diagnostics']['probes'][case['event']].items():
                response = self.request('decision-check PRU ' + probe['decision'])
                require(response.get('potential') is True and type(response.get('allow')) is bool,
                        'Не вычислено диагностическое решение')
                sample['conditions'][name] = response['allow']
                self.checkpoint()
            sample['failed_conditions'] = sorted(name for name, value in sample['conditions'].items() if not value)
            # Для юниона и D primary/accepted — дополнительные наблюдения, не обязательные операнды.
            sample['full_condition'] = sample['conditions']['full']
        first = record['samples'][0]
        record['observations_stable'] = all(sample['saved_state'] == first['saved_state'] and
                                            sample['conditions'] == first['conditions'] and
                                            sample['status'].get('date_raw') == first['status'].get('date_raw')
                                            for sample in record['samples'][1:])
        record['complete'] = True
        self.checkpoint()

    def checkpoint(self):
        self.stream.seek(0)
        json.dump(self.record, self.stream, ensure_ascii=False, indent=2)
        self.stream.write('\n')
        self.stream.truncate()
        self.stream.flush()

    def checked(self, response, operation):
        self.record['steps'].append({'operation': operation, 'response': response})
        self.checkpoint()
        require(response.get('ok') is True, f'{operation}: {response}')
        return response

    def request(self, command, timeout=60):
        return self.checked(cli.request(self.environment, command, timeout), command)

    def wait(self, command):
        return self.checked(cli.wait_request(self.environment, command, 60), command)

    def decision(self, key, expected):
        result = self.request('decision-check PRU ' + key)
        require(result.get('potential') is True and result.get('allow') is expected,
                f'Неожиданный результат {key}: ожидался allow={expected}')

    def save(self, name):
        path = self.environment / 'userdata/save games' / (name + '.v2')
        require(not path.exists(), 'Выходной сейв уже существует')
        self.request('save-native ' + name, 180)
        require(path.is_file() and path.stat().st_size > 0, 'Не создан сейв')
        self.record['steps'].append({'save': str(path), 'sha256': cli.digest(path)})
        self.checkpoint()
        return path

    def load(self, save):
        self.checked(cli.return_to_menu(self.environment, 180), 'return-menu')
        self.checked(cli.load_campaign(self.environment, save, 1200), 'load ' + save)

    def settle(self):
        # Отдельное штатное решение ставится после выбранной опции в очередь.
        # Наблюдаемый флаг подтверждает обработку следующей команды, не queued=true.
        previous = self.request('decision-check PRU chr_barrier_check')['allow']
        self.request('decision-execute PRU chr_barrier_' + ('clear' if previous else 'set'))
        deadline = time.monotonic() + 30
        while True:
            current = self.request('decision-check PRU chr_barrier_check')['allow']
            if current is not previous:
                break
            require(time.monotonic() < deadline, 'Не обработан маркер очереди команд')
            time.sleep(.1)
        require(self.request('status').get('event_windows') == 0, 'Окно события не закрыто')

    def fire(self, event_id, province):
        self.request(f'event-fire-province-chosen {province} {event_id}')
        self.wait(f'event-select-province {province} 0')
        self.settle()

    def preflight(self):
        require(os.name == 'nt', 'Запуск движка требует Windows')
        manifest = json.loads((self.fixtures / 'manifest.json').read_text(encoding='utf-8'))
        metadata = json.loads((self.environment / 'environment.json').read_text(encoding='utf-8'))
        game, user = self.environment / 'game', self.environment / 'userdata'
        require(Path(metadata['game']).resolve() == game and Path(metadata['userdata']).resolve() == user,
                'Некорректные пути изолированной среды')
        require(Path((game / 'userdir.txt').read_text(encoding='ascii').strip()).resolve() == user,
                'userdir не перенаправлен в тестовую среду')
        require(cli.digest(game / 'events/adding_cores.txt') == manifest['adding_cores_sha256'],
                'В стенде другой adding_cores')
        require((game / 'z_CreatineCore/dll_Submods/vic2_test_bridge.dll').is_file(), 'Не установлен тестовый мост')
        for directory in (game, user):
            settings = directory / 'settings.txt'
            if settings.exists():
                require(settings.resolve().is_relative_to(self.environment) and settings.stat().st_nlink == 1,
                        'Настройки не изолированы')
                require(re.search(rb'autosave\s*=\s*"NEVER"', settings.read_bytes()), 'Не отключено автосохранение')
        for name, expected in manifest['files'].items():
            target = user / name if name.startswith('save games/') else game / name
            require(target.resolve().is_relative_to(self.environment) and target.stat().st_nlink == 1,
                    'Фикстура выходит из среды или имеет жёсткие ссылки')
            require(cli.digest(target) == expected['sha256'], 'Фикстура не установлена или изменена: ' + name)
        if self.diagnostics:
            diagnostic = json.loads((self.diagnostics / 'manifest.json').read_text(encoding='utf-8'))
            require(diagnostic['fixture_manifest_sha256'] == cli.digest(self.fixtures / 'manifest.json') and
                    diagnostic['adding_cores_sha256'] == manifest['adding_cores_sha256'],
                    'Диагностика создана для другой фикстуры')
            for name, expected in diagnostic['files'].items():
                target = game / name
                require(target.resolve().is_relative_to(game) and target.stat().st_nlink == 1 and
                        cli.digest(target) == expected['sha256'], 'Не установлена диагностика: ' + name)
            self.record.update(diagnostics=diagnostic, diagnostic_only=True,
                               interpretation='Сбор наблюдений, не подтверждение работоспособности механики')
        session_path = self.environment / 'session.json'
        previous = json.loads(session_path.read_text(encoding='utf-8')) if session_path.exists() else None
        if previous:
            # До изменения INI отклоняем любую активную сессию. Handle только читается.
            import ctypes
            from ctypes import wintypes
            kernel = ctypes.WinDLL('kernel32', use_last_error=True)
            kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
            kernel.OpenProcess.restype = wintypes.HANDLE
            kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
            kernel.CloseHandle.argtypes = [wintypes.HANDLE]
            handle = kernel.OpenProcess(0x100000, False, previous['pid'])
            if handle:
                try:
                    require(kernel.WaitForSingleObject(handle, 0) == 0, 'Тестовая сессия уже работает')
                finally:
                    kernel.CloseHandle(handle)
            else:
                require(ctypes.get_last_error() == 87, 'Не удалось проверить завершение прежней сессии')
        self.record.update(manifest=manifest, metadata=metadata)
        self.record['tool_sources'] = {str(path.relative_to(ROOT)): cli.digest(path) for path in
                                      (Path(__file__), Path(cli.__file__), ROOT / 'tools/vic2test/mp_session.py',
                                       ROOT / 'tools/creatinecore/gpc/make_fixture.py')}
        self.checkpoint()
        return manifest, game, previous

    def run(self):
        config, original = None, None
        try:
            manifest, game, previous = self.preflight()
            cases = [c for c in manifest['cases'] if not self.only or c['name'] in self.only]
            require(cases and (not self.only or set(self.only) == {c['name'] for c in cases}), 'Неизвестные случаи')
            config = game / 'z_CreatineCore/great_power_culture_cores.ini'
            require(config.resolve().is_relative_to(self.environment) and config.stat().st_nlink == 1,
                    'INI не изолирована')
            original = config.read_bytes()
            disabled, count = re.subn(rb'(?m)^(\s*ENABLE_GREAT_POWER_CULTURE_CORES\s*=\s*)[01]', rb'\g<1>0', original)
            require(count == 1, 'Не найден единственный ENABLE_GREAT_POWER_CULTURE_CORES в GPC INI')
            self.record['gpc_ini'] = {'original_sha256': sha(original), 'test_sha256': sha(disabled)}
            config.write_bytes(disabled)
            try:
                started = cli.start(self.environment, hidden=not self.visible)
            finally:
                session_path = self.environment / 'session.json'
                current = json.loads(session_path.read_text(encoding='utf-8')) if session_path.exists() else None
                if current and current.get('token') != (previous or {}).get('token'):
                    require(Path(current['game']).resolve() == game and re.fullmatch('[0-9a-f]{32}', current['token']),
                            'Не подтверждена собственная session')
                    self.record['session'] = current
                    self.record['cleanup_unconfirmed'] = True
                    self.owned = OwnedProcess(current['pid'], game / 'v2game.exe')
                    self.record['cleanup_unconfirmed'] = False
            self.checked(started, 'start')
            require(self.owned is not None, 'Нет собственного процесса')
            self.checked(cli.load_campaign(self.environment, cases[0]['save'], 1200), 'initial-load')
            require(self.request('culture-cores-status').get('enabled') == 0, 'GPC стенда не выключен')
            for index, case in enumerate(cases):
                record = {'name': case['name'], 'complete': False}
                self.record['cases'].append(record)
                self.checkpoint()
                if index:
                    self.load(case['save'])
                status = self.request('status')
                require(status.get('paused') is True and status.get('multiplayer') is False, 'Нужна пауза SP')
                event = manifest['events'][case['event']]
                prefix = self.record['save_prefix'] + f'{index + 1:02d}'
                if self.diagnostics:
                    self.diagnostic_case(case, event, manifest, game, record, prefix)
                    continue
                before = core_pairs(self.save(prefix + '_before'))
                self.decision(event['decision'], case['allow'])
                expected_add = set()
                if case['name'] == 'guard':
                    self.request(f'event-fire-province-chosen {manifest["province"]} {event["id"]}')
                    self.request('decision-execute PRU chr_guard_mutation')
                    deadline = time.monotonic() + 30
                    while True:
                        result = self.request('decision-check PRU chr_foreign_core')
                        if result.get('allow') is True:
                            break
                        require(time.monotonic() < deadline, 'Не завершилась мутация guard')
                        time.sleep(.1)
                    self.decision(event['decision'], False)
                    self.wait(f'event-select-province {manifest["province"]} 0')
                    self.settle()
                    expected_add.add((manifest['foreign_province'], 'OBO'))
                else:
                    self.fire(event['id'], manifest['province'])
                    if case['allow']:
                        expected_add.add((manifest['province'], case['tag']))
                after_path = self.save(prefix + '_after')
                after = core_pairs(after_path)
                record.update(added=sorted(after - before), removed=sorted(before - after),
                              expected_added=sorted(expected_add))
                require(after - before == expected_add and not before - after, 'Неверное изменение корок')
                self.decision(event['decision'], False)
                if case['name'] == 'new':
                    self.fire(event['id'], manifest['province'])
                    require(core_pairs(self.save(prefix + '_repeat')) == after, 'Повтор изменил корки')
                    self.load(after_path.name)
                    require(core_pairs(self.save(prefix + '_reload')) == after, 'Корки изменились при загрузке')
                    self.decision(event['decision'], False)
                    record['repeat_and_reload'] = True
                current = self.request('status')
                require(current.get('date_raw') == status.get('date_raw') and current.get('paused') is True,
                        'Во время опыта изменились дата/пауза')
                record['complete'] = True
                self.checkpoint()
            self.record.update(ok=True, complete=True, subset=bool(self.only))
        except BaseException as error:
            self.record['errors'].append({'type': type(error).__name__, 'message': str(error)})
        finally:
            if self.owned:
                try:
                    self.owned.stop()
                    self.record['process_stopped'] = True
                except BaseException as error:
                    self.record.update(ok=False, complete=False)
                    self.record['errors'].append({'cleanup': str(error)})
            if original is not None:
                try:
                    config.write_bytes(original)
                    require(config.read_bytes() == original, 'INI не восстановлена')
                    self.record['ini_restored'] = True
                except BaseException as error:
                    self.record.update(ok=False, complete=False)
                    self.record['errors'].append({'restore_ini': str(error)})
            self.checkpoint()
        return self.record['ok']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    prep = sub.add_parser('prepare', help='Только новые фикстуры, без запуска игры')
    prep.add_argument('--game-root', required=True, type=Path)
    prep.add_argument('--source-save', required=True, type=Path)
    prep.add_argument('--output', required=True, type=Path)
    prep.add_argument('--dynamic-culture', help='Выбрать D-fallback конкретной культуры; по умолчанию первый в файле')
    diagnostic = sub.add_parser('prepare-diagnostics', help='Отдельный пакет булевых проверок существующей фикстуры')
    diagnostic.add_argument('--fixtures', required=True, type=Path)
    diagnostic.add_argument('--output', required=True, type=Path)
    run = sub.add_parser('run', help='Явный запуск главным агентом в подготовленной среде')
    run.add_argument('--environment', required=True, type=Path)
    run.add_argument('--fixtures', required=True, type=Path)
    run.add_argument('--output', required=True, type=Path)
    run.add_argument('--case', action='append', dest='cases')
    run.add_argument('--diagnostics', type=Path, help='Только диагностические наблюдения; без игровых эффектов')
    run.add_argument('--visible', action='store_true', help='По явному указанию root оставить тестовое окно видимым')
    args = parser.parse_args()
    if args.command == 'prepare':
        print(json.dumps(prepare(args.game_root, args.source_save, args.output, args.dynamic_culture), ensure_ascii=False))
        return 0
    if args.command == 'prepare-diagnostics':
        print(json.dumps(prepare_diagnostics(args.fixtures, args.output), ensure_ascii=False))
        return 0
    output = work_path(args.output)
    require(not output.is_relative_to(work_path(args.environment)), 'Отчёт должен быть вне тестовой среды')
    with output.open('x', encoding='utf-8') as stream:
        runner = Runner(args.environment, args.fixtures, stream, args.cases, args.diagnostics, args.visible)
        ok = runner.run()
    print(json.dumps({'ok': ok, 'report': str(output), 'errors': runner.record['errors']}, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
