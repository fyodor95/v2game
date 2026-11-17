"""Синтетические сейвы для решения Д-страны и ничьей религий, без запуска игры."""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.chronology_import.engine_acceptance import Spans, identifiers, require, scalar


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def _dictionaries(game_root):
    cultures = identifiers(game_root / 'common/cultures.txt')
    religions = identifiers(game_root / 'common/religion.txt')
    types = {path.stem for path in (game_root / 'poptypes').glob('*.txt')}
    require({'north_german', 'south_german', 'british'} <= cultures, 'Нет необходимых культур')
    require({'catholic', 'protestant', 'jewish'} <= religions and types, 'Нет религий или профессий POP')
    return cultures, religions, types


def _inspect(text, dictionaries):
    cultures, religions, types = dictionaries
    result = dict(player=None, countries={}, provinces={}, north=defaultdict(Counter), world_north=Counter())
    for node in Spans(text).roots():
        if node.key == 'player':
            require(result['player'] is None and isinstance(node.value, str), 'Неоднозначный player')
            result['player'] = node
        elif node.key in ('PRU', 'D50'):
            require(node.key not in result['countries'] and isinstance(node.value, list), 'Повторная страна')
            result['countries'][node.key] = node
        elif node.key and node.key.isdigit() and isinstance(node.value, list):
            number = int(node.key)
            require(number not in result['provinces'], 'Повторная провинция')
            owner, controller = scalar(node, 'owner'), scalar(node, 'controller')
            info = dict(owner=owner, controller=controller, end=node.end, pops=[], total=Decimal(0),
                        north=Counter(), cultures=Counter(), cores=node.fields('core'),
                        fields={key: node.single(key) for key in ('owner', 'controller')},
                        land=node.single('life_rating') is not None)
            for pop_index, pop in enumerate(node.value):
                if pop.key not in types or not isinstance(pop.value, list):
                    continue
                matches = [field for field in pop.value if field.key in cultures and
                           isinstance(field.value, str) and field.value in religions]
                require(len(matches) == 1, f'Неоднозначная культура/религия POP: {number}')
                size_field = pop.single('size')
                require(size_field is not None and isinstance(size_field.value, str), 'Нет размера POP')
                size = Decimal(size_field.value)
                require(size.is_finite() and size >= 0, 'Недопустимая численность POP')
                pair = matches[0]
                info['total'] += size
                info['cultures'][pair.key] += size
                if pair.key == 'north_german':
                    info['north'][pair.value] += size
                    result['north'][owner][pair.value] += size
                    result['world_north'][pair.value] += size
                # Удерживаем только поля POP, нужные для замен в исходных провинциях PRU.
                if owner == 'PRU':
                    info['pops'].append(dict(pair=pair, size=size, size_field=size_field, index=pop_index))
            result['provinces'][number] = info
    require(result['player'] is not None and set(result['countries']) == {'PRU', 'D50'}, 'Нет player/PRU/D50')
    return result


def _field(text, field, value):
    require(field is not None and isinstance(field.value, str), 'Нет обязательного скалярного поля')
    quoted = text[field.start:field.end].startswith('"')
    return field.start, field.end, '"' + value + '"' if quoted else value


def _apply(text, edits):
    previous = 0
    chunks = []
    changed = []
    # Один проход не копирует многомегабайтный сейв заново для каждого изменяемого POP.
    for start, end, value in sorted(edits):
        require(previous <= start <= end <= len(text), 'Пересекающиеся замены сейва')
        chunks.extend((text[previous:start], value))
        if text[start:end] != value:
            changed.append(dict(start=start, end=end, before=text[start:end], after=value))
        previous = end
    chunks.append(text[previous:])
    return ''.join(chunks).encode('cp1251'), changed


def _totals(counter):
    return {key: str(value) for key, value in sorted(counter.items()) if value > 0}


def _winner(counter):
    require(counter and max(counter.values()) > 0, 'Нет положительного населения')
    maximum = max(counter.values())
    winners = sorted(key for key, value in counter.items() if value == maximum)
    require(len(winners) == 1, 'Ожидался единственный религиозный лидер')
    return winners[0]


