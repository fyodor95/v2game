"""Создать изолированный fixture, заменяя только культуры существующих POP."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from decimal import Decimal
import hashlib
import json
from pathlib import Path

from save_oracle import TOKEN, Parser, identifiers

WORK = Path(__file__).resolve().parents[3] / '.work'


@dataclass
class Span:
    key: str | None
    key_start: int
    key_end: int
    start: int
    end: int
    value: str | list['Span']

    def fields(self, key):
        if not isinstance(self.value, list):
            raise ValueError('Ожидался блок')
        return [entry for entry in self.value if entry.key == key]

    def single(self, key):
        values = self.fields(key)
        if len(values) > 1:
            raise ValueError(f'Повторное скалярное поле {key}')
        return values[0] if values else None


class Spans:
    def __init__(self, source):
        self.source = source
        self.tokens = self.lex()
        self.token = next(self.tokens, None)

    def lex(self):
        end = 0
        for match in TOKEN.finditer(self.source):
            if match.start() != end:
                raise ValueError(f'Неизвестный символ или незакрытая строка: {end}')
            end = match.end()
            value = match.group()
            if not value.isspace() and not value.startswith('#'):
                yield value, match.start(), match.end()
        if end != len(self.source):
            raise ValueError('Незавершённый текст')

    def take(self):
        token = self.token
        self.token = next(self.tokens, None)
        return token

    def value(self, key, key_start, key_end):
        if self.token is None or self.token[0] in ('}', '='):
            raise ValueError('Отсутствует значение')
        token, start, end = self.take()
        if token != '{':
            return Span(key, key_start, key_end, start, end, Parser.atom(token))
        children, closing = self.entries(nested=True)
        return Span(key, key_start, key_end, start, closing, children)

    def entries(self, nested=False):
        items = []
        while self.token:
            token, start, end = self.token
            if token == '}':
                self.take()
                if not nested:
                    raise ValueError('Лишняя закрывающая скобка')
                return items, end
            if token == '{':
                items.append(self.value(None, start, start))
                continue
            if token == '=':
                raise ValueError('Отсутствует ключ')
            self.take()
            if self.token and self.token[0] == '=':
                self.take()
                items.append(self.value(Parser.atom(token), start, end))
            else:
                items.append(Span(None, start, start, start, end, Parser.atom(token)))
        if nested:
            raise ValueError('Отсутствует закрывающая скобка')
        return items, len(self.source)

    def roots(self):
        # Сохраняем AST только одной провинции/страны, а не всего мира.
        while self.token:
            token, start, end = self.take()
            if token == '}' and self.token is None:
                return  # Штатный .v2 закрывает неявный корневой блок.
            if token in ('}', '{', '=') or not self.token or self.token[0] != '=':
                raise ValueError('Некорректный корневой блок')
            self.take()
            yield self.value(Parser.atom(token), start, end)


def partition(sizes):
    total = sum(sizes)
    if total <= 0 or total > 200000 or total % 2 or len(sizes) > 40:
        return None
    half = total // 2
    mask = (1 << (half + 1)) - 1
    history = [1]
    for size in sizes:
        history.append((history[-1] | (history[-1] << size)) & mask)
    if not (history[-1] >> half) & 1:
        return None
    selected = set()
    remaining = half
    for index in reversed(range(len(sizes))):
        if not (history[index] >> remaining) & 1:
            selected.add(index)
            remaining -= sizes[index]
    if remaining:
        raise AssertionError('Ошибка восстановления точного разбиения')
    return selected


def scalar(node, key, default=None):
    field = node.single(key)
    if field is None:
        return default
    if not isinstance(field.value, str):
        raise ValueError(f'Ожидалось скалярное поле {key}')
    return field.value


def replace_lexeme(source, start, end, replacement):
    return '"' + replacement + '"' if source[start:end].startswith('"') else replacement


def transform(original: bytes, game_root: Path, scenario: str):
    if scenario not in ('tie', 'shared'):
        raise ValueError('Неизвестный сценарий')
    source = original.decode('cp1251')
    cultures = identifiers(game_root / 'common/cultures.txt')
    religion = game_root / 'common/religion.txt'
    religions = identifiers(religion if religion.exists() else game_root / 'common/religions.txt')
    types = {p.stem for p in (game_root / 'poptypes').glob('*.txt')}
    if not {'south_german', 'north_german'} <= cultures or not religions or not types:
        raise ValueError('Нет необходимых словарей культуры/религии/типов POP')
    candidate = None
    primary = {}
    for node in Spans(source).roots():
        if node.key in ('AUS', 'NGF') and isinstance(node.value, list):
            if node.key in primary:
                raise ValueError(f'Повторная страна {node.key}')
            field = node.single('primary_culture')
            if field is None or not isinstance(field.value, str):
                raise ValueError(f'Не найдена основная культура {node.key}')
            primary[node.key] = field
        if (candidate is not None or not node.key or not node.key.isdigit() or
                not isinstance(node.value, list)):
            continue
        owner = scalar(node, 'owner')
        if not owner or owner in ('AUS', 'NGF'):
            continue
        cores = [field.value for field in node.fields('core')]
        if 'AUS' in cores or 'NGF' in cores:
            continue
        pops = []
        for pop in node.value:
            if pop.key not in types or not isinstance(pop.value, list):
                continue
            size = Decimal(scalar(pop, 'size', '-1'))
            if not size.is_finite() or size < 0 or size != size.to_integral_value():
                raise ValueError(f'Неверная численность POP в {node.key}')
            matches = [field for field in pop.value if field.key in cultures and
                       isinstance(field.value, str) and field.value in religions]
            if len(matches) != 1:
                raise ValueError(f'Неоднозначная культура POP в {node.key}')
            pops.append((int(size), matches[0]))
        selected = partition([size for size, _ in pops])
        if selected is not None:
            candidate = (int(node.key), owner, cores, pops, selected)
    if primary.keys() != {'AUS', 'NGF'}:
        raise ValueError('В сохранении отсутствует AUS или NGF')
    if primary['AUS'].value != 'south_german' or primary['NGF'].value != 'north_german':
        raise ValueError('Исходные primary AUS/NGF не соответствуют квалифицированному сценарию')
    if candidate is None:
        raise ValueError('Нет чужой провинции без корок AUS/NGF с точным равным разбиением POP до 200000')
    number, owner, cores, pops, selected = candidate
    replacements = []
    totals = {'south_german': 0, 'north_german': 0}
    for index, (size, field) in enumerate(pops):
        culture = 'south_german' if scenario == 'shared' or index in selected else 'north_german'
        totals[culture] += size
        new = replace_lexeme(source, field.key_start, field.key_end, culture)
        if source[field.key_start:field.key_end] != new:
            replacements.append({'start': field.key_start, 'end': field.key_end,
                                 'before': source[field.key_start:field.key_end], 'after': new,
                                 'kind': 'POP_culture_key'})
    if scenario == 'shared':
        field = primary['NGF']
        if not source[field.start:field.end].startswith('"'):
            raise ValueError('Ожидалось quoted primary_culture NGF')
        replacements.append({'start': field.start, 'end': field.end,
                             'before': source[field.start:field.end], 'after': '"south_german"',
                             'kind': 'NGF_primary_culture'})
    output = source
    for replacement in sorted(replacements, key=lambda item: item['start'], reverse=True):
        output = output[:replacement['start']] + replacement['after'] + output[replacement['end']:]
    result = output.encode('cp1251')
    manifest = {'scenario': scenario, 'province': number, 'old_owner': owner, 'original_cores': cores,
                'population': totals, 'pop_count': len(pops),
                'source_sha256': hashlib.sha256(original).hexdigest(),
                'output_sha256': hashlib.sha256(result).hexdigest(),
                'expected_direct_pairs': [{'province': number, 'country': 'AUS'},
                                          {'province': number, 'country': 'NGF'}],
                'replacements': sorted(replacements, key=lambda item: item['start'])}
    return result, manifest


def validate_output(output: Path, work_root: Path = WORK):
    output = output.resolve()
    root = work_root.resolve()
    if (not output.is_relative_to(root) or output.parent.name != 'save games' or
            output.parent.parent.name != 'userdata' or output.suffix.lower() != '.v2'):
        raise ValueError('Выход должен быть новым .v2 внутри .work/**/userdata/save games')
    if not output.parent.is_dir():
        raise ValueError('Изолированный каталог сохранений ещё не подготовлен')
    return output


def create_fixture(source: Path, game_root: Path, output: Path, scenario: str, *, work_root=WORK):
    output = validate_output(output, work_root)
    source = source.resolve(strict=True)
    metadata = output.with_suffix(output.suffix + '.json')
    if output == source or output.exists() or metadata.exists():
        raise FileExistsError('Исходный файл, fixture и manifest не перезаписываются')
    original = source.read_bytes()
    result, manifest = transform(original, game_root, scenario)
    if source.read_bytes() != original:
        raise RuntimeError('Исходный сейв изменился во время чтения')
    manifest.update(source=str(source), output=str(output))
    created = []
    try:
        with output.open('xb') as saved:
            created.append(output)
            with metadata.open('x', encoding='utf-8') as report:
                created.append(metadata)
                saved.write(result)
                json.dump(manifest, report, ensure_ascii=False, indent=2)
                report.write('\n')
    except Exception:
        # Только собственные новые файлы, пути которых проверены выше.
        for path in created:
            path.unlink()
        raise
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scenario', choices=['tie', 'shared'], required=True)
    args = parser.parse_args()
    manifest = create_fixture(args.source, args.game_root, args.output, args.scenario)
    print(json.dumps({'ok': True, 'province': manifest['province'],
                      'population': manifest['population'], 'output_sha256': manifest['output_sha256']}))


if __name__ == '__main__':
    main()
