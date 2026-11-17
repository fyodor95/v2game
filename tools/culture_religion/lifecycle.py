"""Дополнительные SP-сценарии PHI/PAN и жизненного цикла Д-тэга; запуск только run."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.culture_religion import engine
from tools.culture_religion.snapshot import capture

old = engine.old
require, sha = old.require, old.sha
ADDING = 'events/adding_cores.txt'
REMOVING = 'events/removing_d_cores.txt'
FORMATION = 'events/super_nationalism.txt'
PRIMARY_DECISIONS = 'decisions/new_primary_culture_for_dxx.txt'
DECISIONS = 'decisions/culture_religion_lifecycle.txt'
LOCALISATION = 'localisation/culture_religion_lifecycle.csv'
TARGET = 'D45'


def primary_decision(raw):
    text = raw.decode('cp1251')
    matches = [node for root in old.Spans(text).roots() if root.key == 'political_decisions'
               for node in root.value if node.key == 'D_primary_aborigine']
    require(len(matches) == 1, 'Нет единственного настоящего D_primary_aborigine')
    node = matches[0]
    potential, effect = old.one(node, 'potential'), old.one(node, 'effect')
    require(old.scalar(potential, 'primary_culture') == 'british' and
            old.scalar(old.one(potential, 'capital_scope'), 'culture') == 'aborigine' and
            any(field.key == 'tag' and field.value == TARGET for field in old.one(potential, 'OR').value) and
            old.scalar(old.one(old.one(node, 'allow'), 'capital_scope'), 'culture') == 'aborigine',
            'Изменились условия настоящего решения Д-страны')
    require([(field.key, field.value) for field in effect.value[:2]] ==
            [('primary_culture', 'aborigine'), ('set_religion_from_primary_culture', 'yes')],
            'Изменились порядок культуры и расчёта религии в настоящем решении')
    return dict(key=node.key, decision_sha256=sha(text[node.key_start:node.end].encode('cp1251')),
                culture='aborigine', religion='animist')


def events(raw):
    text = raw.decode('cp1251')
    wanted = {'phi': ('PHI', 'filipino', 'catholic'), 'pan': ('PAN', 'panjabi', 'sikh'),
              'a': (TARGET, 'aborigine', 'animist'), 'b': (TARGET, 'malay', 'sunni')}
    result = {}
    for event in old.Spans(text).roots():
        if event.key != 'province_event': continue
        option = old.one(event, 'option')
        guard = option.single('if')
        if guard is None: continue
        trigger = old.one(event, 'trigger')
        pair = (old.scalar(guard, 'add_core'), old.scalar(trigger, 'culture'),
                old.scalar(trigger, 'pop_majority_religion'))
        for key, expected in wanted.items():
            if pair != expected: continue
            require(key not in result, 'Повторный получатель/пара: ' + key)
            limit = old.one(guard, 'limit')
            trigger_text, limit_text = text[trigger.start:trigger.end], text[limit.start:limit.end]
            require(re.sub(r'\s+', '', trigger_text) == re.sub(r'\s+', '', limit_text), 'Guard отличается от trigger')
            require([field.key for field in option.value] == ['name', 'if'], 'Неизвестная структура опции')
            if key in ('a', 'b'):
                country = old.one(old.one(guard, 'owner'), TARGET)
                require(old.scalar(country, 'primary_culture') == expected[1] and
                        old.scalar(country, 'religion') == expected[2], 'Первая корка не назначает пару')
            result[key] = dict(id=int(old.scalar(event, 'id')), tag=pair[0], culture=pair[1], religion=pair[2],
                               trigger=trigger_text, guarded_effect=text[guard.key_start:guard.end],
                               event_sha256=sha(text[event.key_start:event.end].encode('cp1251')),
                               decision='cr_life_check_' + key)
    require(set(result) == set(wanted), 'Нет всех PHI/PAN и двух пар D45')
    return result


def country_events(removing, formation):
    remove_text, form_text = removing.decode('cp1251'), formation.decode('cp1251')
    removed = [event for event in old.Spans(remove_text).roots()
               if event.key == 'country_event' and old.scalar(event, 'id') == '145000']
    require(len(removed) == 1, 'Нет единственного RemovingDCores145000')
    matches = [block for block in old.one(removed[0], 'option').fields('any_country')
               if old.scalar(old.one(block, 'limit'), 'tag') == TARGET]
    require(len(matches) == 1 and old.scalar(old.one(matches[0], 'limit'), 'exists') == 'no' and
            old.scalar(matches[0], 'primary_culture') == 'british' and
            matches[0].single('religion') is None and
            old.scalar(old.one(matches[0], 'all_core'), 'remove_core') == TARGET,
            'Контракт RemovingDCores для D45 изменился')
    formed = [event for event in old.Spans(form_text).roots() if event.key == 'country_event' and
              old.scalar(old.one(event, 'option'), 'release') == TARGET]
    require(len(formed) == 1, 'Нет единственного настоящего события образования D45')
    return dict(removing=dict(id=145000, forced_ai_trigger=True,
                              event_sha256=sha(remove_text[removed[0].key_start:removed[0].end].encode('cp1251'))),
                formation=dict(id=int(old.scalar(formed[0], 'id')), tag=TARGET,
                               trigger=form_text[old.one(formed[0], 'trigger').start:old.one(formed[0], 'trigger').end],
                               event_sha256=sha(form_text[formed[0].key_start:formed[0].end].encode('cp1251'))))


def dictionaries(game):
    return (old.identifiers(game / 'common/cultures.txt'), old.identifiers(game / 'common/religion.txt'),
            {path.stem for path in (game / 'poptypes').glob('*.txt')})


def select_provinces(raw, game):
    cultures, religions, types = dictionaries(game)
    capital, candidates, player = None, [], None
    for node in old.Spans(raw.decode('cp1251')).roots():
        if node.key == 'player': player = node.value
        if node.key == 'PRU': capital = int(old.scalar(node, 'capital'))
        if not node.key or not node.key.isdigit() or not isinstance(node.value, list): continue
        if old.scalar(node, 'owner') != 'PRU' or node.single('life_rating') is None: continue
        if any(pop.key in types and isinstance(pop.value, list) and any(
                field.key in cultures and isinstance(field.value, str) and field.value in religions
                for field in pop.value) for pop in node.value):
            candidates.append(int(node.key))
    require(player == 'PRU' and capital and 546 in candidates, 'Нужен исходный PRU-сейв с населённой546')
    second = [number for number in candidates if number not in (546, capital)]
    require(second, 'Нет второй населённой PRU-провинции вне столицы')
    return 546, min(second)


def fixture_save(raw, game, tag, pairs):
    """Копия меняет только получателя отсутствия, его корки и заданные пары POP."""
    cultures, religions, types = dictionaries(game)
    text = raw.decode('cp1251')
    changes, transferred, removed, populated, country = [], [], [], set(), None
    for node in old.Spans(text).roots():
        if node.key == tag:
            require(country is None, 'Повторная страна в сейве')
            country = dict(primary=old.scalar(node, 'primary_culture'), religion=old.scalar(node, 'religion'))
            require(not re.search(r'\bcr_life_\w+', text[node.start:node.end]), 'Фикстура уже содержит тестовые флаги')
        if not node.key or not node.key.isdigit() or not isinstance(node.value, list): continue
        number = int(node.key)
        owner, controller = old.scalar(node, 'owner'), old.scalar(node, 'controller')
        if owner == tag:
            require(tag != TARGET, 'Исходная D45 уже существует')
            field = old.one(node, 'owner'); changes.append((field.start, field.end, '"PRU"'))
            if controller == tag:
                field = old.one(node, 'controller'); changes.append((field.start, field.end, '"PRU"'))
            transferred.append(number)
        for core in node.fields('core'):
            if core.value == tag:
                changes.append((core.key_start, core.end, ''))
                removed.append(number)
        if number not in pairs: continue
        require(owner == 'PRU', 'Тестовая провинция не принадлежит PRU')
        require(number not in populated, 'Повторная тестовая провинция')
        field = old.one(node, 'controller'); changes.append((field.start, field.end, '"REB"'))
        culture, religion = pairs[number]
        require(culture in cultures and religion in religions, 'Неизвестная целевая пара')
        count = 0
        for pop in node.value:
            if pop.key not in types or not isinstance(pop.value, list): continue
            fields = [field for field in pop.value if field.key in cultures and
                      isinstance(field.value, str) and field.value in religions]
            require(len(fields) == 1, 'Неоднозначная пара POP')
            field = fields[0]
            changes.extend([(field.key_start, field.key_end, culture), (field.start, field.end, religion)])
            count += 1
        require(count, 'Нет POP в тестовой провинции')
        populated.add(number)
    require(country and populated == set(pairs), 'Нет страны/провинций фикстуры')
    previous, chunks = 0, []
    for start, end, value in sorted(changes):
        require(previous <= start <= end <= len(text), 'Пересекающиеся замены')
        chunks.extend((text[previous:start], value)); previous = end
    chunks.append(text[previous:])
    return ''.join(chunks).encode('cp1251'), dict(initial=country, transferred=transferred,
                                                removed_core_provinces=removed, replacements=len(changes))


def controls(selected, provinces, formation):
    lines, rows = ['political_decisions = {'], []

    def add(key, label, allow, effect=''):
        lines.append(f'{key} = {{ potential = {{ tag = PRU }} allow = {{ {allow} }} '
                     f'effect = {{ {effect} }} ai_will_do = {{ factor = 0 }} }}')
        rows.append(key + ';' + label + ';' * 13 + 'x')

    for key, event in selected.items():
        number = provinces['b' if key == 'b' else 'a']
        add(event['decision'], 'Проверка условия первой корки ' + event['tag'], f'{number} = {event["trigger"]}')
    add('cr_life_form_check', 'Проверка образования Д-страны', formation['trigger'][1:-1])
    # Это тестовая имитация результата конкурента. Вложенная копия province-if
    # внутри country-decision в нативном опыте меняла пару, но не добавляла корку.
    # Прямые эффекты выполняются на паузе после настоящей проверки allow A;
    # корка и пара независимо проверяются в сейве до выбора настоящего B.
    a = selected['a']
    add('cr_life_claim_a', 'Занять Д-тэг до выбора открытого события',
        f'{provinces["a"]} = {a["trigger"]}',
        f'{provinces["a"]} = {{ add_core = {a["tag"]} }} '
        f'{a["tag"]} = {{ primary_culture = {a["culture"]} religion = {a["religion"]} }} '
        'set_country_flag = cr_life_claimed')
    add('cr_life_claimed', 'Завершено занятие Д-тэга', 'has_country_flag = cr_life_claimed')
    add('cr_life_british_core', 'Подготовить корку Д-страны с исходной культурой',
        f'{provinces["a"]} = {a["trigger"]}',
        f'{provinces["a"]} = {{ add_core = {TARGET} }} set_country_flag = cr_life_british_core_done')
    add('cr_life_british_core_done', 'Подготовлена корка Д-страны с исходной культурой',
        'has_country_flag = cr_life_british_core_done')
    add('cr_life_annex', 'Аннексировать тестовую Д-страну', f'{TARGET} = {{ exists = yes }}',
        f'{TARGET} = {{ annex_to = PRU }} set_country_flag = cr_life_annexed')
    add('cr_life_annexed', 'Аннексия завершена', 'has_country_flag = cr_life_annexed')
    add('cr_life_barrier_set', 'Установить маркер очереди', 'always = yes', 'set_country_flag = cr_life_barrier')
    add('cr_life_barrier_clear', 'Сбросить маркер очереди', 'always = yes', 'clr_country_flag = cr_life_barrier')
    add('cr_life_barrier_check', 'Проверить маркер очереди', 'has_country_flag = cr_life_barrier')
    lines.append('}')
    return {DECISIONS: ('\r\n'.join(lines) + '\r\n').encode('cp1251'),
            LOCALISATION: ('\r\n'.join(rows) + '\r\n').encode('cp1251')}


def prepare(game_root, source_save, output):
    game, source, output = Path(game_root).resolve(), Path(source_save).resolve(), old.work_path(output)
    require(not output.exists(), 'Каталог пакета уже существует')
    paths = [source, *(game / name for name in (ADDING, REMOVING, FORMATION, PRIMARY_DECISIONS, 'common/cultures.txt', 'common/religion.txt')),
             *sorted((game / 'poptypes').glob('*.txt')), Path(__file__).resolve()]
    originals = {path: path.read_bytes() for path in paths}
    selected = events(originals[game / ADDING])
    native = country_events(originals[game / REMOVING], originals[game / FORMATION])
    native['primary_decision'] = primary_decision(originals[game / PRIMARY_DECISIONS])
    a, b = select_provinces(originals[source], game)
    provinces = dict(a=a, b=b)
    outputs = controls(selected, provinces, native['formation'])
    fixtures, cases = {}, []
    for key in ('phi', 'pan', 'd'):
        event = selected['a' if key == 'd' else key]
        pairs = {a: (event['culture'], event['religion'])}
        if key == 'd': pairs[b] = (selected['b']['culture'], selected['b']['religion'])
        saved, details = fixture_save(originals[source], game, event['tag'], pairs)
        if key != 'd':
            require(details['initial'] == dict(primary=event['culture'], religion=event['religion']),
                    'Основная культура/религия исходной обычной страны не совпадает с событием')
        else:
            require(details['initial'] != dict(primary=event['culture'], religion=event['religion']),
                    'Исходная D45 уже имеет целевую пару')
        name = 'cr_life_' + key + '.v2'
        outputs['save games/' + name] = saved
        fixtures[key] = dict(save=name, details=details)
        for case in ([key] if key != 'd' else ['stale', 'reuse', 'formation', 'british_decision']):
            cases.append(dict(name=case, save=name))
    manifest = dict(schema=1, kind='culture_religion_lifecycle', game_root=str(game), source_save=str(source),
                    source_sha256=sha(originals[source]), adding_cores_sha256=sha(originals[game / ADDING]),
                    sources={str(path): dict(sha256=sha(raw), bytes=len(raw)) for path, raw in originals.items()},
                    events=selected, native=native, provinces=provinces, fixtures=fixtures, cases=cases,
                    limitations=['Stale: открыто только настоящее B; результат конкурирующего A имитируется тестовым решением '
                                 'с прямыми add_core/primary_culture/religion после проверки исходного trigger A в allow на паузе. '
                                 'Перед выбором B наличие корки и пары подтверждается отдельным сейвом. '
                                 'Точное исполнение опции A и два открытых окна не заявляются: CLI запрещает два одновременных chosen-event.',
                                 'RemovingDCores145000 вызывается принудительно для PRU; внешний trigger ai=yes обходится, '
                                 'внутренние проверки существует/не существует исполняет настоящий event.',
                                 'British: тестовое решение добавляет только корку. Страна образуется настоящим release; '
                                 'новая копия его нативного сейва меняет только player PRU→D45. '
                                 'Затем исполняется настоящее D_primary_aborigine без копирования его эффектов.'],
                    files={name: dict(sha256=sha(raw), bytes=len(raw)) for name, raw in outputs.items()})
    require(all(path.read_bytes() == raw for path, raw in originals.items()), 'Исходники изменились при подготовке')
    output.mkdir(parents=True)
    for name, raw in outputs.items():
        target = output / name; target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as stream: stream.write(raw)
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return manifest


def state(raw, tag):
    result = dict(country=None, owned=[], cores=[], all_cores=set())
    for node in old.Spans(raw.decode('cp1251')).roots():
        if node.key == tag:
            require(result['country'] is None, 'Повторная страна')
            accepted = node.single('culture')
            result['country'] = dict(primary=old.scalar(node, 'primary_culture'), religion=old.scalar(node, 'religion'),
                                     accepted=sorted(field.value for field in accepted.value) if accepted else [])
        if not node.key or not node.key.isdigit() or not isinstance(node.value, list): continue
        number = int(node.key)
        if old.scalar(node, 'owner') == tag: result['owned'].append(number)
        cores = node.fields('core')
        require(len(cores) == len({field.value for field in cores}), 'Повторная корка')
        result['all_cores'].update((number, field.value) for field in cores)
        if any(field.value == tag for field in cores): result['cores'].append(number)
    require(result['country'] is not None, 'Нет страны в сохранении')
    result['owned'].sort(); result['cores'].sort()
    return result


def expect_pair(observed, event, accepted, cores, owned=()):
    require(observed['country'] == dict(primary=event['culture'], religion=event['religion'], accepted=accepted),
            'Не совпали основная культура, религия или признанные культуры')
    require(observed['cores'] == sorted(cores) and observed['owned'] == sorted(owned), 'Не совпали корки или существование страны')


def switch_player_copy(source, destination):
    """Меняет только player в новой соседней копии нативного сейва; исходник неизменен."""
    source, destination = Path(source).resolve(), Path(destination).resolve()
    require(source.parent == destination.parent and source != destination and
            source.suffix == destination.suffix == '.v2' and source.stat().st_nlink == 1,
            'Копия player должна быть новым соседним изолированным сейвом')
    raw = source.read_bytes()
    players = [node for node in old.Spans(raw.decode('cp1251')).roots() if node.key == 'player']
    require(len(players) == 1 and players[0].value == 'PRU', 'Нет единственного исходного player=PRU')
    field = players[0]
    original = raw[field.start:field.end]
    require(original in (b'PRU', b'"PRU"'), 'Неизвестный формат заголовка player')
    replacement = original.replace(b'PRU', TARGET.encode('ascii'))
    modified = raw[:field.start] + replacement + raw[field.end:]
    require(source.read_bytes() == raw, 'Исходный нативный сейв изменился')
    with destination.open('xb') as stream:
        stream.write(modified)
    require(source.read_bytes() == raw and destination.read_bytes() == modified, 'Не сохранена точная копия player')
    return dict(source=str(source), source_sha256=sha(raw), copy=str(destination), copy_sha256=sha(modified),
                only_change='header_player', before='PRU', after=TARGET, value_offset=field.start)


def formation_population(raw, game, province):
    """Подтверждает нативную столицу и положительное население нужной пары после release."""
    cultures, religions, types = dictionaries(game)
    capital, total, populations = None, old.Decimal(0), []
    for node in old.Spans(raw.decode('cp1251')).roots():
        if node.key == TARGET:
            require(capital is None, 'Повторная страна после образования')
            capital = old.scalar(node, 'capital')
        if node.key != str(province): continue
        require(old.scalar(node, 'owner') == TARGET, 'Столица не принадлежит образовавшейся стране')
        for pop in node.value:
            if pop.key not in types or not isinstance(pop.value, list): continue
            pairs = [(field.key, field.value) for field in pop.value if field.key in cultures and
                     isinstance(field.value, str) and field.value in religions]
            require(pairs == [('aborigine', 'animist')], 'Неверная пара POP образовавшейся страны')
            size = old.Decimal(old.scalar(pop, 'size'))
            require(size.is_finite() and size >= 0, 'Некорректная численность POP')
            total += size
            populations.append(dict(type=pop.key, size=str(size)))
    require(capital == str(province) and total > 0, 'Не подтверждены нативная столица и население')
    return dict(capital=province, culture='aborigine', religion='animist', total=str(total), pops=populations)


class Runner(engine.Runner):
    def preflight(self):
        manifest, game, previous = super().preflight()
        require(manifest.get('kind') == 'culture_religion_lifecycle' and manifest.get('schema') == 1,
                'Неизвестный пакет lifecycle')
        for name in (REMOVING, FORMATION, PRIMARY_DECISIONS, 'common/cultures.txt', 'common/religion.txt'):
            expected = manifest['sources'][str(Path(manifest['game_root']) / name)]['sha256']
            require(old.cli.digest(game / name) == expected, 'Исходные данные lifecycle изменились: ' + name)
        self.record['tool_sources']['tools/culture_religion/lifecycle.py'] = old.cli.digest(Path(__file__))
        self.record['tool_sources']['tools/culture_religion/snapshot.py'] = old.cli.digest(Path(__file__).with_name('snapshot.py'))
        return manifest, game, previous

    def save(self, name):
        return old.Runner.save(self, name)

    def observe(self, name, tag, record):
        saved = self.save(name)
        observed = state(saved.read_bytes(), tag)
        public = {key: value for key, value in observed.items() if key != 'all_cores'}
        record.setdefault('observations', []).append(dict(name=name, save=str(saved), sha256=old.cli.digest(saved),
                                                         state=public, core_pairs_sha256=sha(json.dumps(sorted(observed['all_cores'])).encode())))
        self.checkpoint()
        return observed, saved

    def wait_allow(self, key, expected):
        deadline = time.monotonic() + 30
        while True:
            result = self.request('decision-check PRU ' + key)
            require(result.get('potential') is True and type(result.get('allow')) is bool, 'Не проверен маркер очереди')
            if result['allow'] is expected: return
            require(time.monotonic() < deadline, 'Маркер очереди не изменился; мутация не повторяется')
            time.sleep(.1)

    def action(self, key, marker):
        self.decision(marker, False)
        self.decision(key, True)
        self.request('decision-execute PRU ' + key)
        self.wait_allow(marker, True)

    def settle(self):
        result = self.request('decision-check PRU cr_life_barrier_check')
        require(result.get('potential') is True and type(result.get('allow')) is bool, 'Нет булева маркера')
        previous = result['allow']
        self.request('decision-execute PRU cr_life_barrier_' + ('clear' if previous else 'set'))
        self.wait_allow('cr_life_barrier_check', not previous)
        require(self.request('status').get('event_windows') == 0, 'Окно события не закрыто')

    def country_event(self, number):
        self.request(f'event-fire-chosen PRU {number}')
        self.wait('event-select PRU 0')
        self.settle()

    def positive(self, case, manifest, record, prefix):
        event = manifest['events'][case['name']]; province = manifest['provinces']['a']
        before, _ = self.observe(prefix + '_b', event['tag'], record)
        require(not before['owned'] and not before['cores'], 'Обычная страна уже существует или имеет корки')
        self.decision(event['decision'], True)
        self.fire(event['id'], province)
        after, _ = self.observe(prefix + '_a', event['tag'], record)
        require(after['country'] == before['country'] and not after['owned'] and after['cores'] == [province],
                'Обычная первая корка изменила неверные поля')
        require(after['all_cores'] == before['all_cores'] | {(province, event['tag'])}, 'Изменены посторонние корки')
        self.decision(event['decision'], False)

    def stale(self, manifest, record, prefix):
        a, b = manifest['events']['a'], manifest['events']['b']
        pa, pb = manifest['provinces']['a'], manifest['provinces']['b']
        before, _ = self.observe(prefix + '_b', TARGET, record)
        self.decision(a['decision'], True); self.decision(b['decision'], True)
        self.request(f'event-fire-province-chosen {pb} {b["id"]}')
        self.action('cr_life_claim_a', 'cr_life_claimed')
        occupied, _ = self.observe(prefix + '_c', TARGET, record)
        expect_pair(occupied, a, before['country']['accepted'], [pa])
        require(occupied['all_cores'] == before['all_cores'] | {(pa, TARGET)}, 'Занятие тэга изменило посторонние корки')
        self.decision(b['decision'], False)
        self.wait(f'event-select-province {pb} 0'); self.settle()
        stale, _ = self.observe(prefix + '_s', TARGET, record)
        require(stale == occupied, 'Устаревшее событие изменило корки, культуру, религию или владение')
        record['one_open_event_with_test_competitor_mutation'] = True

    def wait_pair(self, expected, record):
        deadline, attempts = time.monotonic() + 30, 0
        while True:
            observed = capture(self.environment, self.record['session'], self.owned, [TARGET])
            attempts += 1
            country = observed['countries'][TARGET]
            if country['primary'] == expected['culture'] and country['religion'] == expected['religion']:
                record.setdefault('runtime_pairs', []).append(dict(attempts=attempts, snapshot=observed))
                self.checkpoint()
                return
            require(time.monotonic() < deadline, 'Не подтверждена пара после решения; мутация не повторяется')
            time.sleep(.1)

    def british_decision(self, manifest, record, prefix):
        province = manifest['provinces']['a']
        default = dict(culture='british', religion='protestant')
        before, _ = self.observe(prefix + '_b', TARGET, record)
        accepted = before['country']['accepted']
        expect_pair(before, default, accepted, [])
        self.action('cr_life_british_core', 'cr_life_british_core_done')
        seeded, _ = self.observe(prefix + '_c', TARGET, record)
        expect_pair(seeded, default, accepted, [province])
        require(seeded['all_cores'] == before['all_cores'] | {(province, TARGET)}, 'Подготовка изменила посторонние корки')
        self.decision('cr_life_form_check', True)
        self.country_event(manifest['native']['formation']['id'])
        formed, native_save = self.observe(prefix + '_e', TARGET, record)
        expect_pair(formed, default, accepted, [province], [province])
        require(formed['all_cores'] == seeded['all_cores'], 'Образование изменило корки')
        record['formation_population'] = formation_population(native_save.read_bytes(), self.environment / 'game', province)
        user_saves = (self.environment / 'userdata/save games').resolve()
        require(native_save.resolve().parent == user_saves and user_saves.is_relative_to(self.environment),
                'Нативный сейв не принадлежит тестовой среде')
        copy_save = user_saves / (prefix + '_d45.v2')
        record['player_copy'] = switch_player_copy(native_save, copy_save)
        self.checkpoint()
        self.load(copy_save.name)
        loaded, _ = self.observe(prefix + '_l', TARGET, record)
        require(loaded == formed, 'Переключение игрока изменило пару, корки или владение')
        self.wait_pair(default, record)
        decision = manifest['native']['primary_decision']
        checked = self.request(f'decision-check {TARGET} ' + decision['key'])
        require(checked.get('potential') is True and checked.get('allow') is True, 'Настоящее решение Д-страны недоступно')
        self.request(f'decision-execute {TARGET} ' + decision['key'])
        self.wait_pair(decision, record)
        changed, changed_save = self.observe(prefix + '_n', TARGET, record)
        expect_pair(changed, decision, accepted, [province], [province])
        require(changed['all_cores'] == formed['all_cores'], 'Решение изменило корки')
        self.load(changed_save.name)
        reloaded, _ = self.observe(prefix + '_r', TARGET, record)
        require(reloaded == changed, 'Пара, владение или корки потеряны при повторной загрузке')
        self.wait_pair(decision, record)
        record['native_british_formation_primary_decision_reload'] = True

    def lifecycle(self, case, manifest, record, prefix):
        a, b = manifest['events']['a'], manifest['events']['b']
        pa, pb = manifest['provinces']['a'], manifest['provinces']['b']
        before, _ = self.observe(prefix + '_b', TARGET, record)
        accepted = before['country']['accepted']
        self.decision(a['decision'], True); self.fire(a['id'], pa)
        first, first_save = self.observe(prefix + '_f', TARGET, record)
        expect_pair(first, a, accepted, [pa])
        require(first['all_cores'] == before['all_cores'] | {(pa, TARGET)}, 'Первая корка изменила посторонние корки')
        if case['name'] == 'formation':
            self.load(first_save.name)
            reloaded, _ = self.observe(prefix + '_r', TARGET, record)
            require(reloaded == first, 'Первая корка/пара потеряны при загрузке')
        self.decision('cr_life_form_check', True)
        self.country_event(manifest['native']['formation']['id'])
        formed, formed_save = self.observe(prefix + '_e', TARGET, record)
        expect_pair(formed, a, accepted, [pa], [pa])
        require(formed['all_cores'] == first['all_cores'], 'Образование изменило корки')
        if case['name'] == 'formation':
            self.load(formed_save.name)
            reloaded, _ = self.observe(prefix + '_l', TARGET, record)
            require(reloaded == formed, 'Образовавшаяся страна/пара потеряны при загрузке')
            record['first_reload_formation_reload'] = True
            return
        # Реальный reset должен защищать существующий D-тэг.
        self.country_event(manifest['native']['removing']['id'])
        protected, _ = self.observe(prefix + '_p', TARGET, record)
        expect_pair(protected, a, accepted, [pa], [pa])
        require(not (protected['all_cores'] - formed['all_cores']) and
                all(re.fullmatch(r'D\d\d', tag) and tag != TARGET for _, tag in formed['all_cores'] - protected['all_cores']),
                'RemovingDCores изменил защищённые/обычные корки')
        self.action('cr_life_annex', 'cr_life_annexed'); self.settle()
        absent, _ = self.observe(prefix + '_x', TARGET, record)
        expect_pair(absent, a, accepted, [pa])
        require(absent['all_cores'] == protected['all_cores'], 'Аннексия изменила корки')
        self.country_event(manifest['native']['removing']['id'])
        reset, _ = self.observe(prefix + '_z', TARGET, record)
        expect_pair(reset, dict(culture='british', religion=a['religion']), accepted, [])
        require(reset['all_cores'] == absent['all_cores'] - {(pa, TARGET)}, 'Повторный reset изменил посторонние корки')
        self.decision(b['decision'], True); self.fire(b['id'], pb)
        reused, _ = self.observe(prefix + '_u', TARGET, record)
        expect_pair(reused, b, accepted, [pb])
        require(reused['all_cores'] == reset['all_cores'] | {(pb, TARGET)}, 'Повторное использование изменило посторонние корки')
        record['exists_protected_then_annex_reset_reuse'] = True

    def run(self):
        config, original = None, None
        try:
            manifest, game, previous = self.preflight()
            cases = [case for case in manifest['cases'] if not self.only or case['name'] in self.only]
            require(cases and (not self.only or set(self.only) == {case['name'] for case in cases}), 'Неизвестные случаи')
            config = game / 'z_CreatineCore/great_power_culture_cores.ini'
            require(config.resolve().is_relative_to(self.environment) and config.stat().st_nlink == 1, 'INI не изолирована')
            original = config.read_bytes()
            disabled, count = re.subn(rb'(?m)^(\s*ENABLE_GREAT_POWER_CULTURE_CORES\s*=\s*)[01]', rb'\g<1>0', original)
            require(count == 1, 'Нет единственной настройки GPC')
            self.record['gpc_ini'] = dict(original_sha256=sha(original), test_sha256=sha(disabled))
            config.write_bytes(disabled)
            try:
                started = old.cli.start(self.environment, hidden=True)
            finally:
                path = self.environment / 'session.json'
                session = json.loads(path.read_text(encoding='utf-8')) if path.exists() else None
                if session and session.get('token') != (previous or {}).get('token'):
                    require(Path(session['game']).resolve() == game and re.fullmatch('[0-9a-f]{32}', session['token']), 'Чужая session')
                    self.record.update(session=session, cleanup_unconfirmed=True)
                    self.owned = old.OwnedProcess(session['pid'], game / 'v2game.exe')
                    self.record['cleanup_unconfirmed'] = False
            self.checked(started, 'start')
            require(self.owned is not None, 'Нет собственного процесса')
            self.checked(old.cli.load_campaign(self.environment, cases[0]['save'], 1200), 'initial-load')
            require(self.request('culture-cores-status').get('enabled') == 0, 'GPC стенда не выключен')
            for index, case in enumerate(cases):
                record = dict(name=case['name'], complete=False)
                self.record['cases'].append(record); self.checkpoint()
                if index: self.load(case['save'])
                status = self.request('status')
                require(status.get('paused') is True and status.get('multiplayer') is False, 'Требуется пауза SP')
                prefix = self.record['save_prefix'] + str(index)
                if case['name'] in ('phi', 'pan'): self.positive(case, manifest, record, prefix)
                elif case['name'] == 'stale': self.stale(manifest, record, prefix)
                elif case['name'] == 'british_decision': self.british_decision(manifest, record, prefix)
                else: self.lifecycle(case, manifest, record, prefix)
                final = self.request('status')
                require(final.get('paused') is True and final.get('date_raw') == status.get('date_raw'), 'Изменились дата/пауза')
                record['complete'] = True; self.checkpoint()
            self.record.update(ok=True, complete=True, subset=bool(self.only))
        except BaseException as error:
            self.record.update(ok=False, complete=False)
            self.record['errors'].append(dict(type=type(error).__name__, message=str(error)))
        finally:
            if self.owned:
                try:
                    self.owned.stop(); self.record['process_stopped'] = True
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
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('prepare')
    p.add_argument('--game-root', required=True, type=Path)
    p.add_argument('--source-save', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p = commands.add_parser('run')
    p.add_argument('--environment', required=True, type=Path)
    p.add_argument('--fixtures', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--case', action='append', dest='cases', choices=('phi', 'pan', 'stale', 'reuse', 'formation', 'british_decision'))
    args = parser.parse_args()
    if args.command == 'prepare':
        result = prepare(args.game_root, args.source_save, args.output)
        print(json.dumps(dict(ok=True, fixtures=str(args.output), cases=result['cases']), ensure_ascii=False))
        return 0
    output = old.work_path(args.output)
    require(not output.is_relative_to(old.work_path(args.environment)), 'Отчёт должен находиться вне среды')
    with output.open('x', encoding='utf-8') as stream:
        runner = Runner(args.environment, args.fixtures, stream, args.cases)
        ok = runner.run()
    print(json.dumps(dict(ok=ok, errors=runner.record['errors'], output=str(output)), ensure_ascii=False))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
