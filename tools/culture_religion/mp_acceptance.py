"""Изолированный пакет и явный запуск сетевой приёмки культуры и религии."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct
import sys
import time
import traceback
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.chronology_import import engine_acceptance as engine
from tools.chronology_import import natural_spawn as old_natural
from tools.chronology_import import native_event_probe as probe
from tools.culture_religion import snapshot
from tools.culture_religion.native_acceptance import wait_initialisation
from mp_driver import Driver
from mp_preflight import compare

CORE_EVENT = 24001
DECISION_EVENT = 3997400
PROVINCE = 546
SAVE = 'cr_mp.v2'
DECISION_PATH = 'decisions/new_primary_culture_for_dxx.txt'
ADDING_PATH = 'events/adding_cores.txt'
EVENT_PATH = 'events/culture_religion_mp_test.txt'
LOC_PATH = 'localisation/culture_religion_mp_test.csv'
EXPECTED = {'D45': {'primary': 'aborigine', 'religion': 'animist'},
            'D50': {'primary': 'north_german', 'religion': 'catholic'}}
require = engine.require
sha = engine.sha


def edits(text, changes):
    previous, chunks = 0, []
    for start, end, replacement in sorted(changes):
        require(previous <= start <= end <= len(text),
                'Пересекающиеся замены фикстуры')
        chunks.extend((text[previous:start], replacement))
        previous = end
    chunks.append(text[previous:])
    return ''.join(chunks)


def accelerate(raw):
    """Сохранить эффекты/условия и все spread; изменить только участие first и MTTH."""
    text = raw.decode('cp1251')
    changes, disabled, selected = [], [], []
    for event in engine.Spans(text).roots():
        if event.key != 'province_event':
            continue
        option = engine.one(event, 'option')
        guard = option.single('if')
        if guard is None:
            continue
        number = int(engine.scalar(event, 'id'))
        trigger = engine.one(event, 'trigger')
        limit = engine.one(guard, 'limit')
        require(re.sub(r'\s+', '', text[trigger.start:trigger.end]) ==
                re.sub(r'\s+', '', text[limit.start:limit.end]), 'Повторная проверка отличается от trigger')
        if number == CORE_EVENT:
            require(engine.scalar(trigger, 'culture') == 'aborigine' and
                    engine.scalar(trigger, 'pop_majority_religion') == 'animist' and
                    engine.scalar(guard, 'add_core') == 'D45', 'ID24001 больше не соответствует D45/aborigine/animist')
            country = engine.one(engine.one(guard, 'owner'), 'D45')
            require(engine.scalar(country, 'primary_culture') == 'aborigine' and
                    engine.scalar(country, 'religion') == 'animist', 'Первая корка не назначает культуру и религию')
            require(event.single('is_triggered_only') is None, 'Целевое событие уже triggered-only')
            mtth = engine.one(event, 'mean_time_to_happen')
            changes.append((mtth.start, mtth.end, '{ days = 1 }'))
            selected.append({'id': number, 'tag': 'D45', 'culture': 'aborigine', 'religion': 'animist',
                             'trigger_sha256': sha(text[trigger.start:trigger.end].encode('cp1251')),
                             'option_sha256': sha(text[option.start:option.end].encode('cp1251'))})
        else:
            only = event.single('is_triggered_only')
            changes.append((only.start, only.end, 'yes') if only else
                           (event.start + 1, event.start + 1, '\r\n\tis_triggered_only = yes\r\n'))
            disabled.append(number)
    require(len(selected) == 1, 'Нет единственного целевого первого события')
    return edits(text, changes).encode('cp1251'), dict(event=selected[0], disabled_first_ids=disabled,
                                                     source_sha256=sha(raw), mtth_days=1)


def decision_event(raw):
    text = raw.decode('cp1251')
    matches = [decision for root in engine.Spans(text).roots() if root.key == 'political_decisions'
               for decision in root.value if decision.key == 'D_primary_north_german']
    require(len(matches) == 1, 'Нет единственного D_primary_north_german')
    effect = engine.one(matches[0], 'effect')
    require(engine.scalar(effect, 'primary_culture') == 'north_german' and
            engine.scalar(effect, 'set_religion_from_primary_culture') == 'yes', 'Решение не содержит новый метод')
    body = text[effect.start + 1:effect.end - 1]
    # Эффекты решения копируются побайтно; маркер добавляется после них.
    event = (f'country_event = {{\r\n\tid = {DECISION_EVENT}\r\n'
             '\tfire_only_once = yes\r\n'
             '\ttitle = "CR_MP_TITLE"\r\n\tdesc = "CR_MP_DESC"\r\n'
             '\ttrigger = { tag = D50 date = 1836.1.2 primary_culture = british '
             'NOT = { has_country_flag = cr_mp_done } }\r\n'
             '\tmean_time_to_happen = { days = 1 }\r\n'
             '\toption = {\r\n\t\tname = "CR_MP_OPTION"\r\n' + body +
             '\r\n\t\tset_country_flag = cr_mp_done\r\n\t}\r\n}\r\n').encode('cp1251')
    return event, dict(id=DECISION_EVENT, country='D50', decision='D_primary_north_german',
                       source_sha256=sha(raw), effects_sha256=sha(body.encode('cp1251')),
                       copied_effects=body, done_flag='cr_mp_done', fire_only_once=True)


def rebel_save(raw, game_root):
    """Править только новую MP-копию: POP546, контролёр, D45-корки и army.location."""
    text = raw.decode('cp1251')
    cultures = engine.identifiers(game_root / 'common/cultures.txt')
    religions = engine.identifiers(game_root / 'common/religion.txt')
    types = {p.stem for p in (game_root / 'poptypes').glob('*.txt')}
    require('aborigine' in cultures and 'animist' in religions and types, 'Нет словарей фикстуры')
    changes, removed, pops, selected, d45, d50, date, player = [], [], 0, 0, 0, 0, None, None
    initial_countries = {}
    for node in engine.Spans(text).roots():
        if node.key == 'date': date = node.value
        if node.key == 'player': player = node.value
        if node.key == 'D45':
            d45 += 1
            initial_countries['D45'] = dict(primary=engine.scalar(node, 'primary_culture'), religion=engine.scalar(node, 'religion'))
            require(engine.scalar(node, 'primary_culture') != 'aborigine' or
                    engine.scalar(node, 'religion') != 'animist', 'D45 уже имеет целевую пару: нет доказательства изменения')
        if node.key == 'D50':
            d50 += 1
            initial_countries['D50'] = dict(primary=engine.scalar(node, 'primary_culture'), religion=engine.scalar(node, 'religion'))
            require(engine.scalar(node, 'primary_culture') == 'british' and
                    engine.scalar(node, 'religion') == 'jewish', 'Исходная D50 не british/jewish')
            require(not re.search(r'\bcr_mp_done\b', text[node.start:node.end]), 'Маркер события уже присутствует у D50')
        if not node.key or not node.key.isdigit() or not isinstance(node.value, list):
            continue
        require(engine.scalar(node, 'owner') != 'D45', 'D45 уже существует')
        for core in node.fields('core'):
            if core.value == 'D45':
                changes.append((core.key_start, core.end, ''))
                removed.append(int(node.key))
        if int(node.key) != PROVINCE:
            continue
        selected += 1
        require(engine.scalar(node, 'owner') == 'PRU', '546 должна оставаться PRU')
        controller = engine.one(node, 'controller')
        changes.append((controller.start, controller.end, '"REB"'))
        for pop in node.value:
            if pop.key not in types or not isinstance(pop.value, list): continue
            fields = [field for field in pop.value if field.key in cultures and field.value in religions]
            require(len(fields) == 1, 'Неоднозначная культура POP546')
            field = fields[0]
            changes.extend([(field.key_start, field.key_end, 'aborigine'),
                            (field.start, field.end, 'animist')])
            pops += 1
    require(date == '1836.1.1' and player == 'PRU', 'Нужен старт1836.1.1 с игроком PRU')
    require(selected == d45 == d50 == 1 and pops > 0, 'Нет единственных546/D45/D50 или POP')
    modified, armies = old_natural.relocate_prussian_armies(edits(text, changes).encode('cp1251'), PROVINCE)
    return modified, dict(province=PROVINCE, owner='PRU', controller='REB', pop_count=pops,
                          removed_core_provinces=removed, army_relocation=armies, initial_countries=initial_countries)


def prepare(game_root, source_save, output):
    from tools.culture_religion import decision_fixture
    game_root, source_save, output = Path(game_root).resolve(), Path(source_save).resolve(), engine.work_path(output)
    require(not output.exists(), 'Каталог пакета уже существует')
    sources = {str(source_save): source_save.read_bytes(),
               str(game_root / ADDING_PATH): (game_root / ADDING_PATH).read_bytes(),
               str(game_root / DECISION_PATH): (game_root / DECISION_PATH).read_bytes()}
    for path in (game_root / 'events').glob('*.txt'):
        require(not re.search(rb'\bid\s*=\s*' + str(DECISION_EVENT).encode() + rb'\b', path.read_bytes()),
                'ID тестового события уже занят: ' + str(path))
    base_outputs, base_manifest = decision_fixture.transform(sources[str(source_save)], game_root)
    saved, save_details = rebel_save(base_outputs['cr_mp_base.v2'], game_root)
    accelerated, core = accelerate(sources[str(game_root / ADDING_PATH)])
    country, decision = decision_event(sources[str(game_root / DECISION_PATH)])
    rows = {'CR_MP_TITLE': 'Сетевая проверка культуры и религии',
            'CR_MP_DESC': 'Проверка эффектов решения Д-страны на обоих участниках партии.',
            'CR_MP_OPTION': 'Назначить культуру и религию по населению'}
    loc = ''.join(key + ';' + value + ';' * 13 + 'x\r\n' for key, value in rows.items()).encode('cp1251')
    outputs = {'save games/' + SAVE: saved, ADDING_PATH: accelerated, EVENT_PATH: country, LOC_PATH: loc}
    sources.update({str(game_root / 'common/cultures.txt'): (game_root / 'common/cultures.txt').read_bytes(),
                    str(game_root / 'common/religion.txt'): (game_root / 'common/religion.txt').read_bytes(),
                    str(Path(__file__).resolve()): Path(__file__).read_bytes(),
                    str(Path(decision_fixture.__file__).resolve()): Path(decision_fixture.__file__).read_bytes()})
    sources.update({str(path): path.read_bytes() for path in sorted((game_root / 'poptypes').glob('*.txt'))})
    manifest = dict(schema=1, prepared_only=True, game_root=str(game_root), save=SAVE,
                    host_tag='PRU', client_tag='D50', province=PROVINCE,
                    expected=EXPECTED, base_fixture=base_manifest, rebel_fixture=save_details,
                    core_event=core, decision_event=decision,
                    sources={path: {'sha256': sha(data), 'bytes': len(data)} for path, data in sources.items()},
                    files={name: {'sha256': sha(data), 'bytes': len(data)} for name, data in outputs.items()})
    require(all(Path(path).read_bytes() == raw for path, raw in sources.items()), 'Исходники изменились при подготовке')
    output.mkdir(parents=True)
    for name, data in outputs.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as stream: stream.write(data)
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return manifest


def inspect_cores(read, base, province_id):
    """Layout world_reader.h: province+148 — vector<Identity{tag,index}>, шаг8."""
    def u32(at): return struct.unpack('<I', read(at, 4))[0]
    world, manager = u32(base + 0xe588e8), u32(base + 0xe587e4)
    require(world and manager, 'Нет мира или реестра стран')
    count, countries = u32(manager), u32(manager + 4)
    require(0 < count <= 65536 and countries, 'Неверный реестр стран')
    begin, end = struct.unpack('<II', read(world + 0xacc, 8))
    require(begin and end >= begin and (end - begin) % 4 == 0 and
            province_id > 0 and province_id < (end - begin) // 4 <= 100000, 'Неверный vector провинций')
    province = u32(begin + province_id * 4)
    require(province and u32(province + 0x58) == province_id, 'Идентичность провинции не совпала')

    def identity(at):
        packed, index = struct.unpack('<II', read(at, 8))
        require(0 < index < count, 'Неверный индекс страны в Identity')
        country = u32(countries + index * 4)
        require(country and read(country + 0x1c, 8) == struct.pack('<II', packed, index), 'Identity не соответствует реестру')
        tag = struct.pack('<I', packed)
        require(tag[3] == 0 and re.fullmatch(rb'[A-Z0-9]{3}', tag[:3]), 'Неверный тег Identity')
        return tag[:3].decode('ascii')

    owner, controller = identity(province + 0x128), identity(province + 0x130)
    first, last = struct.unpack('<II', read(province + 0x148, 8))
    require(last >= first and (last - first) % 8 == 0 and (last - first) // 8 <= count and
            (first or first == last), 'Неверный vector корок')
    cores = [identity(at) for at in range(first, last, 8)]
    require(len(cores) == len(set(cores)), 'Повторная корка в vector')
    return dict(province=province_id, owner=owner, controller=controller, cores=sorted(cores),
                date_raw=u32(world + 0xb0c))


def capture(environment, session, owned):
    result = snapshot.capture(environment, session, owned, ['D45', 'D50'])
    executable = (environment / 'game/v2game.exe').resolve()
    process = probe.ReadOnlyProcess(owned.pid, executable)
    try:
        base = process.main_module()['base']
        require(base == result['pe']['actual_base'], 'База процесса изменилась')
        result['province'] = inspect_cores(process.read, base, PROVINCE)
        require(result['province']['date_raw'] == result['date_raw'], 'Дата изменилась при чтении корок')
    finally:
        process.close()
    return result


def check_results(host, client, date):
    for role, result in (('host', host), ('client', client)):
        require(result['date_raw'] == date and result['province']['date_raw'] == date, 'Не совпала дата RPM: ' + role)
        for tag, expected in EXPECTED.items():
            require({key: result['countries'][tag][key] for key in expected} == expected,
                    'Не совпали культура/религия ' + role + '/' + tag)
        require('D45' in result['province']['cores'], 'Нет корки D45 в546: ' + role)
    require(host['province'] == client['province'], 'Провинция546 различается у участников')


class EventDriver(Driver):
    """Driver.advance с обслуживанием естественных окон в его read-only ожидании."""
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.advancing = False
        self.moving_requested = False
        self.handling_events = False
        self.selected = set()
        self.record['option_selections'] = []

    def drain(self):
        require(not self.handling_events, 'Повторный вход выбора опций')
        self.handling_events = True
        handled = False
        try:
            for role, environment, tag in (('host', self.host, 'PRU'), ('client', self.client, 'D50')):
                response = self.wait(environment, 'mp-events', lambda value: isinstance(value.get('events'), list))
                events = response['events']
                ids = [item['event_id'] for item in events]
                require(len(ids) == len(set(ids)), 'Неоднозначные окна с одним ID; опция не отправлена')
                for item in events:
                    number = item['event_id']
                    require(item['country'] == tag and item['options'] > 0, 'Чужое окно или нет опций')
                    if number in (CORE_EVENT, DECISION_EVENT):
                        require(role == ('host' if number == CORE_EVENT else 'client'), 'Целевое событие у неверного игрока')
                        require(number not in self.selected, 'Повторное целевое событие; мутация не повторена')
                        scope, province = ('province', PROVINCE) if number == CORE_EVENT else ('country', 0)
                        require(item.get('scope') == scope and item.get('province') == province,
                                'Нет квалифицированного scope/province целевого MP-окна')
                    # send допускает только квалифицированный отказ ДО enqueue;
                    # неизвестный результат немедленно завершает сценарий.
                    sent = self.send(environment, f'mp-option {number} 0')
                    require(sent.get('queued') is True and sent.get('event_id') == number and sent.get('option') == 0,
                            'Не подтверждена постановка опции; повтор запрещён')
                    if number in (CORE_EVENT, DECISION_EVENT):
                        require(sent.get('scope') == scope and sent.get('province') == province and
                                sent.get('country') == tag, 'Ответ mp-option не подтверждает целевой scope; повтор запрещён')
                    self.wait(environment, 'mp-events', lambda value: all(event['event_id'] != number for event in value.get('events', [])))
                    self.record['option_selections'].append(dict(role=role, event_id=number, option=0,
                                                                target=number in (CORE_EVENT, DECISION_EVENT)))
                    self.selected.add(number)
                    self.save()
                    handled = True
            return handled
        finally:
            self.handling_events = False

    def wait(self, environment, wire, predicate):
        def observe(state):
            if predicate(state): return True
            if (self.advancing and self.moving_requested and not self.handling_events and
                    environment == self.host and wire == 'mp-state' and state.get('paused') is True):
                if self.drain():
                    # Временно выключить наблюдателя во вложенном ожидании pause.
                    self.handling_events = True
                    try: self.pause(False)
                    finally: self.handling_events = False
            return False
        return super().wait(environment, wire, observe)

    def call(self, environment, wire, mutation=False, timeout=None):
        result = super().call(environment, wire, mutation=mutation, timeout=timeout)
        if environment == self.host and result.get('ok') and wire in ('mp-pause 0', 'mp-pause 1'):
            self.moving_requested = wire == 'mp-pause 0'
        return result

    def advance(self, days, speed=4):
        self.advancing = True
        try: return super().advance(days, speed)
        finally: self.advancing = False


def verify_package(package, host, client):
    package = engine.work_path(package)
    raw = (package / 'manifest.json').read_bytes()
    manifest = json.loads(raw)
    require(manifest.get('schema') == 1 and manifest.get('expected') == EXPECTED and manifest.get('save') == SAVE,
            'Неизвестная схема или ожидания пакета')
    require(set(manifest['files']) == {'save games/' + SAVE, ADDING_PATH, EVENT_PATH, LOC_PATH}, 'Неполный пакет')
    original_game = Path(manifest['game_root'])
    for name, expected in manifest['sources'].items():
        source = Path(name)
        if not source.is_relative_to(original_game): continue
        relative = source.relative_to(original_game)
        if relative.as_posix() in manifest['files']: continue
        # Фиксируем именно исходные definitions/решение, от которых зависит
        # пакет. Сейв и вспомогательные скрипты не являются входами game/.
        if relative.parts[0] not in ('common', 'decisions', 'poptypes'): continue
        for environment in (host, client):
            target = environment / 'game' / relative
            require(target.is_file() and sha(target.read_bytes()) == expected['sha256'],
                    'Исходные definitions/решение изменились после подготовки: ' + str(target))
    for name, expected in manifest['files'].items():
        source = package / name
        require(source.resolve().is_relative_to(package) and source.is_file() and source.stat().st_nlink == 1,
                'Неизолированный файл пакета')
        require(sha(source.read_bytes()) == expected['sha256'] and source.stat().st_size == expected['bytes'], 'Хеш пакета не совпал: ' + name)
        for environment in (host, client):
            target = environment / ('userdata' if name.startswith('save games/') else 'game') / name
            require(target.resolve().is_relative_to(environment) and target.is_file() and target.stat().st_nlink == 1,
                    'Файл пакета не установлен в отдельную среду: ' + str(target))
            require(target.read_bytes() == source.read_bytes(), 'Содержимое среды отличается от пакета: ' + str(target))
    return manifest, sha(raw)


def run(host, client, package, output, timeout=600, visible=False):
    host, client, output = engine.work_path(host), engine.work_path(client), engine.work_path(output)
    require(not output.is_relative_to(host) and not output.is_relative_to(client), 'Отчёт должен находиться вне сред')
    driver = EventDriver(host, client, output, timeout)
    owned, sessions = {}, {}
    logs_attempted = False
    driver.record.update(physical_input=False, visible_windows=visible, test='culture_religion', errors=[])
    try:
        manifest, manifest_hash = verify_package(package, host, client)
        driver.record.update(fixture_manifest_sha256=manifest_hash, expected=EXPECTED)
        preflight = compare(host, client)
        driver.record['preflight'] = preflight
        require(preflight.get('ok'), 'Игровые данные участников различаются')
        driver.save()
        for role, environment in (('host', host), ('client', client)):
            path = environment / 'session.json'
            previous = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
            started_at = time.time()
            try:
                started = engine.cli.start(environment, hidden=not visible)
            finally:
                current = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
                if current.get('token') and current.get('token') != previous.get('token'):
                    require(Path(current['game']).resolve() == environment / 'game', 'Чужая session')
                    driver.record.setdefault('ownership', {})[role] = dict(pid=current['pid'], confirmed=False)
                    owned[role] = engine.OwnedProcess(current['pid'], environment / 'game/v2game.exe')
                    driver.record['ownership'][role]['confirmed'] = True
                    sessions[role] = current
            require(started.get('ok') and role in owned, 'Запуск не подтверждён: ' + str(started))
            driver.record.setdefault('started', {})[role] = started
            driver.save()
            # RPC во время initialisation может превысить180с и отравить сессию.
            wait_initialisation(environment, started_at)
        driver.bootstrap(SAVE, 'PRU', 'D50')
        driver.record.update(ok=False, complete=False)
        driver.pause(True)
        initial = driver.call(host, 'mp-state')
        require(initial.get('ok') and initial.get('paused'), 'Нет начальной паузы')
        initial_date = initial['date_raw']
        driver.record['initial_date_raw'] = initial_date
        while True:
            driver.drain()
            if {CORE_EVENT, DECISION_EVENT} <= driver.selected: break
            current = driver.call(host, 'mp-state')
            require(current.get('ok'), str(current))
            require(current['date_raw'] - initial_date < 80 * 24, 'За80дней не подтверждены оба целевых окна')
            driver.advance(2)
            driver.record.update(ok=False, complete=False)
            driver.save()
        driver.pause(True)
        before = driver.wait(host, 'mp-state', lambda value: value.get('paused') is True and value.get('pending') is False)
        driver.wait(client, 'mp-state', lambda value: value.get('paused') is True and value.get('pending') is False and value.get('date_raw') == before['date_raw'])
        # Закрытие локального окна означает enqueue, поэтому отдельно ждём
        # появления фактических результатов с обеих сторон на общей паузе.
        deadline = time.monotonic() + timeout
        while True:
            states = {role: capture(environment, sessions[role], owned[role])
                      for role, environment in (('host', host), ('client', client))}
            driver.record['snapshots'] = states
            driver.save()
            try:
                check_results(states['host'], states['client'], before['date_raw'])
                break
            except ValueError:
                require(time.monotonic() < deadline, 'Не подтверждены одинаковые результаты на обоих участниках')
                time.sleep(.2)
        driver.record['effects_ok'] = True
        for environment in (host, client):
            driver.wait(environment, 'mp-state', lambda value: value.get('paused') is True and value.get('pending') is False and value.get('date_raw') == before['date_raw'])
        driver.save_campaign('crmp_' + uuid.uuid4().hex[:8])
        logs_attempted = True
        driver.collect_logs()
        proof = driver.record.get('log_proof', {})
        require(proof.get('complete') is True and
                proof.get('proof', {}).get('gpc', {}).get('pass_count', 0) >= 1,
                'Нет полной парной проверки SYNC и хотя бы одного GPC-прохода')
        driver.record.update(ok=True, complete=True, functional_ok=True)
    except BaseException as error:
        driver.record.update(ok=False, complete=False)
        driver.record['errors'].append(dict(type=type(error).__name__, message=str(error), traceback=traceback.format_exc()))
    finally:
        if len(owned) == 2 and not logs_attempted:
            try: driver.collect_logs()
            except BaseException as error:
                driver.record['log_collection_error'] = str(error)
        cleanup = {}
        for role, process in reversed(list(owned.items())):
            try:
                process.stop()
                cleanup[role] = {'stopped': True}
            except BaseException as error:
                cleanup[role] = {'stopped': False, 'error': str(error)}
                driver.record.update(ok=False, complete=False)
        for role, ownership in driver.record.get('ownership', {}).items():
            if not ownership['confirmed']:
                cleanup[role] = {'stopped': False, 'error': 'Процесс создан, но удерживаемый handle не получен'}
                driver.record.update(ok=False, complete=False)
        driver.record['cleanup'] = cleanup
        driver.save()
    return driver.record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('prepare', help='Создать пакет без игр и изменений исходных данных')
    p.add_argument('--game-root', required=True, type=Path)
    p.add_argument('--source-save', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p = commands.add_parser('run', help='Явно запустить две заранее подготовленные среды')
    p.add_argument('--host', required=True, type=Path)
    p.add_argument('--client', required=True, type=Path)
    p.add_argument('--package', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--timeout', type=float, default=600)
    p.add_argument('--visible', action='store_true', help='Оставить собственные окна видимыми; ввод остаётся через CLI')
    args = parser.parse_args()
    if args.command == 'prepare':
        result = prepare(args.game_root, args.source_save, args.output)
        print(json.dumps({'ok': True, 'package': str(args.output), 'files': result['files']}, ensure_ascii=False))
        return 0
    require(args.timeout > 0, 'Тайм-аут должен быть положительным')
    result = run(args.host, args.client, args.package, args.output, args.timeout, args.visible)
    print(json.dumps({'ok': result['ok'], 'report': str(args.output), 'errors': result['errors']}, ensure_ascii=False))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
