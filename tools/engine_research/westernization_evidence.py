"""Извлечь код для отчёта о вестернизации, без запуска игры."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
EXE_SHA256 = '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648'
RANGES = {
    'civilize': (0x542370, 0x542BB8),
    'reform_fractions': (0x547030, 0x5471C4),
    'research_setter': (0x5366F0, 0x536766),
    'research_update': (0x507770, 0x508322),
    'research_daily': (0x538150, 0x5381F3),
    'research_project': (0x579EE0, 0x579F7A),
    'research_start': (0x513930, 0x513A26),
    'great_power_list': (0x68C5C0, 0x68C990),
    'signed_division': (0xAC02A0, 0xAC0350),
    'floor_float': (0x401000, 0x401026),
    'float_to_integer': (0xB31C00, 0xB31CB0),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dumpbin', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / '.work'):
        parser.error('Результаты должны находиться внутри .work')
    raw = (ROOT / 'v2game.exe').read_bytes()
    actual = hashlib.sha256(raw).hexdigest()
    if actual != EXE_SHA256:
        raise ValueError('Другой EXE: требуется новый анализ, ' + actual)
    pe = struct.unpack_from('<I', raw, 0x3C)[0]
    base = struct.unpack_from('<I', raw, pe + 52)[0]
    sections = []
    for index in range(struct.unpack_from('<H', raw, pe + 6)[0]):
        offset = pe + 24 + struct.unpack_from('<H', raw, pe + 20)[0] + index * 40
        _, rva, size, position = struct.unpack_from('<IIII', raw, offset + 8)
        sections.append((rva, size, position))

    def read(va, size):
        for rva, length, position in sections:
            if rva <= va - base and va - base + size <= rva + length:
                offset = position + va - base - rva
                return raw[offset:offset + size]
        raise ValueError(f'VA вне секций: {va:X}')

    output.mkdir(parents=True, exist_ok=True)
    result = dict(exe_sha256=actual, image_base=hex(base), ranges={})
    for name, (start, end) in RANGES.items():
        data = read(start, end - start)
        process = subprocess.run([str(args.dumpbin.resolve()), '/nologo', '/disasm:bytes',
                                  f'/range:0x{start:x},0x{end - 1:x}', str(ROOT / 'v2game.exe')],
                                 capture_output=True, check=True)
        listing = process.stdout.decode('ascii')
        (output / (name + '.asm.txt')).write_bytes(listing.replace('\r\n', '\n').replace('\r', '').encode('utf-8'))
        result['ranges'][name] = dict(va=hex(start), rva=hex(start - base), bytes=len(data),
                                     sha256=hashlib.sha256(data).hexdigest())
    result['constants'] = {hex(va): struct.unpack('<d', read(va, 8))[0]
                           for va in (0xE45660, 0xE45840)}
    result['constants']['0xe45e74_float'] = struct.unpack('<f', read(0xE45E74, 4))[0]
    inputs = [ROOT / name for name in ('common/defines.lua', 'common/issues.txt',
              'common/technology.txt', 'common/on_actions.txt',
              'z_CreatineCore/creatinecore_settings.ini', 'z_CreatineCore/creatinecore.dll')]
    inputs += sorted((ROOT / 'technologies').glob('*.txt'))
    result['inputs'] = {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in inputs}
    (output / 'evidence.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
