"""Встроить тестовое состояние для первого успеха настоящего MTTH 60.

Меняются только присваивания зарезервированного cc_gpc_ в корневом flags.
Производственный код, вероятность и режим выполнения механики не меняются.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

from make_fixture import Spans, WORK, validate_output


MASK = 0xffffffff
EPOCH = 0x029C55C0
MAX_DAY = 87653485
SEED = 0x475043
FIELDS = ('schema', 'seed', 'rng', 'last_day', 'pass_sequence')
PREFIX = 'cc_gpc_'
MONTH_DAYS = (31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)


def day_from_date(date):
    match = re.fullmatch(r'(\d{1,6})\.(\d{1,2})\.(\d{1,2})', date)
    if not match:
        raise ValueError('Неверная дата сохранения')
    year, month, day = map(int, match.groups())
    if not 1 <= month <= 12 or not 1 <= day <= MONTH_DAYS[month - 1]:
        raise ValueError('Дата вне 365-дневного календаря Victoria II')
    result = year * 365 + sum(MONTH_DAYS[:month - 1]) + day - 1
    if result > MAX_DAY:
        raise ValueError('Дата за пределами положительного int32 движка')
    return result


def xorshift32(value):
    if type(value) is not int or not 0 < value <= MASK:
        raise ValueError('Требуется ненулевой uint32 RNG')
    value ^= (value << 13) & MASK
    value ^= value >> 17
    value ^= (value << 5) & MASK
    return value & MASK


def inverse_xorshift32(value):
    if type(value) is not int or not 0 < value <= MASK:
        raise ValueError('Требуется ненулевой uint32 draw')

    def undo(number, shift, left):
        result = number
        for distance in range(shift, 32, shift):
            result ^= ((number << distance) & MASK) if left else number >> distance
        return result & MASK

    return undo(undo(undo(value, 5, True), 17, False), 13, True)


def checksum(state):
    result = 2166136261
    for field in FIELDS:
        word = state[field]
        if type(word) is not int or not 0 <= word <= MASK:
            raise ValueError(f'Поле {field} не uint32')
        for byte in word.to_bytes(4, 'little'):
            result = ((result ^ byte) * 16777619) & MASK
    return result


def encode(state):
    if state['schema'] != 1 or not state['seed'] or not state['rng'] or state['last_day'] > MAX_DAY:
        raise ValueError('Некорректное состояние schema 1')
    words = dict(state, checksum=checksum(state))
    flags = {'cc_gpc_v1_ready': 'yes'}
    for field in (*FIELDS, 'checksum'):
        for bit in range(32):
            flags[f'cc_gpc_v1_{field}_b{bit:02d}_{(words[field] >> bit) & 1}'] = 'yes'
    return flags


def _known_names():
    return {'cc_gpc_v1_ready', 'cc_gpc_v1_writing'} | {
        f'cc_gpc_v1_{field}_b{bit:02d}_{value}'
        for field in (*FIELDS, 'checksum') for bit in range(32) for value in (0, 1)}


def validate_existing(flags):
    known = _known_names()
    if any(name not in known for name in flags):
        raise ValueError('Неизвестное cc_gpc_ состояние; нужен явный --replace-existing-test-state')
    active = {name for name, value in flags.items() if value == 'yes'}
    if not active:
        return
    if 'cc_gpc_v1_writing' in active or 'cc_gpc_v1_ready' not in active:
        raise ValueError('Незавершённое cc_gpc_ состояние; нужен явный --replace-existing-test-state')
    words = {}
    for field in (*FIELDS, 'checksum'):
        value = 0
        for bit in range(32):
            stem = f'cc_gpc_v1_{field}_b{bit:02d}_'
            rails = [rail for rail in (0, 1) if stem + str(rail) in active]
            if len(rails) != 1:
                raise ValueError('Отсутствующий или двойной бит существующего cc_gpc_ состояния')
            value |= rails[0] << bit
        words[field] = value
    if words['schema'] != 1 or not words['seed'] or not words['rng'] or words['last_day'] > MAX_DAY or checksum(words) != words['checksum']:
        raise ValueError('Повреждённое существующее cc_gpc_ состояние')


def transform(original: bytes, *, fault=None, replace_existing_test_state=False):
    if fault not in (None, 'unknown-version', 'partial'):
        raise ValueError('Неизвестный режим повреждения fixture')
    source = original.decode('cp1251')
    date, root_flags = None, None
    for node in Spans(source).roots():
        if node.key == 'date':
            if date is not None or not isinstance(node.value, str):
                raise ValueError('Повторная или неверная корневая дата')
            date = node.value
        if node.key == 'flags':
            if root_flags is not None or not isinstance(node.value, list):
                raise ValueError('Повторный или неверный корневой flags')
            root_flags = node
    if date is None or root_flags is None:
        raise ValueError('Нет корневой даты или блока flags')
    last_day = day_from_date(date)
    rng = inverse_xorshift32(1)
    state = {'schema': 1, 'seed': SEED, 'rng': rng, 'last_day': last_day, 'pass_sequence': 0}
    flags = encode(state)
    injected = None
    if fault == 'unknown-version':
        injected = 'cc_gpc_v2_ready'
        flags[injected] = 'yes'
    elif fault == 'partial':
        injected = 'cc_gpc_v1_schema_b00_1'
        del flags[injected]
    replacements, previous = [], {}
    seen = set()
    for node in root_flags.value:
        if not node.key or not isinstance(node.value, str) or node.value not in ('yes', 'no'):
            raise ValueError('Некорректное присваивание глобального флага')
        normalized = node.key.lower()
        if normalized in seen:
            raise ValueError('Повторный глобальный флаг')
        seen.add(normalized)
        if normalized.startswith(PREFIX):
            previous[normalized] = node.value
            replacements.append({'start': node.key_start, 'end': node.end,
                                 'before': source[node.key_start:node.end], 'after': '', 'kind': 'remove_cc_gpc'})
    if not replace_existing_test_state:
        validate_existing(previous)
    eol = '\r\n' if '\r\n' in source else '\n'
    encoded = eol + eol.join('\t' + name + '=yes' for name in sorted(flags)) + eol
    insertion = root_flags.end - 1
    replacements.append({'start': insertion, 'end': insertion, 'before': '', 'after': encoded,
                         'kind': 'insert_cc_gpc'})
    output = source
    for item in sorted(replacements, key=lambda item: item['start'], reverse=True):
        output = output[:item['start']] + item['after'] + output[item['end']:]
    result = output.encode('cp1251')
    next_day = last_day + (1 if last_day % 2 == 0 else 2)
    if next_day > MAX_DAY:
        raise ValueError('Следующая страновая проверка за пределами диапазона')
    next_raw = xorshift32(rng)
    next_draw = (next_raw - 1) % 1000000
    if next_raw != 1 or next_draw >= 555:
        raise AssertionError('Не выполнена гарантия первого производственного успеха')
    manifest = {'scenario': 'production_mtth60_first_hit', 'fault': fault,
                'fault_flag': injected, 'replace_existing_test_state': bool(replace_existing_test_state),
                'source_date': date, 'source_day': last_day, 'source_raw_date': EPOCH + last_day * 24,
                'next_attempt_day': next_day, 'next_attempt_raw_date': EPOCH + next_day * 24,
                'state': state, 'checksum': checksum(state), 'active_flags': len(flags),
                'normal_active_flags': 193, 'removed_existing_flags': len(previous),
                'next_raw_draw': next_raw, 'next_draw': next_draw, 'threshold': 555,
                'mtth_months': 60, 'test_always': False,
                'source_sha256': hashlib.sha256(original).hexdigest(),
                'output_sha256': hashlib.sha256(result).hexdigest(), 'replacements': replacements}
    return result, manifest


def create_fixture(source: Path, output: Path, *, fault=None, replace_existing_test_state=False, work_root=WORK):
    output = validate_output(output, work_root)
    source = source.resolve(strict=True)
    report = output.with_suffix(output.suffix + '.json')
    if output == source or output.exists() or report.exists():
        raise FileExistsError('Исходный файл, fixture и manifest не перезаписываются')
    original = source.read_bytes()
    result, manifest = transform(original, fault=fault, replace_existing_test_state=replace_existing_test_state)
    if source.read_bytes() != original:
        raise RuntimeError('Исходный сейв изменился во время подготовки')
    manifest.update(source=str(source), output=str(output))
    created = []
    try:
        with output.open('xb') as saved:
            created.append(output)
            with report.open('x', encoding='utf-8') as metadata:
                created.append(report)
                saved.write(result)
                json.dump(manifest, metadata, ensure_ascii=False, indent=2)
                metadata.write('\n')
    except Exception:
        for path in created:
            path.unlink()
        raise
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fault', choices=('unknown-version', 'partial'))
    parser.add_argument('--replace-existing-test-state', action='store_true')
    args = parser.parse_args()
    manifest = create_fixture(args.source, args.output, fault=args.fault,
                              replace_existing_test_state=args.replace_existing_test_state)
    print(json.dumps({'ok': True, 'state': manifest['state'], 'active_flags': manifest['active_flags'],
                      'next_draw': manifest['next_draw'], 'threshold': manifest['threshold'],
                      'fault': manifest['fault'], 'output_sha256': manifest['output_sha256']}))


if __name__ == '__main__':
    main()