def transform(raw: bytes, game_root: Path):
    """Вернуть outputs[имя.v2] и manifest; исходные файлы и игровые данные не изменяются."""
    dictionaries = _dictionaries(Path(game_root))
    text = raw.decode('cp1251')
    source = _inspect(text, dictionaries)
    pru, dynamic = source['countries']['PRU'], source['countries']['D50']
    require(source['player'].value == 'PRU', 'Нужен исходный player PRU')
    require(scalar(pru, 'primary_culture') == 'north_german', 'Исходная основная культура PRU должна быть north_german')
    require(not any(info['owner'] == 'D50' for info in source['provinces'].values()), 'D50 уже владеет провинциями')
    capital_value = scalar(pru, 'capital')
    require(capital_value is not None and capital_value.isdigit(), 'Не определена исходная столица PRU')
    pru_capital = int(capital_value)
    require(pru_capital in source['provinces'] and source['provinces'][pru_capital]['owner'] == 'PRU',
            'Столица PRU не принадлежит PRU')
    require(546 in source['provinces'] and source['provinces'][546]['owner'] == 'PRU', 'Провинция 546 должна остаться PRU')
    eligible = sorted((info['total'], number) for number, info in source['provinces'].items()
                      if number not in (pru_capital, 546) and info['owner'] == info['controller'] == 'PRU'
                      and info['land'] and info['total'] > 0)
    require(len(eligible) >= 2 and eligible[0][0] < eligible[-1][0], 'Нет двух подходящих провинций разной численности')
    capital_size, capital = eligible[0]
    large_size = eligible[-1][0]
    large = min(number for size, number in eligible if size == large_size)
    selected = (capital, large)
    newline = '\r\n' if '\r\n' in text else '\n'
    dynamic_edits = [_field(text, dynamic.single('primary_culture'), 'british'),
                     _field(text, dynamic.single('religion'), 'jewish')]
    capital_field = dynamic.single('capital')
    if capital_field is None:
        dynamic_edits.append((dynamic.end - 1, dynamic.end - 1, newline + '\tcapital=' + str(capital) + newline))
    else:
        dynamic_edits.append(_field(text, capital_field, str(capital)))
    for number, info in source['provinces'].items():
        for core in info['cores']:
            if core.value == 'D50':
                dynamic_edits.append((core.key_start, core.end, ''))
        if number not in selected:
            continue
        dynamic_edits.extend(_field(text, info['fields'][key], 'D50') for key in ('owner', 'controller'))
        dynamic_edits.append((info['end'] - 1, info['end'] - 1, newline + '\tcore="D50"' + newline))
        for pop in info['pops']:
            pair = pop['pair']
            # Ключ культуры и значение религии меняются независимо; остальные поля POP сохраняются.
            quoted = text[pair.key_start:pair.key_end].startswith('"')
            dynamic_edits.append((pair.key_start, pair.key_end, '"north_german"' if quoted else 'north_german'))
            dynamic_edits.append(_field(text, pair, 'protestant' if number == capital else 'catholic'))
    tie_edits, kept = [], []
    for number, info in sorted(source['provinces'].items()):
        if info['owner'] != 'PRU':
            continue
        for pop in info['pops']:
            pair = pop['pair']
            if pair.key != 'north_german':
                continue
            if len(kept) < 2 and pop['size'] > 0:
                religion = 'catholic' if not kept else 'protestant'
                tie_edits.extend([_field(text, pair, religion), _field(text, pop['size_field'], '100')])
                kept.append(dict(province=number, pop_index=pop['index'], religion=religion, size='100'))
            else:
                quoted = text[pair.key_start:pair.key_end].startswith('"')
                tie_edits.append((pair.key_start, pair.key_end, '"south_german"' if quoted else 'south_german'))
    require(len(kept) == 2, 'У PRU меньше двух положительных north_german POP')
    edits = {'cr_d_primary.v2': dynamic_edits + [_field(text, source['player'], 'D50')],
             'cr_tie.v2': tie_edits, 'cr_mp_base.v2': dynamic_edits}
    outputs, scenarios = {}, {}
    for filename, replacements in edits.items():
        result, mutations = _apply(text, replacements)
        state = _inspect(result.decode('cp1251'), dictionaries)
        scenario = dict(output_sha256=sha(result), bytes=len(result), mutations=mutations,
                        player=state['player'].value)
        if filename == 'cr_tie.v2':
            require(state['north']['PRU'] == Counter(catholic=Decimal(100), protestant=Decimal(100)), 'Не получена точная ничья')
            scenario.update(country='PRU', primary='north_german', kept_pops=kept,
                            initial_religion=scalar(pru, 'religion'), totals=_totals(state['north']['PRU']),
                            expected_by_current_religion={'jewish': 'catholic', 'protestant': 'protestant'})
        else:
            owned = sorted(number for number, info in state['provinces'].items() if info['owner'] == 'D50')
            cores = sorted(number for number, info in state['provinces'].items()
                           for core in info['cores'] if core.value == 'D50')
            require(owned == cores == sorted(selected), 'Неверные владения или корки D50')
            actual_dynamic = state['countries']['D50']
            require(scalar(actual_dynamic, 'primary_culture') == 'british' and
                    scalar(actual_dynamic, 'religion') == 'jewish' and
                    scalar(actual_dynamic, 'capital') == str(capital), 'Неверная исходная идентичность D50')
            require(state['player'].value == ('D50' if filename == 'cr_d_primary.v2' else 'PRU'), 'Неверный player')
            for number in selected:
                info = state['provinces'][number]
                require(info['controller'] == 'D50' and info['total'] == source['provinces'][number]['total'] and
                        info['cultures'] == Counter(north_german=info['total']), 'Изменились POP или контролёр')
            require(_winner(state['north']['D50']) == 'catholic' and
                    _winner(state['provinces'][capital]['north']) == 'protestant' and
                    _winner(state['world_north']) == 'protestant', 'Не различаются страна, столица и мир')
            require(state['provinces'][546]['owner'] == 'PRU' and state['provinces'][pru_capital]['owner'] == 'PRU',
                    'Отнята защищённая провинция PRU')
            scenario.update(country='D50', capital=capital, provinces=list(selected),
                            primary_before='british', religion_before='jewish', primary_after='north_german',
                            expected_religion='catholic', capital_religion='protestant', world_religion='protestant',
                            totals=_totals(state['north']['D50']), world_totals=_totals(state['world_north']),
                            province_population={str(capital): str(capital_size), str(large): str(large_size)})
        outputs[filename], scenarios[filename] = result, scenario
    manifest = dict(schema=1, kind='culture_religion_decision_fixtures', source_sha256=sha(raw),
                    source_bytes=len(raw), game_root=str(Path(game_root).resolve()),
                    countries={'PRU': {'capital': pru_capital}, 'D50': {'capital': capital}},
                    protected_prussian_provinces=sorted({546, pru_capital}), scenarios=scenarios,
                    mutation_offsets='Байты исходного CP1251; end не включён')
    return outputs, manifest


