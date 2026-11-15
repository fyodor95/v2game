"""Сверить точное изменение всех корок с независимым oracle до прохода."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

from save_oracle import Block, preview, read_entries


def _tag(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Z0-9]{3}', value):
        raise ValueError(f'Недопустимый тег страны: {value!r}')
    return value


def _number(value):
    if type(value) is not int or value <= 0:
        raise ValueError(f'Недопустимый ID провинции: {value!r}')
    return value


def _provinces(rows):
    if not isinstance(rows, list) or not rows:
        raise ValueError('Oracle должен содержать непустой список всех провинций')
    result = {}
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError('Некорректная запись провинции')
        number = _number(row.get('id'))
        if number in result:
            raise ValueError(f'Повторный ID провинции {number}')
        cores = row.get('cores')
        if not isinstance(cores, list):
            raise ValueError(f'Нет списка корок провинции {number}')
        checked = [_tag(core) for core in cores]
        if len(set(checked)) != len(checked):
            raise ValueError(f'Повторная корка провинции {number}')
        owner = row.get('owner')
        if owner is not None:
            _tag(owner)
        result[number] = set(checked)
    return result


def _pairs(rows, provinces, allowed_tags=None):
    if not isinstance(rows, list):
        raise ValueError('Некорректный список ожидаемых добавлений')
    result = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError('Некорректная запись ожидаемой корки')
        pair = (_number(row.get('province')), _tag(row.get('country')))
        if pair[0] not in provinces:
            raise ValueError(f'Неизвестная провинция ожидаемой корки {pair[0]}')
        if pair in result:
            raise ValueError(f'Повторная ожидаемая пара {pair}')
        if allowed_tags is not None and pair[1] not in allowed_tags:
            raise ValueError(f'Получатель не входит в квалифицированный набор GP: {pair[1]}')
        if pair[1] in provinces[pair[0]]:
            raise ValueError(f'Ожидаемая новая корка уже существует: {pair}')
        result.add(pair)
    return result


def _core_pairs(provinces):
    return {(number, tag) for number, cores in provinces.items() for tag in cores}


def _rows(pairs):
    return [{'province': number, 'country': tag} for number, tag in sorted(pairs)]


def _validate_raw_save(save):
    # preview нормализует повторные core= в set. Проверяем исходный документ
    # отдельно, чтобы malformed .v2 не превратился в успешный нормализованный отчёт.
    provinces = []
    for entry in read_entries(save, implicit_root=True):
        if entry.key and entry.key.isdigit() and isinstance(entry.value, Block):
            owner = entry.value.single('owner')
            provinces.append({'id': int(entry.key), 'owner': owner,
                              'cores': entry.value.values('core')})
    return _provinces(provinces)


def compare_delta(before, after, *, require_no_remaining=False):
    """Сравнить два уже разобранных oracle; исходные структуры не изменяются."""
    if not isinstance(before, dict) or before.get('ok') is not True or before.get('complete') is not True:
        raise ValueError('Исходный oracle не завершён успешно')
    if not isinstance(after, dict) or after.get('ok') is not True or after.get('complete') is not True:
        raise ValueError('Итоговый oracle не завершён успешно')
    old = _provinces(before.get('provinces'))
    new = _provinces(after.get('provinces'))
    old_tags, new_tags = before.get('great_powers'), after.get('great_powers')
    if not isinstance(old_tags, list) or not isinstance(new_tags, list) or not old_tags:
        raise ValueError('Отсутствует квалифицированный список GP')
    for tags in (old_tags, new_tags):
        for tag in tags:
            _tag(tag)
        if len(set(tags)) != len(tags):
            raise ValueError('Повторный GP-тег')
    if set(old_tags) != set(new_tags):
        raise ValueError('Наборы GP до и после различаются')
    for state in (before, after):
        if state.get('great_powers_source') != 'explicit_qualified_tags':
            raise ValueError('GP должны быть заданы квалифицированными тегами')
        if not isinstance(state.get('save_sha256'), str) or not re.fullmatch(r'[0-9a-fA-F]{64}', state['save_sha256']):
            raise ValueError('Отсутствует SHA-256 сохранения')
    expected = _pairs(before.get('additions'), old, set(old_tags))
    remaining = _pairs(after.get('additions'), new, set(new_tags))
    actual = _core_pairs(new) - _core_pairs(old)
    removed = _core_pairs(old) - _core_pairs(new)
    missing, unexpected = expected - actual, actual - expected
    missing_provinces = sorted(old.keys() - new.keys())
    additional_provinces = sorted(new.keys() - old.keys())
    ok = not (missing or unexpected or removed or missing_provinces or additional_provinces or
              (require_no_remaining and remaining))
    return {'ok': ok, 'complete': ok, 'expected': _rows(expected), 'additions': _rows(actual),
            'missing': _rows(missing), 'unexpected': _rows(unexpected), 'removed': _rows(removed),
            'missing_provinces': missing_provinces, 'additional_provinces': additional_provinces,
            'remaining_candidates': _rows(remaining), 'require_no_remaining': bool(require_no_remaining),
            'before_sha256': before['save_sha256'].lower(), 'after_sha256': after['save_sha256'].lower()}


def check_delta(before_oracle: Path, after_save: Path, game_root: Path, great_powers,
                *, require_no_remaining=False):
    raw = before_oracle.read_bytes()
    before = json.loads(raw.decode('utf-8-sig'))
    if not isinstance(great_powers, list) or not great_powers:
        raise ValueError('Нужен непустой список тегов GP')
    for tag in great_powers:
        _tag(tag)
    if len(set(great_powers)) != len(great_powers):
        raise ValueError('Повторные GP-теги')
    initial_sha = hashlib.sha256(after_save.read_bytes()).hexdigest()
    raw_cores = _validate_raw_save(after_save)
    after = preview(after_save, game_root, great_powers)
    if raw_cores != _provinces(after.get('provinces')):
        raise ValueError('Сырое сохранение и итоговый oracle содержат разные корки')
    if after.get('save_sha256') != initial_sha or hashlib.sha256(after_save.read_bytes()).hexdigest() != initial_sha:
        raise RuntimeError('Итоговый сейв изменился во время проверки')
    result = compare_delta(before, after, require_no_remaining=require_no_remaining)
    result.update(before_oracle_sha256=hashlib.sha256(raw).hexdigest(),
                  before_oracle=str(before_oracle), after_save=str(after_save))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-oracle', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--gp', nargs='+', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-no-remaining', action='store_true')
    args = parser.parse_args()
    if args.output.exists() or args.output.resolve() in {args.before_oracle.resolve(), args.after.resolve()}:
        raise FileExistsError('Отчёт и исходные файлы не перезаписываются')
    result = check_delta(args.before_oracle, args.after, args.game_root, args.gp,
                         require_no_remaining=args.require_no_remaining)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, ensure_ascii=False, indent=2)
        output.write('\n')
    print(json.dumps({'ok': result['ok'], 'additions': len(result['additions']),
                      'missing': len(result['missing']), 'unexpected': len(result['unexpected']),
                      'removed': len(result['removed'])}))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
