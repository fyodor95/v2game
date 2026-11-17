"""Контракты новых предикатов по настоящим PE; DLL и игра не загружаются."""
import hashlib
from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]


class Image:
    def __init__(self, path):
        self.raw = path.read_bytes()
        pe = struct.unpack_from('<I', self.raw, 0x3c)[0]
        self.machine, count = struct.unpack_from('<HH', self.raw, pe + 4)
        optional = pe + 24
        self.base = struct.unpack_from('<I', self.raw, optional + 28)[0]
        sections = optional + struct.unpack_from('<H', self.raw, pe + 20)[0]
        self.sections = [struct.unpack_from('<4I', self.raw, sections + index * 40 + 8)
                         for index in range(count)]
        at, size = struct.unpack_from('<II', self.raw, optional + 96 + 5 * 8)
        finish = at + size
        self.relocations = []
        while at < finish:
            page, length = struct.unpack('<II', self.read(at, 8))
            if length < 8 or length & 1 or at + length > finish:
                raise ValueError('Повреждённый блок PE-relocations')
            for item in range(at + 8, at + length, 2):
                entry = struct.unpack('<H', self.read(item, 2))[0]
                if entry >> 12 == 3:
                    self.relocations.append(page + (entry & 0xfff))
            at += length

    def read(self, rva, size):
        for _, address, length, raw in self.sections:
            if address <= rva and rva + size <= address + length:
                offset = raw + rva - address
                return self.raw[offset:offset + size]
        raise ValueError(f'RVA вне секций: {rva:x}')

    def dword(self, rva):
        return struct.unpack('<I', self.read(rva, 4))[0]


class PredicateContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'tools/vic2test/native/bridge.cpp').read_text(encoding='utf-8')
        cls.engine = Image(ROOT / 'v2game.exe')

    def test_native_vtables_and_factory_routes_match_exact_engine(self):
        self.assertEqual(self.engine.machine, 0x14c)
        self.assertEqual(hashlib.sha256(self.engine.raw).hexdigest(),
                         '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648')
        entries = re.findall(r'\{ (0x[0-9A-F]+), (0x[0-9A-F]+), "([^"\n]+)", (true|false) \}', self.source)
        found = {}
        for table, evaluate, name, _ in entries:
            if name not in ('civilized', 'pop_majority_religion'):
                continue
            found[name] = int(table, 16)
            self.assertEqual(self.engine.dword(int(table, 16) + 0x18),
                             self.engine.base + int(evaluate, 16))
        self.assertEqual(found, {'civilized': 0xA31C40, 'pop_majority_religion': 0xA31F88})
        # Реальные переходы из фабрики токенов, а не только похожие vtable.
        for token, constructor in [(0x869, 0x4B74E0), (0x880, 0x4F1240)]:
            index = self.engine.read(0x4B6794 + token - 0x798, 1)[0]
            case = self.engine.dword(0x4B66F4 + index * 4) - self.engine.base
            call = case + 20
            self.assertEqual(self.engine.read(call, 1), b'\xe8')
            relative = struct.unpack('<i', self.engine.read(call + 1, 4))[0]
            self.assertEqual(call + 5 + relative, constructor)

    def test_core_full_bodies_relocations_and_vtable_install_match_built_dll(self):
        path = ROOT / '.work/creatinecore/build/creatinecore.dll'
        if not path.exists():
            self.skipTest('Для проверки тела требуется собранное creatinecore.dll')
        core = Image(path)
        self.assertEqual((core.machine, core.base), (0x14c, 0x10000000))
        for name, rva in [('countryReligionEvaluate', 0x33BC0), ('countryReligionLookup', 0x33AA0)]:
            body = re.search(r'static const BYTE ' + name + r'Bytes\[\] = \{([^}]+)\};', self.source)
            fixups = re.search(r'static const unsigned ' + name + r'Relocations\[\] = \{([^}]+)\};', self.source)
            expected = bytes(int(value, 16) for value in re.findall(r'0x([0-9A-F]+)', body.group(1)))
            offsets = [int(value) for value in re.findall(r'\d+', fixups.group(1))]
            self.assertEqual(core.read(rva, len(expected)), expected)
            self.assertEqual([at - rva for at in core.relocations if rva <= at < rva + len(expected)], offsets)
        entry = re.search(r'\{ (0x[0-9A-F]+), (0x[0-9A-F]+), "country_religion", false, true \}', self.source)
        table, evaluate = (int(value, 16) for value in entry.groups())
        self.assertEqual(core.read(0x34B5C, 10), b'\xc7\x05' + struct.pack('<II', core.base + table + 0x18,
                                                                                         core.base + evaluate))
        self.assertEqual(core.read(0x34B66, 10), b'\xc7\x05' + struct.pack('<II', core.base + table + 0x28,
                                                                                         core.base + 0x34270))
        call = evaluate + 0x44
        self.assertEqual(core.read(call, 1), b'\xe8')
        self.assertEqual(call + 5 + struct.unpack('<i', core.read(call + 1, 4))[0], 0x33AA0)


if __name__ == '__main__':
    unittest.main()