def prepare(game_root: Path, source_save: Path, output: Path, *, work_root=ROOT / '.work'):
    """Создать новый пакет output/save games/*.v2 и manifest.json; существующие файлы не трогать."""
    game_root, source_save = Path(game_root).resolve(strict=True), Path(source_save).resolve(strict=True)
    output, allowed = Path(output).resolve(), Path(work_root).resolve()
    require(output.is_relative_to(allowed) and output != allowed, 'Выход должен быть новым каталогом внутри .work')
    require(not output.exists(), 'Выходной каталог уже существует')
    original = source_save.read_bytes()
    outputs, manifest = transform(original, game_root)
    require(source_save.read_bytes() == original, 'Исходный сейв изменился во время подготовки')
    manifest.update(source=str(source_save), output=str(output))
    created = []
    output.mkdir(parents=True, exist_ok=False)
    directory = output / 'save games'
    try:
        directory.mkdir()
        for name, raw in outputs.items():
            path = directory / name
            with path.open('xb') as stream:
                created.append(path)
                stream.write(raw)
        path = output / 'manifest.json'
        with path.open('x', encoding='utf-8', newline='\n') as stream:
            created.append(path)
            json.dump(manifest, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        require(source_save.read_bytes() == original, 'Исходный сейв изменился во время записи')
    except BaseException:
        for path in reversed(created):
            path.unlink()
        if directory.exists():
            directory.rmdir()
        output.rmdir()
        raise
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('prepare',))
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--source-save', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = prepare(args.game_root, args.source_save, args.output)
    print(json.dumps({'ok': True, 'output': manifest['output'], 'source_sha256': manifest['source_sha256'],
                      'scenarios': {name: {key: value for key, value in scenario.items() if key != 'mutations'}
                                    for name, scenario in manifest['scenarios'].items()}}, ensure_ascii=False, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
