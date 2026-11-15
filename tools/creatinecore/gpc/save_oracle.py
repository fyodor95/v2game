"""Независимый расчёт новых корок из CP1251-сохранения Victoria II.

GP задаются явно: числовой great_nations в .v2 не интерпретируется без
квалифицированного соответствия ID. При ничьей учитываются все крупнейшие культуры.
Получатели определяются исключительно по основной культуре, без признанных.
Парсер сохраняет повторные присваивания и элементы списков, но не форматирование.
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
import hashlib
import json
from pathlib import Path
import re


TECHNOLOGY = "nationalism_n_imperialism"
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|#[^\r\n]*|\s+|[{}=]|[^\s{}="#]+')


@dataclass
class Entry:
    key: str | None
    value: str | "Block"


@dataclass
class Block:
    items: list[Entry]

    def values(self, key: str):
        return [item.value for item in self.items if item.key == key]

    def single(self, key: str, default=None):
        values = self.values(key)
        if len(values) > 1:
            raise ValueError(f"Повторено скалярное поле {key}")
        return values[0] if values else default

    def atoms(self):
        if any(item.key is not None or isinstance(item.value, Block) for item in self.items):
            raise ValueError("Ожидался список скалярных значений")
        return [item.value for item in self.items]


class Parser:
    def __init__(self, text: str, implicit_root=False):
        self.source = text
        self.implicit_root = implicit_root
        self.token_offset = 0
        self.tokens = self._tokens()
        self.current = next(self.tokens, None)

    def _tokens(self):
        end = 0
        for match in TOKEN.finditer(self.source):
            if match.start() != end:
                raise ValueError(f"Недопустимый символ или незакрытая строка: {end}")
            end = match.end()
            value = match.group()
            if not value.isspace() and not value.startswith('#'):
                self.token_offset = match.start()
                yield value
        if end != len(self.source):
            raise ValueError(f"Незавершённый текст: {end}")

    def take(self):
        result = self.current
        self.current = next(self.tokens, None)
        return result

    @staticmethod
    def atom(token: str):
        if token.startswith('"'):
            return re.sub(r'\\(["\\])', r'\1', token[1:-1])
        return token

    def value(self):
        if self.current == '{':
            self.take()
            items = list(self.entries(nested=True))
            return Block(items)
        if self.current is None or self.current in ('}', '='):
            raise ValueError("Отсутствует значение")
        return self.atom(self.take())

    def entries(self, nested=False):
        while self.current is not None:
            if self.current == '}':
                if not nested:
                    # Штатные .v2 этой сборки завершают неявный корневой блок
                    # одной скобкой, не имеющей открывающей пары в тексте.
                    if self.implicit_root:
                        self.take()
                        if self.current is None:
                            return
                    raise ValueError(f"Лишняя закрывающая скобка: {self.token_offset}, "
                                     f"{self.source[max(0, self.token_offset-100):self.token_offset+100]!r}")
                self.take()
                return
            if self.current == '{':
                yield Entry(None, self.value())
                continue
            if self.current == '=':
                raise ValueError(f"Ожидалось имя поля или элемент списка: {self.token_offset}, "
                                 f"{self.source[max(0, self.token_offset-60):self.token_offset+60]!r}")
            first = self.atom(self.take())
            if self.current == '=':
                self.take()
                yield Entry(first, self.value())
            else:
                yield Entry(None, first)
        if nested:
            raise ValueError("Отсутствует закрывающая скобка")


def read_entries(path: Path, implicit_root=False):
    # Верхний уровень обрабатывается последовательно: население и армии всей
    # планеты одновременно в AST не удерживаются.
    yield from Parser(path.read_bytes().decode('cp1251'), implicit_root=implicit_root).entries()


def identifiers(path: Path):
    result = set()
    for group in read_entries(path):
        if isinstance(group.value, Block):
            for child in group.value.items:
                if child.key and isinstance(child.value, Block):
                    result.add(child.key)
    return result


def researched(block: Block):
    technology = block.single('technology', Block([]))
    if not isinstance(technology, Block):
        raise ValueError("Некорректный блок технологии")
    item = technology.single(TECHNOLOGY)
    if item is None:
        return False
    values = item.atoms() if isinstance(item, Block) else [item]
    if not values:
        raise ValueError("Пустое состояние технологии")
    return Decimal(values[0]) >= 1


def province_data(number: int, block: Block, cultures, religions, poptypes):
    totals = Counter()
    for pop in block.items:
        if pop.key not in poptypes or not isinstance(pop.value, Block):
            continue
        size = pop.value.single('size')
        if size is None or isinstance(size, Block):
            raise ValueError(f"Провинция {number}: POP без численности")
        try:
            amount = Decimal(size)
        except InvalidOperation as error:
            raise ValueError(f"Провинция {number}: неверная численность POP") from error
        if not amount.is_finite() or amount < 0 or amount != amount.to_integral_value():
            raise ValueError(f"Провинция {number}: численность POP должна быть целой")
        matches = [item.key for item in pop.value.items
                   if item.key in cultures and isinstance(item.value, str) and item.value in religions]
        if len(matches) != 1:
            raise ValueError(f"Провинция {number}: неоднозначная культура/религия POP")
        totals[matches[0]] += int(amount)
    maximum = max(totals.values(), default=0)
    dominant = sorted(c for c, size in totals.items() if size == maximum and maximum > 0)
    cores = block.values('core')
    if any(not isinstance(core, str) for core in cores):
        raise ValueError(f"Провинция {number}: неверный список корок")
    owner = block.single('owner')
    if owner is not None and not isinstance(owner, str):
        raise ValueError(f"Провинция {number}: неверный владелец")
    return {'id': number, 'owner': owner, 'cores': sorted(set(cores)),
            'population': dict(sorted(totals.items())), 'dominant_cultures': dominant}


def preview(save: Path, game_root: Path, great_powers: list[str]):
    if not great_powers or len(set(great_powers)) != len(great_powers):
        raise ValueError("Нужен непустой список уникальных квалифицированных тегов GP")
    cultures = identifiers(game_root / 'common/cultures.txt')
    religion_path = game_root / 'common/religion.txt'
    if not religion_path.exists():
        religion_path = game_root / 'common/religions.txt'
    religions = identifiers(religion_path)
    poptypes = {path.stem for path in (game_root / 'poptypes').glob('*.txt')}
    if not cultures or not religions or not poptypes:
        raise ValueError("Не загружены словари культур, религий или типов POP")
    technology_present = any(
        entry.key == TECHNOLOGY
        for path in sorted((game_root / 'technologies').glob('*.txt'))
        for entry in read_entries(path)
    )
    countries = {}
    provinces = {}
    date = None
    for entry in read_entries(save, implicit_root=True):
        if entry.key == 'date':
            date = entry.value
        if not isinstance(entry.value, Block) or entry.key is None:
            continue
        if entry.key.isdigit():
            number = int(entry.key)
            if number in provinces:
                raise ValueError(f"Повторная провинция {number}")
            provinces[number] = province_data(number, entry.value, cultures, religions, poptypes)
        elif re.fullmatch('[A-Z0-9]{3}', entry.key):
            if entry.key in countries:
                raise ValueError(f"Повторная страна {entry.key}")
            primary = entry.value.single('primary_culture')
            # Accepted — только диагностика. Ошибка этого поля не блокирует
            # расчёт по primary и не превращается в дополнительный фильтр.
            values = []
            accepted_valid = True
            for accepted in entry.value.values('culture'):
                try:
                    if not isinstance(accepted, Block):
                        raise ValueError("Ожидался список")
                    values.extend(accepted.atoms())
                except ValueError:
                    accepted_valid = False
            if primary is not None and primary not in cultures:
                raise ValueError(f"Страна {entry.key}: неизвестная культура")
            countries[entry.key] = {'primary_culture': primary, 'accepted_cultures': sorted(set(values)),
                                    'accepted_cultures_diagnostic_valid': accepted_valid,
                                    'technology_researched': researched(entry.value)}
    unknown = sorted(set(great_powers) - countries.keys())
    if unknown:
        raise ValueError(f"Не найдены страны GP: {unknown}")
    owners = {p['owner'] for p in provinces.values() if p['owner']}
    for tag in sorted(set(great_powers)):
        countries[tag]['eligible'] = tag in owners and (
            not technology_present or countries[tag]['technology_researched'])
    additions = []
    ties = []
    for p in sorted(provinces.values(), key=lambda p: p['id']):
        if not p['owner'] or not p['dominant_cultures']:
            continue
        if p['owner'] not in countries:
            raise ValueError(f"Не найден владелец провинции {p['id']}: {p['owner']}")
        if len(p['dominant_cultures']) > 1:
            ties.append({'province': p['id'], 'cultures': p['dominant_cultures']})
        for tag in sorted(great_powers):
            c = countries[tag]
            if (c['eligible'] and p['owner'] != tag and tag not in p['cores'] and
                    c['primary_culture'] in p['dominant_cultures']):
                additions.append({'province': p['id'], 'country': tag})
    return {'ok': True, 'complete': True, 'ties': ties,
            'save_sha256': hashlib.sha256(save.read_bytes()).hexdigest(), 'date': date,
            'great_powers_source': 'explicit_qualified_tags', 'great_powers': sorted(great_powers),
            'technology_present': technology_present,
            'countries': {tag: countries[tag] for tag in sorted(great_powers)},
            'provinces': sorted(provinces.values(), key=lambda p: p['id']), 'additions': additions}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--save', type=Path, required=True)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--gp', nargs='+', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError("Oracle не перезаписывает существующий отчёт")
    result = preview(args.save, args.game_root, args.gp)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
        output.write('\n')
    return 0 if result['complete'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
