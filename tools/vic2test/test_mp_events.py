"""Проверка контрактов адаптера по реальному PE; игру и DLL не запускает.

Поведение валидатора проверяется отдельным native/mp_events_test.cpp (MSVC x86).
"""
import hashlib
from pathlib import Path
import re
import struct
import unittest


ROOT = Path(__file__).resolve().parents[2]


class NativeImageContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = (ROOT / 'v2game.exe').read_bytes()
        pe = struct.unpack_from('<I', cls.image, 0x3C)[0]
        cls.machine, count = struct.unpack_from('<HH', cls.image, pe + 4)
        optional_size = struct.unpack_from('<H', cls.image, pe + 20)[0]
        cls.base = struct.unpack_from('<I', cls.image, pe + 24 + 28)[0]
        sections = pe + 24 + optional_size
        cls.sections = [struct.unpack_from('<4I', cls.image, sections + index * 40 + 8)
                        for index in range(count)]

    def read(self, rva, size):
        for _, address, raw_size, raw in self.sections:
            if address <= rva and rva + size <= address + raw_size:
                offset = raw + rva - address
                return self.image[offset:offset + size]
        self.fail(f'RVA вне секций: {rva:x}')

    def test_qualified_image(self):
        self.assertEqual(self.machine, 0x14C)
        self.assertEqual(hashlib.sha256(self.image).hexdigest(),
                         '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648')

    def test_adapter_signatures_match_engine(self):
        source = (ROOT / 'tools/vic2test/native/mp_events.h').read_text(encoding='utf-8')
        arrays = {
            name: bytes(int(value, 0) for value in re.findall(r'0[xX][0-9A-Fa-f]+|\b\d+\b', values))
            for name, values in re.findall(
                r'static const unsigned char (\w+)\[\] = \{([^}]+)\};', source)
        }
        sites = re.findall(r'Signature\(base, (0x[0-9A-Fa-f]+), (\w+)\);', source)
        self.assertEqual(len(sites), len(arrays))
        self.assertGreaterEqual(len(sites), 16)
        for address, name in sites:
            with self.subTest(name=name, address=address):
                expected = arrays[name]
                self.assertEqual(self.read(int(address, 16), len(expected)), expected)
        self.assertEqual(struct.unpack('<I', self.read(0xA333BC + 0x18, 4))[0],
                         self.base + 0x507E00)


if __name__ == '__main__':
    unittest.main()
