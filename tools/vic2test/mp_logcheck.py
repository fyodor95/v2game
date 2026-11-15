"""Сверка скопированных журналов настоящей сетевой партии.

Сравниваются данные одного завершённого барьера экспорта. Совпадение журналов
не заменяет независимую проверку правил выдачи корок и факта общей MP-сессии.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re


PASS = re.compile(r'GPC runtime: pass=(\d+) day=(\d+) additions=(\d+) '
                  r'elapsed_ms=(\d+) rng=(\d+) cores=([0-9A-Fa-f]{8}) state=([0-9A-Fa-f]{8})(?:\s|$)')
INIT = re.compile(r'GPC runtime: initialized day=(\d+) seed=(\d+)(?:\s|$)')
VERSION = re.compile(r'GPC runtime: version=(\d+) primary_only=(\d+) all_ties=(\d+) '
                     r'enabled=(\d+) months=(\d+) threshold=(\d+)(?:\s|$)')
SYNC = re.compile(r'\bSYNC n=(\d+) date=(\d{4}-\d{2}-\d{2} \d{2}:\d{2}) '
                  r'sum=(\d+)/(\d+)(?:\s|$)')
AUX = re.compile(r'\baux=(\d+)/(\d+)(?:\s|$)')


def _source(path):
    path = Path(path)
    record = {'path': str(path), 'status': 'missing'}
    try:
        raw = path.read_bytes()
    except FileNotFoundError:
        return record, ''
    except OSError as error:
        record.update(status='unreadable', error=str(error))
        return record, ''
    record.update(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())
    try:
        text = raw.decode('utf-8-sig')
    except UnicodeError as error:
        record.update(status='invalid_encoding', error=str(error))
        return record, ''
    record['status'] = 'present' if text.strip() else 'empty'
    return record, text


def _gpc(text, role, errors):
    passes, initializations, versions, incomplete = [], [], [], []
    seen = set()
    previous = None
    for number, line in enumerate(text.splitlines(), 1):
        if 'GPC' in line and re.search(r'\bERROR\b', line):
            errors.append(f'{role}: GPC ERROR, строка {number}')
        match = PASS.search(line)
        if match:
            sequence, day, additions, elapsed, rng = map(int, match.groups()[:5])
            # elapsed — локальное измерение, не часть синхронного состояния.
            entry = {'pass': sequence, 'day': day, 'additions': additions, 'rng': rng,
                     'cores': match[6].upper(), 'state': match[7].upper()}
            if sequence in seen or (previous and (sequence <= previous['pass'] or day <= previous['day'])):
                errors.append(f'{role}: повтор или нарушение порядка проходов GPC, строка {number}')
            seen.add(sequence)
            previous = entry
            passes.append(entry)
        elif 'GPC runtime: pass=' in line:
            incomplete.append(number)
        match = INIT.search(line)
        if match:
            initializations.append({'day': int(match[1]), 'seed': int(match[2])})
        elif 'GPC runtime: initialized ' in line:
            incomplete.append(number)
        match = VERSION.search(line)
        if match:
            versions.append(dict(zip(('version', 'primary_only', 'all_ties', 'enabled', 'months', 'threshold'),
                                     map(int, match.groups()))))
        elif 'GPC runtime: version=' in line:
            incomplete.append(number)
    unique_versions = {tuple(sorted(version.items())) for version in versions}
    if len(unique_versions) > 1:
        errors.append(f'{role}: в журнале смешаны разные конфигурации GPC')
    version = versions[-1] if versions else None
    if version and version['enabled'] == 0 and (passes or initializations):
        errors.append(f'{role}: выключенная механика выполняла проходы или инициализацию')
    return {'passes': passes, 'initializations': initializations, 'version': version,
            'incomplete_lines': incomplete}


def _native(text, role, errors):
    rows = {}
    incomplete = []
    for number, line in enumerate(text.splitlines(), 1):
        for real in re.finditer(r'\bfirst_real=([^\s]+)', line):
            if real[1] != '-':
                errors.append(f'{role}: first_real={real[1]}, строка {number}')
        if re.search(r'\bFIRST real OOS\b|\bOOS (?:hit|n)=', line):
            errors.append(f'{role}: нативное расхождение OOS, строка {number}')
        match = SYNC.search(line)
        if not match:
            if re.search(r'\bSYNC n=', line):
                incomplete.append(number)
            continue
        left, right = int(match[3]), int(match[4])
        entry = {'sum': left}
        if left != right:
            errors.append(f'{role}: локальный/удалённый checksum различаются {match[2]}, строка {number}')
        aux = AUX.search(line)
        if aux:
            entry['aux'] = int(aux[1])
            if int(aux[1]) != int(aux[2]):
                errors.append(f'{role}: локальный/удалённый aux различаются {match[2]}, строка {number}')
        if match[2] in rows and rows[match[2]] != entry:
            errors.append(f'{role}: разные checksum одной даты {match[2]}')
        rows[match[2]] = entry
    return {'dates': rows, 'incomplete_lines': incomplete}


def compare_logs(hostlog, clientlog, hostoos, clientoos, *,
                 require_native_sync=False, require_gpc_passes=0):
    """ok — нет опровергающих данных и выполнены обязательные требования.

    complete — оба вида доказательств полны на переданном барьере. Пропущенные
    необязательные журналы дают ok=True, complete=False, status=inconclusive.
    """
    if isinstance(require_gpc_passes, bool) or not isinstance(require_gpc_passes, int) or require_gpc_passes < 0:
        raise ValueError('Число обязательных проходов должно быть неотрицательным целым')
    sources, texts = {}, {}
    for name, path in (('host_gpc', hostlog), ('client_gpc', clientlog),
                       ('host_native', hostoos), ('client_native', clientoos)):
        sources[name], texts[name] = _source(path)
    errors, reasons = [], []
    hg = _gpc(texts['host_gpc'], 'host', errors)
    cg = _gpc(texts['client_gpc'], 'client', errors)
    native_errors = []
    hn = _native(texts['host_native'], 'host', native_errors)
    cn = _native(texts['client_native'], 'client', native_errors)

    gpc_sources = all(sources[name]['status'] == 'present' for name in ('host_gpc', 'client_gpc'))
    gpc_equal = hg['passes'] == cg['passes'] and hg['initializations'] == cg['initializations']
    if gpc_sources and not gpc_equal:
        errors.append('GPC: последовательности проходов или init seed/day хоста и клиента различаются')
    if gpc_sources and hg['version'] != cg['version']:
        errors.append('GPC: конфигурации хоста и клиента различаются')
    disabled = bool(hg['version'] and hg['version'] == cg['version'] and hg['version']['enabled'] == 0)
    gpc_complete = bool(gpc_sources and gpc_equal and hg['version'] and hg['version'] == cg['version'] and
                        not hg['incomplete_lines'] and not cg['incomplete_lines'] and
                        (hg['passes'] or disabled))
    if not gpc_complete:
        reasons.append('GPC: недостаточно полных парных данных')
    gpc_invalid = bool(errors)
    if require_gpc_passes and (not gpc_complete or disabled or len(hg['passes']) < require_gpc_passes):
        errors.append(f'GPC: не подтверждены обязательные {require_gpc_passes} проходов на обоих участниках')

    native_sources = all(sources[name]['status'] == 'present' for name in ('host_native', 'client_native'))
    host_dates, client_dates = hn['dates'], cn['dates']
    common = sorted(host_dates.keys() & client_dates.keys())
    for date in common:
        if host_dates[date] != client_dates[date]:
            native_errors.append(f'Native: checksum хоста и клиента различаются на {date}')
    missing_host = sorted(client_dates.keys() - host_dates.keys())
    missing_client = sorted(host_dates.keys() - client_dates.keys())
    native_complete = bool(native_sources and common and not missing_host and not missing_client and
                           not hn['incomplete_lines'] and not cn['incomplete_lines'])
    if not native_complete:
        reasons.append('Native: журнал отсутствует, пуст или переданный барьер покрыт не полностью')
    native_invalid = bool(native_errors)
    if require_native_sync and not native_complete:
        native_errors.append('Native: обязательная полная парная проверка SYNC не подтверждена')
    errors.extend(native_errors)
    complete = gpc_complete and native_complete and not errors
    return {'ok': not errors, 'complete': complete, 'errors': errors, 'inconclusive': reasons,
            'sources': sources,
            'proof': {'gpc': {'status': 'invalid' if gpc_invalid else 'complete' if gpc_complete else 'inconclusive',
                              'disabled': disabled, 'pass_count': len(hg['passes']),
                              'host': hg, 'client': cg},
                      'native_sync': {'status': 'invalid' if native_invalid else 'complete' if native_complete else 'inconclusive',
                                      'common_dates': common, 'missing_host_dates': missing_host,
                                      'missing_client_dates': missing_client, 'host': hn, 'client': cn}}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-log', type=Path, required=True)
    parser.add_argument('--client-log', type=Path, required=True)
    parser.add_argument('--host-oos', type=Path, required=True)
    parser.add_argument('--client-oos', type=Path, required=True)
    parser.add_argument('--require-native-sync', action='store_true')
    parser.add_argument('--require-gpc-passes', type=int, default=0)
    args = parser.parse_args()
    result = compare_logs(args.host_log, args.client_log, args.host_oos, args.client_oos,
                          require_native_sync=args.require_native_sync,
                          require_gpc_passes=args.require_gpc_passes)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
