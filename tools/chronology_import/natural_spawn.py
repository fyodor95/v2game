"""Естественное срабатывание первой корки в изолированном стенде; запуск только явный."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from decimal import Decimal
import json
from pathlib import Path
import re
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.chronology_import import engine_acceptance as engine
from tools.chronology_import import native_event_probe

ADVANCE_STEPS = (2, 2, 12, 16, 2, 2)


def relocate_prussian_armies(raw, province):
    """Убрать только army.location PRU из повстанческой провинции искусственного сейва."""
    text = raw.decode('cp1251')
    country, source, destinations = None, None, {}
    for node in engine.Spans(text).roots():
        if node.key == 'PRU':
            engine.require(country is None, 'Повторный блок PRU')
            country = node
        if not node.key or not node.key.isdigit() or not isinstance(node.value, list):
            continue
        number = int(node.key)
        owner, controller = engine.scalar(node, 'owner'), engine.scalar(node, 'controller')
        if number == province:
            source = {'owner': owner, 'controller': controller}
        # У морских провинций нет сухопутного life_rating. Не выбираем просто свободный ID.
        life_rating = node.single('life_rating')
        if number != province and owner == controller == 'PRU' and life_rating is not None:
            destinations[number] = {'owner': owner, 'controller': controller, 'life_rating': life_rating.value}
    engine.require(country is not None, 'Нет блока PRU')
    engine.require(source == {'owner': 'PRU', 'controller': 'REB'}, 'Исходная провинция должна принадлежать PRU и контролироваться REB')
    replacements = []
    for index, army in enumerate(country.fields('army')):
        location = army.single('location')
        if location is not None and location.value == str(province):
            engine.require(destinations, 'Нет другой сухопутной провинции под владением и контролем PRU')
            destination = 549 if 549 in destinations else min(destinations)
            before = text[location.start:location.end]
            engine.require(before.isascii() and before.isdigit(), 'location армии не является числовым ID')
            replacements.append({'army_index': index, 'army_name': engine.scalar(army, 'name'),
                                 'start': location.start, 'end': location.end,
                                 'before': before, 'after': str(destination)})
    modified = text
    for replacement in sorted(replacements, key=lambda value: value['start'], reverse=True):
        modified = (modified[:replacement['start']] + replacement['after'] +
                    modified[replacement['end']:])
    destination = int(replacements[0]['after']) if replacements else None
    return modified.encode('cp1251'), {
        'source_province': province, 'destination_province': destination,
        'destination_validation': destinations[destination] if destination is not None else None,
        'army_count': len(country.fields('army')), 'moved_army_count': len(replacements),
        'replacements': replacements, 'offsets': 'Исходные byte offsets CP1251; end не включён'}


def accelerate(raw):
    """Сохранить условия и эффект; изменить только MTTH и участие других первых событий."""
    text = raw.decode('cp1251')
    source = engine.find_events(raw)['new']
    first, target, candidates = [], None, set()
    for event in engine.Spans(text).roots():
        if event.key != 'province_event':
            continue
        option = engine.one(event, 'option')
        guard = option.single('if')
        if guard is None:
            continue
        first.append(event)
        trigger = engine.one(event, 'trigger')
        if (engine.scalar(trigger, 'culture') == source['culture'] and
                engine.scalar(trigger, 'pop_majority_religion') == source.get('religion')):
            candidates.add(engine.scalar(guard, 'add_core'))
        if int(engine.scalar(event, 'id')) == source['id']:
            engine.require(target is None, 'Повторный ID события OBO')
            target = event
    engine.require(target is not None and None not in candidates, 'Не найдены первые кандидаты')
    mtth = engine.one(target, 'mean_time_to_happen')
    months = engine.one(mtth, 'months')
    engine.require(Decimal(months.value) == 4 * len(candidates), 'Исходный MTTH OBO не равен 4n')
    engine.require(not mtth.fields('days'), 'В исходном MTTH уже есть days')
    engine.require(not target.fields('is_triggered_only'), 'OBO уже исключено из случайных событий')
    changes = [(months.key_start, months.key_end, 'days'), (months.start, months.end, '1')]
    newline = '\r\n' if '\r\n' in text else '\n'
    disabled = []
    for event in first:
        if event is target:
            continue
        triggered = event.single('is_triggered_only')
        if triggered:
            changes.append((triggered.start, triggered.end, 'yes'))
        else:
            changes.append((event.start + 1, event.start + 1, newline + '\tis_triggered_only = yes'))
        disabled.append(int(engine.scalar(event, 'id')))
    patched = text
    for begin, end, replacement in sorted(changes, reverse=True):
        patched = patched[:begin] + replacement + patched[end:]
    result = patched.encode('cp1251')
    engine.require(engine.find_events(result)['new'] == source, 'Изменились ID/trigger OBO')
    return result, {'event': source, 'source_months': months.value, 'test_days': '1',
                    'unique_candidates': sorted(candidates), 'n': len(candidates),
                    'disabled_first_event_ids': disabled, 'source_sha256': engine.sha(raw),
                    'test_sha256': engine.sha(result)}


def auto_selected_ids(raw):
    """Отличать именно автоопцию от упоминания ID в консольной команде или локализации."""
    phrase = r'vic2test: автоматический выбор первой опции события id=(\d+), всего=\d+'
    for encoding in ('utf-8', 'cp1251'):
        # Последняя запись журнала может быть дописана посреди многобайтовой буквы.
        text = raw.decode(encoding, errors='replace')
        values = [int(value) for value in re.findall(phrase, text)]
        if values:
            return values
    return []


@contextmanager
def temporary_files(changes, artifacts, record):
    """Исходные байты сохраняются до первой записи; выход восстанавливает даже частичную запись."""
    originals = []
    try:
        for index, (path, modified) in enumerate(changes):
            original = path.read_bytes()
            (artifacts / f'original-{index}-{path.name}').write_bytes(original)
            (artifacts / f'test-{index}-{path.name}').write_bytes(modified)
            originals.append((path, original))
            path.write_bytes(modified)
        yield
    finally:
        errors = []
        for path, original in reversed(originals):
            try:
                path.write_bytes(original)
                engine.require(path.read_bytes() == original, 'Байты не восстановлены')
                record.setdefault('restored_files', {})[str(path)] = engine.sha(original)
            except BaseException as error:
                errors.append(f'{path}: {error}')
        engine.require(not errors, '; '.join(errors))


class NaturalRunner(engine.Runner):
    def __init__(self, environment, fixtures, stream, artifacts):
        super().__init__(environment, fixtures, stream, ['new'], visible=True)
        self.artifacts = engine.work_path(artifacts)
        self.record.update(test='natural_first_core', probability_measurement=False,
                           advance_steps_days=list(ADVANCE_STEPS), advance_timeout_seconds=300,
                           interpretation='Естественный планировщик при ускоренном MTTH; не оценка частоты 4n')

    def observe_conditions(self, saved, game, manifest):
        # Ложное условие — результат диагностики, а не отказ чтения состояния.
        native = self.request('decision-check PRU ' + manifest['events']['new']['decision'])
        engine.require(type(native.get('potential')) is bool and type(native.get('allow')) is bool,
                       'Нативное условие события не вычислено')
        return {'native_condition': native,
                'conditions_held': native['potential'] and native['allow'],
                'actual_event': self.event_condition(manifest),
                'saved_state': engine.saved_diagnostic_state(saved, game, manifest['province'], 'OBO')}

    def event_condition(self, manifest):
        event_id = manifest['events']['new']['id']
        command = f'event-check-province {manifest["province"]} {event_id}'
        deadline = time.monotonic() + 30
        while True:
            result = engine.cli.request(self.environment, command, 15)
            if result.get('error') != 'E_DAY_BUSY':
                break
            # Повторяется только read-only запрос: дата могла измениться до окончания дневной фазы.
            self.record['steps'].append({'operation': command, 'response': result})
            self.checkpoint()
            engine.require(time.monotonic() < deadline, 'Не завершилась дневная фаза перед event-check')
            time.sleep(.1)
        self.checked(result, command)
        engine.require(result.get('read_only') is True and result.get('event_id') == event_id and
                       result.get('province') == manifest['province'] and type(result.get('trigger')) is bool,
                       'Не подтверждён native event-check нужного события')
        return result

    def prepare_save(self, manifest, case):
        directory = self.environment / 'userdata/save games'
        source = directory / case['save']
        engine.require(source.resolve().is_relative_to(directory.resolve()) and source.stat().st_nlink == 1,
                       'Исходная фикстура не изолирована')
        original = source.read_bytes()
        expected = manifest['files']['save games/' + case['save']]['sha256']
        engine.require(engine.sha(original) == expected, 'Исходная фикстура изменилась после preflight')
        modified, details = relocate_prussian_armies(original, manifest['province'])
        stem = 'n' + uuid.uuid4().hex[:12]
        target = directory / (stem + '.v2')
        engine.require(len(stem) <= 15 and re.fullmatch('[a-z0-9]+', stem), 'Некорректное имя нового сейва')
        engine.require(target.resolve().is_relative_to(self.environment) and not target.exists(),
                       'Целевой сейв не изолирован или уже существует')
        engine.require(source.read_bytes() == original, 'Исходная фикстура изменилась во время подготовки')
        with target.open('xb') as stream:
            stream.write(modified)
        engine.require(target.stat().st_nlink == 1 and target.read_bytes() == modified, 'Новая копия не подтверждена')
        engine.require(source.read_bytes() == original, 'Исходная фикстура изменилась после подготовки')
        self.record['natural_save'] = dict(details, source_path=str(source), source_sha256=expected,
                                           path=str(target), sha256=engine.sha(modified))
        self.checkpoint()
        return target.name

    def advance(self, days):
        initial = self.request('status')
        self.record['steps'].append({'advance_days': days})
        result = self.request(f'advance-auto {days}', 15)
        target = result['target_date_raw']
        engine.require(target == initial['date_raw'] + days * 24, 'Некорректная целевая дата')
        deadline = time.monotonic() + 300
        while True:
            current = self.request('status', 15)
            if current.get('advance_target') == 0 and current.get('paused') is True:
                engine.require(current['date_raw'] == target, 'Прогон остановился на другой дате')
                return current
            engine.require(time.monotonic() < deadline, 'Истёк срок естественного прогона')
            time.sleep(.2)

    def start_owned(self, game, previous):
        try:
            result = engine.cli.start(self.environment, hidden=False)
        finally:
            path = self.environment / 'session.json'
            session = json.loads(path.read_text(encoding='utf-8')) if path.exists() else None
            if session and session.get('token') != (previous or {}).get('token'):
                engine.require(Path(session['game']).resolve() == game and
                               re.fullmatch('[0-9a-f]{32}', session['token']), 'Чужая session')
                self.record.update(session=session, cleanup_unconfirmed=True)
                self.owned = engine.OwnedProcess(session['pid'], game / 'v2game.exe')
                self.record['cleanup_unconfirmed'] = False
        self.checked(result, 'start-visible')
        engine.require(self.owned is not None, 'Нет собственного процесса')

    def run(self):
        try:
            manifest, game, previous = self.preflight()
            path = game / 'events/adding_cores.txt'
            config = game / 'z_CreatineCore/great_power_culture_cores.ini'
            for candidate in (path, config):
                engine.require(candidate.resolve().is_relative_to(self.environment) and
                               candidate.stat().st_nlink == 1, 'Файл стенда не изолирован')
            raw = path.read_bytes()
            production = engine.ROOT / 'events/adding_cores.txt'
            engine.require(production.read_bytes() == raw, 'Стенд не совпадает с текущим основным adding_cores')
            modified, details = accelerate(raw)
            engine.require(details['event']['id'] == manifest['events']['new']['id'], 'Другой ID OBO в фикстуре')
            self.record['event_patch'] = details
            self.record['production_sha256'] = engine.cli.digest(production)
            self.record['tool_sources'][str(Path(__file__).relative_to(engine.ROOT))] = engine.cli.digest(Path(__file__))
            probe_source = Path(native_event_probe.__file__)
            self.record['tool_sources'][str(probe_source.relative_to(engine.ROOT))] = engine.cli.digest(probe_source)
            case = next(case for case in manifest['cases'] if case['name'] == 'new')
            natural_save = self.prepare_save(manifest, case)
            disabled, count = re.subn(rb'(?m)^(\s*ENABLE_GREAT_POWER_CULTURE_CORES\s*=\s*)[01]',
                                      rb'\g<1>0', config.read_bytes())
            engine.require(count == 1, 'Не найден единственный переключатель GPC')
            self.checkpoint()
            with temporary_files([(path, modified), (config, disabled)], self.artifacts, self.record):
                try:
                    self.start_owned(game, previous)
                    self.checked(engine.cli.load_campaign(self.environment, natural_save, 1200), 'load ' + natural_save)
                    engine.require(self.request('culture-cores-status').get('enabled') == 0, 'GPC не отключён')
                    status = self.request('status')
                    engine.require(status.get('paused') is True and status.get('multiplayer') is False,
                                   'Нужна одиночная кампания на паузе')
                    prefix = self.record['save_prefix']
                    before = engine.core_pairs(self.save(prefix + '_before'))
                    engine.require(not any(tag == 'OBO' for _, tag in before), 'OBO уже имеет корки')
                    self.decision(manifest['events']['new']['decision'], True)
                    actual = self.event_condition(manifest)
                    self.record['initial_actual_event'] = actual
                    self.record['initial_province_cache_empty'] = actual['cached']['count'] == 0
                    self.checkpoint()
                    engine.require(actual['trigger'], 'Настоящий trigger события ложен до продвижения')
                    # Native load может пропустить начальную месячную обработку (world+D10).
                    # Наблюдаем пустой список, затем проверяем штатный переход месяца.
                    log = game / 'z_CreatineCore/Logs/v2dll.log'
                    offset = log.stat().st_size
                    selected, found, total_selected, day = [], False, 0, 0
                    for step_days in ADVANCE_STEPS:
                        state = self.advance(step_days)
                        day += step_days
                        total_selected += state.get('auto_events_selected', 0)
                        # Поздний popup может появиться уже после raw-date остановки.
                        # Следующий advance-auto выберет его; отдельный decision-barrier
                        # здесь не нужен: успех доказывают опция в логе и корка в сейве.
                        saved = self.save(prefix + f'_day{day}')
                        after = engine.core_pairs(saved)
                        log_bytes = log.read_bytes()
                        engine.require(len(log_bytes) >= offset, 'Журнал усечён во время прогона')
                        delta = log_bytes[offset:]
                        (self.artifacts / f'native-day{day}.log').write_bytes(delta)
                        selected = auto_selected_ids(delta)
                        observation = {'day': day, 'save': str(saved), 'state': state,
                                       'total_auto_selected': total_selected,
                                       'auto_selected_ids': selected, 'added': sorted(after - before),
                                       'removed': sorted(before - after),
                                       'obo_cores': sorted(number for number, tag in after if tag == 'OBO')}
                        self.record.setdefault('observations', []).append(observation)
                        self.checkpoint()
                        observation.update(self.observe_conditions(saved, game, manifest))
                        self.checkpoint()
                        found = (manifest['province'], 'OBO') in after
                        if found:
                            engine.require(details['event']['id'] in selected and total_selected > 0,
                                           'Корка есть, но естественная автоопция не подтверждена')
                            engine.require(observation['obo_cores'] == [manifest['province']],
                                           'Обнаружены неожиданные дополнительные OBO-корки')
                            self.decision(manifest['events']['new']['decision'], False)
                            break
                    engine.require(found, f'За {sum(ADVANCE_STEPS)} дня OBO-корка не появилась')
                    self.record.update(ok=True, complete=True)
                finally:
                    if self.owned:
                        try:
                            if not self.record['ok']:
                                try:
                                    self.record['native_event_probe'] = native_event_probe.capture(
                                        self.environment, self.record['session'], self.owned,
                                        manifest['province'], details['event']['id'])
                                except BaseException as error:
                                    self.record['native_event_probe'] = {'complete': False, 'read_only': True,
                                                                        'errors': [{'type': type(error).__name__, 'message': str(error)}]}
                                self.checkpoint()
                        finally:
                            self.owned.stop()
                            self.record['process_stopped'] = True
        except BaseException as error:
            self.record.update(ok=False, complete=False)
            self.record['errors'].append({'type': type(error).__name__, 'message': str(error)})
        finally:
            self.checkpoint()
        return self.record['ok']


def prepare_snr(game_root, output, province=546):
    """Только пакет для последующего показа флага главным агентом, без запуска и установки."""
    game_root, output = Path(game_root).resolve(), engine.work_path(output)
    engine.require(province > 0, 'Некорректная провинция')
    engine.require(not output.exists(), 'Каталог пакета уже существует')
    number = 3997301
    for path in (game_root / 'events').glob('*.txt'):
        engine.require(not re.search(rb'(?<![A-Za-z0-9_])id\s*=\s*' + str(number).encode() + rb'\b', path.read_bytes()),
                       'ID тестового события уже занят')
    text = (f'country_event = {{\r\n\tid = {number}\r\n\tis_triggered_only = yes\r\n'
            '\ttitle = "CHR_SNR_SETUP_TITLE"\r\n\tdesc = "CHR_SNR_SETUP_DESC"\r\n'
            '\toption = {\r\n\t\tname = "CHR_SNR_SETUP_OPTION"\r\n'
            '\t\tif = {\r\n\t\t\tlimit = { tag = PRU NOT = { exists = SNR } }\r\n'
            f'\t\t\t{province} = {{ add_core = SNR secede_province = SNR }}\r\n'
            '\t\t}\r\n\t}\r\n}\r\n')
    rows = {'CHR_SNR_SETUP_TITLE': 'Проверка страны Сеннар',
            'CHR_SNR_SETUP_DESC': 'Испытание новой страны и её флага в изолированной среде.',
            'CHR_SNR_SETUP_OPTION': 'Создать Сеннар для проверки'}
    localisation = ''.join(key + ';' + value + ';' * 13 + 'x\r\n' for key, value in rows.items())
    files = {'events/chronology_snr_test.txt': text.encode('cp1251'),
             'localisation/chronology_snr_test.csv': localisation.encode('cp1251')}
    manifest = {'event_id': number, 'country': 'SNR', 'province': province, 'invoke_as': 'PRU',
                'game_root': str(game_root), 'files': {name: engine.sha(raw) for name, raw in files.items()},
                'interpretation': 'Только подготовка фикстуры; GUI и эффект ещё не проверены'}
    output.mkdir(parents=True)
    for name, raw in files.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    run = commands.add_parser('run', help='Явно запустить видимый бесшумный стенд и восстановить его файлы')
    run.add_argument('--environment', required=True, type=Path)
    run.add_argument('--fixtures', required=True, type=Path)
    run.add_argument('--output', required=True, type=Path, help='Новый каталог отчёта и резервных копий в .work')
    prepare = commands.add_parser('prepare-snr', help='Только новый пакет для GUI-проверки SNR')
    prepare.add_argument('--game-root', required=True, type=Path)
    prepare.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.command == 'prepare-snr':
        print(json.dumps(prepare_snr(args.game_root, args.output), ensure_ascii=False))
        return 0
    output, environment = engine.work_path(args.output), engine.work_path(args.environment)
    engine.require(not output.is_relative_to(environment) and not output.exists(),
                   'Нужен новый каталог вне тестовой среды')
    output.mkdir(parents=True)
    with (output / 'report.json').open('x', encoding='utf-8') as stream:
        runner = NaturalRunner(environment, args.fixtures, stream, output)
        ok = runner.run()
    print(json.dumps({'ok': ok, 'report': str(output / 'report.json'), 'errors': runner.record['errors']}, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
