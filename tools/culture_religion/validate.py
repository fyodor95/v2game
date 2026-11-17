"""Независимая проверка религиозных первых корок и сохранности распространения."""
from __future__ import annotations
import argparse
from collections import Counter
from decimal import Decimal
import json
from pathlib import Path
import re
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.chronology_import.validate_core_events import reconstruct_map, read, one, scalar, block, signature, expected_trigger
from tools.vic2test.scripts import parse

def population_pairs(root):
    result = Counter()
    for path in sorted((root / 'history/pops/1836.1.1').glob('*.txt')):
        for province in parse(read(path)):
            for pop in province.children:
                fields = {n.key: n.value for n in pop.children}
                if 'culture' in fields and Decimal(fields.get('size', '0')) > 0:
                    result[fields['culture'], fields['religion']] += Decimal(fields['size'])
    return result

def build_model(root):
    legacy = reconstruct_map(root)
    countries, dynamic = legacy['countries'], legacy['dynamic']
    for path in sorted((root / 'history/countries').glob('*.txt')):
        if path.name[:3] in countries:
            countries[path.name[:3]]['religion'] = one(parse(read(path)), 'religion').value
    religions = [r.key for g in parse(read(root / 'common/religion.txt')) for r in g.children if r.children]
    pairs = population_pairs(root)
    config = re.sub(r'#[^\n]*', '', read(root / 'autogen.cfg'))
    excluded_block = re.search(r'\bcountries_without_culture\s*=\s*\{([^{}]*)\}', config)
    excluded = set(re.findall(r'\b[A-Z][A-Z0-9]{2}\b', excluded_block[1])) if excluded_block else set()
    rows, index = [], 0
    def choose(tags, culture):
        for pred in (lambda t: countries[t]['civilized'], lambda t: t == culture[:3].upper()):
            matches = [t for t in tags if pred(t)]
            if len(matches) == 1: return matches[0]
        return None
    for old in legacy['rows']:
        culture = old['culture']
        for religion in religions:
            if (culture, religion) not in pairs: continue
            eligible = [t for t, c in countries.items() if t not in excluded and c['religion'] == religion]
            primary = [t for t in eligible if t not in dynamic and countries[t]['primary'] == culture]
            accepted = [t for t in eligible for c in countries[t]['accepted'] if c == culture]
            union = old['union'] if old['union'] in eligible else None
            additional = []
            def fallback():
                nonlocal index
                tag = dynamic[index % len(dynamic)]; index += 1
                return tag
            def extend(tags, u, omit=None):
                additional.extend(t for t in tags if t != omit)
                if u and u not in tags: additional.append(u)
            if len(primary) == 1:
                main = primary[0]; extend(accepted, None if main == union else union)
            elif len(primary) == 2 and union in primary:
                main = next(t for t in primary if t != union); additional = accepted + [union]
            elif len(primary) > 1:
                main = choose(primary, culture) or fallback()
                extend(primary, None, main); extend(accepted, None if union in primary else union)
            elif not union:
                main = fallback(); extend(accepted, None)
            elif len(accepted) == 1:
                main = accepted[0]; extend(accepted, union, main)
            else:
                main = choose(accepted, culture) or fallback(); extend(accepted, union, main)
            spread = list(dict.fromkeys([main] + additional))
            first = [t for t in spread if countries[t]['primary'] == culture or t == union or (t == main and t in dynamic)]
            rows.append(dict(culture=culture, religion=religion, main=main, union=union,
                             dynamic_fallback=main if main in dynamic else None, first=first, spread=spread))
    spread = {(row['culture'], tag) for row in legacy['rows'] + rows for tag in row['spread']}
    return dict(rows=rows, spread=spread, countries=countries, legacy=legacy)

def first_trigger(row, tag, countries):
    result = expected_trigger(row, tag, True)
    result.insert(2, scalar('pop_majority_religion', row['religion']))
    owner = one(result, 'owner'); recipient = one(owner.children, tag)
    if tag != row['dynamic_fallback']:
        recipient.children.append(scalar('country_religion', countries[tag]['religion']))
        if countries[tag]['civilized']: owner.children.insert(0, scalar('civilized', 'yes'))
    return result

def canonical(nodes):
    return tuple(sorted((n.key[1:-1] if n.key in ('"PHI"','"PAN"') else n.key, n.value or '', canonical(n.children)) for n in nodes))

