"""Воспроизводимое распределение культур; только --apply меняет игровые файлы.

--baseline указывает на неизменяемый экспорт исходных игровых файлов, не на
рабочее дерево. Сначала формируются реестр и все результаты, затем проверяются
хеши рабочей копии; неожиданные изменения блокируют весь проход записи.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from decimal import Decimal
import hashlib
import itertools
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tools/creatinecore/gpc'))
sys.path.insert(0, str(ROOT / 'tools/vic2test'))
from make_fixture import Spans, scalar
from assignments import ABORIGINE_NAME, NEW, REGIONS

DATES = ('1836.1.1', '1861.4.14')
GENERIC = ('asian_minor', 'african_minor', 'native_american_minor')
NAME_TRANSLATIONS = {
    'Оцелотl': 'Оцелотль', 'Gonzбlez': 'Гонсалес', 'Gutiйrrez': 'Гутьеррес',
    'Guzmбn': 'Гусман', 'Henestrosa': 'Энестроса', 'Isael': 'Исаэль',
    'Jimйnez': 'Хименес', 'Juбrez': 'Хуарес', 'Lуpez': 'Лопес',
    'Matus': 'Матус', 'Mйrida': 'Мерида',
}


def russian_names(text):
    for old, new in NAME_TRANSLATIONS.items():
        text = re.sub(r'(?<!\w)'+re.escape(old)+r'(?!\w)', new, text)
    if re.search('[A-Za-z]', text):
        raise ValueError('Непереведённое имя в новом именном наборе: '+text)
    return text


def digest(data):
    return hashlib.sha256(data).hexdigest()


def dump(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def assignments():
    result = {}
    for source, old, region, cultures in REGIONS:
        for new, provinces in cultures.items():
            for value in provinces.split():
                key = (old, int(value))
                if key in result:
                    raise ValueError(f'Повторное назначение: {key}')
                result[key] = dict(culture=new, source=source, region=region)
    return result


def selected(culture, size, total):
    return size > 0 and (culture == 'aborigine' or culture in GENERIC and size * 4 > total)


def rewrite(raw, rules, path='fixture', names=None):
    """Менять только скаляр culture: никакой пересериализации POP и округления."""
    text = raw.decode('cp1251')
    replacements, records, composition = [], [], {}
    for province in Spans(text).roots():
        if not province.key.isdigit() or not isinstance(province.value, list):
            raise ValueError(f'Некорректная провинция: {path}/{province.key}')
        pid = int(province.key)
        pops = [p for p in province.value if isinstance(p.value, list) and p.single('size')]
        counts = Counter()
        for pop in pops:
            size = Decimal(scalar(pop, 'size'))
            if not size.is_finite() or size < 0:
                raise ValueError(f'Некорректный размер POP: {path}/{pid}')
            counts[scalar(pop, 'culture')] += size
        total = sum(counts.values())
        if pid in composition and any(counts[k] or composition[pid].get(k, 0) for k in (*GENERIC, 'aborigine')):
            raise ValueError(f'Повторная целевая провинция: {path}/{pid}')
        target = dict(counts)
        for old, size in sorted(counts.items()):
            if not selected(old, size, total):
                continue
            rule = rules.get((old, pid))
            if not rule:
                raise ValueError(f'Нет распределения для {path}/{pid}/{old}')
            new = rule['culture']
            changed = [pop for pop in pops if scalar(pop, 'culture') == old]
            if old != new:
                for pop in changed:
                    field = pop.single('culture')
                    token = '"' + new + '"' if text[field.start:field.end].startswith('"') else new
                    replacements.append((field.start, field.end, token))
                target[new] = target.get(new, Decimal(0)) + size
                del target[old]
            records.append(dict(path=path, province=pid, province_name=(names or {}).get(pid, ''),
                                old_culture=old, new_culture=new, size=str(size), total=str(total),
                                pop_blocks=len(changed), source=rule['source'], region=rule['region'],
                                rationale='Местный ареал/объединение по источнику; исходные POP не делятся, все остальные поля сохранены.'))
        # В исходном Australia1861 два нецелевых блока2482; сохраняем оба.
        if pid in composition:
            for culture, size in target.items():
                composition[pid][culture] = composition[pid].get(culture, Decimal(0)) + size
        else:
            composition[pid] = target
    for start, end, value in sorted(replacements, reverse=True):
        text = text[:start] + value + text[end:]
    return text.encode('cp1251'), records, composition


def masked_pops(raw):
    """Независимый инвариант: весь исходный текст, кроме значений culture."""
    text = raw.decode('cp1251')
    edits = []
    for province in Spans(text).roots():
        if not isinstance(province.value, list):
            continue
        for pop in province.value:
            if isinstance(pop.value, list) and pop.single('size'):
                field = pop.single('culture')
                if not field:
                    raise ValueError('POP без культуры')
                edits.append((field.start, field.end))
    for start, end in reversed(edits):
        text = text[:start] + '<culture>' + text[end:]
    return digest(text.encode('cp1251'))


def culture_data(raw):
    text = raw.decode('cp1251')
    groups = {g.key: g for g in Spans(text).roots()}
    cultures = {}
    for group in groups.values():
        for c in group.value:
            if isinstance(c.value, list):
                if c.key in cultures:
                    raise ValueError('Повтор культуры ' + c.key)
                cultures[c.key] = c
    return text, groups, cultures


def rgb(culture):
    node = culture.single('color')
    values = tuple(int(x.value) for x in node.value)
    if len(values) != 3 or not all(0 <= x <= 255 for x in values):
        raise ValueError('Недопустимый RGB ' + culture.key)
    return values


def lab(color):
    """sRGB D65 → CIELAB; расстояние CIE76, не расстояние необработанных RGB."""
    r, g, b = (v / 255 for v in color)
    r, g, b = (v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in (r, g, b))
    x = (.4124564*r + .3575761*g + .1804375*b) / .95047
    y = .2126729*r + .7151522*g + .0721750*b
    z = (.0193339*r + .1191920*g + .9503041*b) / 1.08883
    x, y, z = (v ** (1/3) if v > (6/29)**3 else v / (3*(6/29)**2) + 4/29 for v in (x, y, z))
    return 116*y-16, 500*(x-y), 200*(y-z)


def distance(left, right):
    return sum((a-b)**2 for a, b in zip(left, right)) ** .5


def province_edges(root):
    """Соседство по общим сторонам пикселей настоящей карты provinces.bmp."""
    definitions = {}
    for line in (root/'map/definition.csv').read_bytes().decode('cp1251').splitlines():
        parts = line.split(';')
        if parts[0].isdigit():
            # В исходнике1825 имеет223.; значение целое, файл не исправляем.
            channels = [Decimal(x) for x in parts[1:4]]
            if len(channels)!=3 or any(x!=int(x) or not 0<=x<=255 for x in channels):
                raise ValueError('Некорректная строка определения карты: '+line)
            r, g, b = map(int, channels)
            definitions[b | g << 8 | r << 16] = int(parts[0])
    data = (root/'map/provinces.bmp').read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    header, width, height, planes, depth, compression = struct.unpack_from('<IiiHHI', data, 14)
    if header != 40 or depth != 24 or compression != 0 or planes != 1:
        raise ValueError('Ожидалась несжатая 24-битная карта BMP')
    stride = (width * 3 + 3) & ~3
    edges, previous = set(), None
    for y in range(abs(height)):
        row = data[offset+y*stride:offset+y*stride+width*3]
        pixels = [definitions.get(b | g << 8 | r << 16, 0) for b, g, r in zip(row[::3], row[1::3], row[2::3])]
        for a, b in zip(pixels, pixels[1:] + pixels[:1]):
            if a != b and a and b:
                edges.add((min(a,b), max(a,b)))
        if previous:
            for a, b in zip(pixels, previous):
                if a != b and a and b:
                    edges.add((min(a,b), max(a,b)))
        previous = pixels
    return sorted(edges)


def palette(cultures, compositions, edges):
    neighbors = defaultdict(set)
    for comp in compositions:
        for a, b in edges:
            for c in comp.get(a, {}):
                for d in comp.get(b, {}):
                    if c != d:
                        neighbors[c].add(d)
                        neighbors[d].add(c)
        # Две культуры в одной провинции тоже могут соседствовать после миграций.
        for counts in comp.values():
            for c, d in itertools.combinations(sorted(counts), 2):
                neighbors[c].add(d)
                neighbors[d].add(c)
    colors = {k: rgb(v) for k, v in cultures.items()}
    labs = {k: lab(v) for k, v in colors.items()}
    candidates = [(c, lab(c)) for c in itertools.product(range(32, 225, 24), repeat=3)]
    candidates = [(c, v) for c, v in candidates if 30 <= v[0] <= 82]
    for culture in sorted(NEW, key=lambda k: (-len(neighbors[k]), k)):
        adjacent = [labs[n] for n in sorted(neighbors[culture]) if n in labs]
        all_labs = list(labs.values())
        def score(item):
            _, candidate = item
            return (min((distance(candidate, n) for n in adjacent), default=100),
                    min(distance(candidate, n) for n in all_labs))
        color, value = max((x for x in candidates if x[0] not in colors.values()), key=score)
        colors[culture], labs[culture] = color, value
    details = {}
    for c in sorted(NEW):
        distances = [distance(labs[c], labs[n]) for n in neighbors[c] if n in labs]
        details[c] = dict(rgb=colors[c], neighbors=sorted(neighbors[c]),
                         minimum_neighbor_delta_e=round(min(distances),3) if distances else None)
    return colors, details


def add_cultures(raw, colors):
    text, groups, cultures = culture_data(raw)
    if set(NEW) & cultures.keys():
        raise ValueError('Baseline уже содержит новые культуры')
    newline = '\r\r\n' if '\r\r\n' in text else '\r\n' if '\r\n' in text else '\n'
    additions = defaultdict(list)
    for code, (name, group, template) in sorted(NEW.items()):
        assert group in groups and template in cultures, code
        assert re.fullmatch(r'[a-z][a-z0-9_]*', code), code
        name.encode('cp1251')
        fields = []
        for key in ('first_names', 'last_names'):
            node = cultures[template].single(key)
            assert node and node.value, (code, template, key)
            fields.append('\t\t' + key + ' = ' + russian_names(text[node.start:node.end]))
        color = ' '.join(map(str, colors[code]))
        additions[group].append(newline.join(['\t'+code+' = {', '\t\tcolor = { '+color+' }', *fields, '\t}', '']))
    for group in sorted(additions, key=lambda g: groups[g].end, reverse=True):
        point = groups[group].end - 1
        union = groups[group].single('union')
        if union:
            point = text.rfind('\n', 0, union.key_start) + 1
        block = newline + '\t# Местные культуры: tools/culture-history/assignments.py' + newline
        block += newline.join(additions[group])
        text = text[:point] + block + text[point:]
    return text.encode('cp1251')


def build(baseline, root):
    sources = json.loads((HERE/'sources.json').read_text(encoding='utf-8'))
    rules = assignments()
    names = {}
    for p in (root/'history/provinces').rglob('*.txt'):
        parts = p.stem.split(' - ', 1)
        if parts[0].isdigit(): names[int(parts[0])] = parts[-1]
    raw_cultures = (baseline/'common/cultures.txt').read_bytes()
    _, _, old_cultures = culture_data(raw_cultures)
    assert all(r['culture'] in old_cultures or r['culture'] in NEW for r in rules.values())
    assert all(r['source'] in sources for r in rules.values())
    outputs, records, compositions, file_records, population_files = {}, [], [], {}, {}
    for date in DATES:
        comp = {}
        for file in sorted((baseline/'history/pops'/date).glob('*.txt')):
            path = file.relative_to(baseline).as_posix()
            raw = file.read_bytes()
            rewritten, entries, provinces = rewrite(raw, rules, path, names)
            assert not comp.keys() & provinces.keys(), 'Повтор ID провинции между файлами'
            comp.update(provinces)
            records.extend(dict(date=date, **e) for e in entries)
            assert masked_pops(raw) == masked_pops(rewritten), path
            population_files[path] = dict(before_sha256=digest(raw), after_sha256=digest(rewritten),
                                          unchanged_pop_fields_sha256=masked_pops(raw))
            if rewritten != raw:
                outputs[path] = rewritten
                file_records[path] = population_files[path]
        compositions.append(comp)
    assert {r['new_culture'] for r in records} >= set(NEW), 'Неиспользуемые новые культуры'
    counts = {date: dict(Counter(r['old_culture'] for r in records if r['date']==date)) for date in DATES}
    assert counts[DATES[0]] == dict(asian_minor=33, african_minor=169, native_american_minor=116, aborigine=44), counts
    assert counts[DATES[1]] == dict(asian_minor=33, african_minor=165, native_american_minor=85, aborigine=40), counts
    australian = Counter()
    for record in records:
        if record['date']==DATES[0] and record['old_culture']=='aborigine':
            australian[record['new_culture']] += Decimal(record['size'])
    assert max(australian, key=australian.get)=='aborigine', australian
    colors, color_details = palette(old_cultures, compositions, province_edges(root))
    outputs['common/cultures.txt'] = add_cultures(raw_cultures, colors)
    original_loc = (baseline/'localisation/all_orig.csv').read_bytes()
    target_loc, count = re.subn(rb'(?m)^(aborigine;)[^;\r\n]*', lambda m: m[1]+ABORIGINE_NAME.encode('cp1251'), original_loc)
    assert count == 1
    outputs['localisation/all_orig.csv'] = target_loc
    rows = ['#CODE;Русский;;;;;;;;;;;;;x'] + [f'{k};{v[0]};;;;;;;;;;;;;x' for k,v in sorted(NEW.items())]
    outputs['localisation/culture_history.csv'] = ('\r\n'.join(rows)+'\r\n').encode('cp1251')
    for tag in ('GBS', 'CNG', 'SOK'):
        matches = list((baseline/'history/countries').glob(tag+' - *.txt'))
        assert len(matches)==1
        path = matches[0].relative_to(baseline).as_posix()
        raw = matches[0].read_bytes()
        if tag=='GBS':
            changed, n = re.subn(rb'(primary_culture\s*=\s*)african_minor\b', rb'\g<1>papel_balanta', raw)
        else:
            changed, n = re.subn(rb'(?m)^[ \t]*culture[ \t]*=[ \t]*african_minor[^\r\n]*(?:\r*\n|$)', b'', raw)
        assert n==1, (tag,n)
        outputs[path] = changed
    for path, content in outputs.items():
        before = (baseline/path).read_bytes() if (baseline/path).exists() else None
        file_records.setdefault(path, dict(before_sha256=digest(before) if before is not None else None, after_sha256=digest(content)))
    report = dict(schema=1, dates=list(DATES), new_cultures=len(NEW), total_cultures=len(old_cultures)+len(NEW),
                  selection_counts=counts, australian_1836={k:str(v) for k,v in sorted(australian.items())},
                  sources=sources, files=file_records, population_files=population_files, records=records, colors=color_details,
                  assignments_sha256=digest((HERE/'assignments.py').read_bytes()),
                  sources_sha256=digest((HERE/'sources.json').read_bytes()),
                  province_map_sha256=digest((root/'map/provinces.bmp').read_bytes()))
    return outputs, report


def check(root, report):
    assert report['assignments_sha256']==digest((HERE/'assignments.py').read_bytes()), 'Реестр назначений устарел'
    assert report['sources_sha256']==digest((HERE/'sources.json').read_bytes()), 'Реестр источников устарел'
    assert report['province_map_sha256']==digest((root/'map/provinces.bmp').read_bytes()), 'Карта для выбора цветов изменилась'
    for path, entry in report['files'].items():
        raw = (root/path).read_bytes()
        assert digest(raw)==entry['after_sha256'], 'Изменён файл '+path
        if path.startswith('history/pops/'):
            assert masked_pops(raw)==entry['unchanged_pop_fields_sha256'], path
    actual_pop_paths = {p.relative_to(root).as_posix() for date in DATES for p in (root/'history/pops'/date).glob('*.txt')}
    assert actual_pop_paths==report['population_files'].keys(), 'Список POP-файлов изменён'
    for path, entry in report['population_files'].items():
        raw=(root/path).read_bytes()
        assert digest(raw)==entry['after_sha256'], 'Изменён POP-файл '+path
        assert masked_pops(raw)==entry['unchanged_pop_fields_sha256'], path
    _, groups, cultures = culture_data((root/'common/cultures.txt').read_bytes())
    assert len(cultures)==report['total_cultures']
    assert scalar(groups['central_american_cultures'], 'union')=='UCA'
    assert 'central_american' in [c.key for c in groups['central_american_cultures'].value]
    uca = next((root/'history/countries').glob('UCA - *.txt'))
    assert next(n.value for n in Spans(uca.read_bytes().decode('cp1251')).roots() if n.key=='primary_culture')=='central_american'
    for file in (root/'history/countries').glob('*.txt'):
        pending=list(Spans(file.read_bytes().decode('cp1251')).roots())
        while pending:
            node=pending.pop()
            if isinstance(node.value,list): pending.extend(node.value)
            elif node.key in ('primary_culture','culture'): assert node.value not in GENERIC, (file,node.value)
    for code in NEW:
        assert rgb(cultures[code])==tuple(report['colors'][code]['rgb']), code
        for name in ('first_names','last_names'):
            assert cultures[code].single(name).value, (code,name)
            assert all(not re.search('[A-Za-z]',str(n.value)) for n in cultures[code].single(name).value), (code,name)
    loc = defaultdict(list)
    for file in (root/'localisation').glob('*.csv'):
        for line in file.read_bytes().decode('cp1251').splitlines():
            parts=line.split(';')
            if parts[0] in NEW or parts[0]=='aborigine': loc[parts[0]].append(parts[1])
    for code, (name, _, _) in NEW.items():
        assert loc[code]==[name], (code,loc[code])
        assert re.search('[А-Яа-яЁё]',name) and not re.search('[A-Za-z]',name), code
    assert loc['aborigine']==[ABORIGINE_NAME]
    pairs, totals, pop_blocks = {}, {}, {}
    for date in DATES:
        found, total, count = set(), Decimal(0), 0
        for file in (root/'history/pops'/date).glob('*.txt'):
            for province in Spans(file.read_bytes().decode('cp1251')).roots():
                counts = Counter()
                for pop in province.value:
                    if isinstance(pop.value,list) and pop.single('size'):
                        size = Decimal(scalar(pop,'size'));culture = scalar(pop,'culture')
                        assert culture in cultures, (file,culture)
                        counts[culture] += size
                        total += size;count += 1
                        if size>0: found.add((culture,scalar(pop,'religion')))
                assert all(not selected(k,counts[k],sum(counts.values())) for k in GENERIC), (file,province.key)
        pairs[date]=len(found);totals[date]=str(total);pop_blocks[date]=count
    return dict(ok=True, new_cultures=len(NEW), total_cultures=len(cultures), pairs=pairs,
                population_totals=totals,pop_blocks=pop_blocks,uca=True,files=len(report['files']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--report', type=Path, default=HERE/'registry.json')
    args = parser.parse_args()
    if args.check:
        report=json.loads(args.report.read_text(encoding='utf-8'))
        print(json.dumps(check(args.root,report), ensure_ascii=False, indent=2))
        return
    if args.baseline is None or args.baseline.resolve()==args.root.resolve():
        parser.error('Нужен отдельный неизменяемый --baseline')
    outputs, report = build(args.baseline,args.root)
    if args.apply:
        # Preflight всех файлов до первой записи. Разрешён повторный запуск с тем же результатом.
        for path, entry in report['files'].items():
            target=args.root/path
            actual=digest(target.read_bytes()) if target.exists() else None
            if actual not in (entry['before_sha256'],entry['after_sha256']):
                raise ValueError('Рабочий файл изменён независимо: '+path)
        for path, raw in outputs.items():
            target=args.root/path
            if not target.exists() or target.read_bytes()!=raw:
                target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
        check(args.root,report)
    dump(args.report,report)
    print(json.dumps(dict(applied=args.apply, new_cultures=len(NEW), records=len(report['records']),
                          files=len(outputs), selection_counts=report['selection_counts'],
                          australian_1836=report['australian_1836']),ensure_ascii=False,indent=2))


if __name__=='__main__':
    main()
