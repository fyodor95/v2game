"""Фиксирует хеши собранного модуля и его исходников для проверок и установки."""
import hashlib
import json
from pathlib import Path
import subprocess

SOURCE = Path(__file__).resolve().parent
ROOT = SOURCE.parents[2]
BUILD = ROOT / '.work/gpc-build'
EXPECTED_EXE = '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    if digest(ROOT / 'v2game.exe') != EXPECTED_EXE:
        raise RuntimeError('EXE отличается от квалифицированной Victoria II 3.04')
    binary = BUILD / 'ccore_great_power_culture_cores.dll'
    content = binary.read_bytes()
    pe = int.from_bytes(content[60:64], 'little')
    if content[pe:pe + 4] != b'PE\0\0' or int.from_bytes(content[pe + 4:pe + 6], 'little') != 0x14c:
        raise RuntimeError('Ожидалась DLL Win32')
    sources = sorted(SOURCE.glob('*.h')) + [SOURCE / name for name in
        ('runtime.cpp', 'build_runtime.cmd', 'write_manifest.py', 'great_power_culture_cores.ini')]
    record = {'schema': 1, 'machine': 'x86', 'expected_exe_sha256': EXPECTED_EXE,
              'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'artifact': {'name': binary.name, 'bytes': len(content), 'sha256': digest(binary)},
              'sources': {path.name: digest(path) for path in sorted(sources)}}
    (BUILD / 'manifest.json').write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('Манифест модуля:', BUILD / 'manifest.json')


if __name__ == '__main__':
    main()