def validate(root):
    model = build_model(root)
    expected = {(r['culture'], r['religion'], t): r for r in model['rows'] for t in r['first']}
    seen, spread_seen, ids, errors = Counter(), Counter(), set(), []
    loc = Counter(line.split(';', 1)[0] for line in read(root / 'localisation/adding_cores.csv').splitlines())
    loc_values={line.split(';',2)[0]:line.split(';',2)[1] for line in read(root / 'localisation/adding_cores.csv').splitlines() if ';' in line}
    for event in parse(read(root / 'events/adding_cores.txt')):
        event_id = '?'
        try:
            assert event.key=='province_event', 'Требуется провинциальное событие'
            body = event.children; event_id = one(body, 'id').value
            pending=list(body)
            while pending:
                node=pending.pop();pending.extend(node.children)
                assert not (node.children and node.key in ('PHI','PAN')), 'Зарезервированный тэг страны должен быть в кавычках'
            assert Counter(n.key for n in body)==Counter({k:1 for k in ('id','title','desc','trigger','mean_time_to_happen','option')}), 'Изменено расписание или структура события'
            assert one(body,'title').value=='EVTNAME'+event_id and one(body,'desc').value=='EVTDESC'+event_id, 'Неверные ссылки title/desc'
            assert event_id not in ids, 'Повторный ID'
            ids.add(event_id)
            trigger = one(body, 'trigger').children; option = one(body, 'option').children
            assert one(option,'name').value=='EVTOPTA'+event_id, 'Неверная ссылка опции'
            guarded = [n for n in option if n.key == 'if']
            effects = one(option, 'if').children if guarded else option
            tag = one(effects, 'add_core').value
            if guarded:
                culture = one(trigger, 'culture').value; religion = one(trigger, 'pop_majority_religion').value
                key = culture, religion, tag
                assert key in expected, f'Лишняя первая корка {key}'
                seen[key] += 1; row = expected[key]
                assert canonical(trigger) == canonical(first_trigger(row, tag, model['countries'])), 'Условия первой корки'
                assert signature(one(effects, 'limit').children) == signature(trigger), 'Неполный guard'
                required = [block('limit', *trigger), scalar('add_core', tag)]
                if tag == row['dynamic_fallback']:
                    required.append(block('owner', block(tag, scalar('primary_culture', culture), scalar('religion', religion))))
                assert canonical(effects) == canonical(required), 'Культура/религия/эффекты Д-тэга'
                assert len(option) == 2 and option[0].key == 'name', 'Эффекты вне guard'
                months = 4 * len(row['first'])
            else:
                culture = one(one(trigger, 'any_pop').children, 'has_pop_culture').value; key = culture, tag
                assert key in model['spread'], f'Лишнее распространение {key}'
                spread_seen[key] += 1
                assert canonical(trigger) == canonical(expected_trigger(dict(culture=culture), tag, False)), 'Условия распространения'
                assert canonical([n for n in option if n.key != 'name']) == canonical([scalar('add_core', tag)]), 'Эффекты распространения'
                months = 4
            mtth = one(body, 'mean_time_to_happen').children
            assert len(mtth) == 1 and Decimal(one(mtth, 'months').value) == months, 'Нормировка MTTH'
            for prefix in ('EVTNAME', 'EVTDESC', 'EVTOPTA'):
                assert loc[prefix + event_id] == 1 and re.search('[А-Яа-яЁё]',loc_values.get(prefix+event_id,'')), 'Локализация'
        except (AssertionError, ValueError, KeyError) as error:
            errors.append(f'{event_id}: {error}')
    errors += [f'Первая корка {key}: {seen[key]}' for key in expected if seen[key] != 1]
    errors += [f'Распространение {key}: {spread_seen[key]}' for key in model['spread'] if spread_seen[key] != 1]
    return dict(ok=not errors, errors=errors, pair_count=len(model['rows']), first_count=sum(seen.values()),
                spread_count=sum(spread_seen.values()), expected_first=len(expected), expected_spread=len(model['spread']),
                dynamic_assignments=sum(bool(r['dynamic_fallback']) for r in model['rows']),
                denominators={r['culture'] + '/' + r['religion']: len(r['first']) for r in model['rows']})

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd()); parser.add_argument('--output', type=Path)
    args = parser.parse_args(); result = validate(args.root)
    encoded = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output: args.output.write_text(encoded, encoding='utf-8')
    print(encoded); raise SystemExit(0 if result['ok'] else 1)
