"""Проверка копий пользовательских сейвов через нативный CLI-мост."""
import argparse
import ctypes
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / 'tools/vic2test/cli.py'


def command(*args):
    result = subprocess.run([sys.executable, str(CLI), *map(str, args)], cwd=ROOT,
                            capture_output=True, env={**os.environ, 'PYTHONIOENCODING': 'utf-8'}, timeout=400)
    value = json.loads(result.stdout.decode('utf-8'))
    if result.returncode or not value.get('ok'):
        raise RuntimeError(json.dumps(value, ensure_ascii=False))
    return value


def wait_exit(pid):
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.restype = w.HANDLE
    k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
    k.CloseHandle.argtypes = [w.HANDLE]
    process = k.OpenProcess(0x100000, False, pid)
    if process:
        try:
            if k.WaitForSingleObject(process, 20000) != 0:
                raise RuntimeError('Тестовый процесс не завершился')
        finally:
            k.CloseHandle(process)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    parser.add_argument('--saves-root', type=Path, required=True)
    parser.add_argument('--names', nargs='+', default=['1841.v2', '1891.v2', '1931.v2'])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--auto-events', action='store_true', help='Выбирать первые опции фоновых событий в тестовой копии')
    args = parser.parse_args()
    environment = args.environment.resolve()
    if not environment.is_relative_to((ROOT / '.work').resolve()):
        raise ValueError('Тестовая среда должна находиться внутри .work')
    metadata = json.loads((environment / 'environment.json').read_text(encoding='utf-8'))
    userdata = Path(metadata['userdata']).resolve()
    if userdata != environment / 'userdata':
        raise ValueError('Пользовательский каталог должен принадлежать тестовой среде')
    savedir = userdata / 'save games'
    records = []
    try:
        for index, name in enumerate(args.names):
            if Path(name).name != name:
                raise ValueError('Требуется имя файла без пути')
            source = args.saves_root / name
            original = hashlib.sha256(source.read_bytes()).hexdigest()
            shutil.copy2(source, savedir / name)
            started = command('start', '--environment', environment, '--save', name, '--hidden')
            before = command('status', '--environment', environment)
            auto = ['--auto-events'] if args.auto_events else []
            advanced = command('advance', '--environment', environment, '--days', 90 if name == '1931.v2' else 30, '--timeout', 300, *auto)
            assert advanced['completed'] and advanced['date_raw'] >= before['date_raw']
            output_name = 'ccore_' + uuid.uuid4().hex[:8]
            saved = command('save', '--environment', environment, '--name', output_name)
            deadline = time.monotonic() + 60
            result = savedir / (output_name + '.v2')
            while (not result.exists() or result.stat().st_size == 0) and time.monotonic() < deadline:
                time.sleep(.25)
            assert result.is_file() and result.stat().st_size > 0
            command('stop', '--environment', environment)
            wait_exit(started['pid'])
            reopened = command('start', '--environment', environment, '--save', result.name, '--hidden')
            reloaded = command('status', '--environment', environment)
            assert reloaded['date_raw'] == advanced['date_raw']
            final = command('advance', '--environment', environment, '--days', 1, '--timeout', 120, *auto)
            command('stop', '--environment', environment)
            wait_exit(reopened['pid'])
            assert hashlib.sha256(source.read_bytes()).hexdigest() == original
            records.append({'source': name, 'sha256': original, 'start': before,
                            'advance': advanced, 'save': saved, 'reload': reloaded, 'final': final})
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps({'ok': True, 'complete': len(records) == len(args.names), 'runs': records}, ensure_ascii=False, indent=2), encoding='utf-8')
    except Exception as error:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps({'ok': False, 'runs': records, 'error': str(error)}, ensure_ascii=False, indent=2), encoding='utf-8')
        raise


if __name__ == '__main__':
    main()
