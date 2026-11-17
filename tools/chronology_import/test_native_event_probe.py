"""Разбор снимка памяти проверяется без процессов, WinAPI и запуска игры."""
import struct
import unittest

from tools.chronology_import import native_event_probe as subject


class Memory:
    def __init__(self):
        self.bytes = {}

    def put(self, address, raw):
        self.bytes.update((address + index, value) for index, value in enumerate(raw))

    def u32(self, address, value):
        self.put(address, struct.pack('<I', value))

    def vector(self, address, begin, values):
        self.put(address, struct.pack('<II', begin, begin + 4 * len(values)))
        self.put(begin, struct.pack('<' + 'I' * len(values), *values))

    def read(self, address, size):
        try:
            return bytes(self.bytes[address + index] for index in range(size))
        except KeyError as error:
            raise OSError(f'Недоступный адрес {error}') from error


class NativeEventProbeTests(unittest.TestCase):
    def setUp(self):
        self.memory = memory = Memory()
        self.base, self.world, self.province = 0x2500000, 0x10000, 0x40000
        self.event, self.other, self.node = 0x60000, 0x61000, 0x52000
        memory.u32(self.base + 0xE588E8, self.world)
        memory.u32(self.base + 0xE587E4, 0x20000)
        memory.u32(0x20000, 3)
        memory.u32(0x20004, 0x21000)
        memory.u32(self.world + 0xACC, 0x30000)
        memory.u32(self.world + 0xAD0, 0x30000 + 600 * 4)
        memory.u32(self.world + 0xB0C, 768)
        memory.put(self.world + 0xB20, b'\x01')
        memory.put(self.world + 0xD11, b'\x00')
        memory.u32(0x30000 + 546 * 4, self.province)
        memory.u32(self.province + 0x58, 546)
        memory.put(self.province + 0x128, b'PRU')
        memory.put(self.province + 0x130, b'REB')
        memory.vector(self.province + 0x14, 0x73000, [self.other, self.event])
        memory.u32(self.base + 0xE58A98, 0x50000)
        memory.u32(0x50004, 7)
        memory.u32(0x50008, 0x51000)
        bucket = ((0x27 << 16) + 24754) % 7
        memory.u32(0x51000 + bucket * 4, self.node)
        memory.u32(self.node, self.event)
        memory.u32(self.node + 4, 0)
        memory.u32(self.event + 8, 0x27)
        memory.u32(self.event + 12, 24754)
        memory.put(self.event + 0x30, bytes(32))
        memory.put(self.event + 0xC0, bytes(32))
        memory.u32(self.event + 0xC8, 300)
        memory.put(self.event + 0x1A0, b'\x00')
        memory.put(self.event + 0x1A3, b'\x01')
        memory.u32(self.other + 8, 0x27)
        memory.u32(self.other + 12, 70002)
        memory.u32(self.base + 0xF19B80, 0x70000)
        memory.vector(0x70020, 0x71000, [self.other, self.event])
        memory.vector(0x70030, 0x72000, [self.event])

    def inspect(self):
        return subject.inspect_native(self.memory.read, self.base, 546, 24754)

    def executable_image(self):
        image = bytearray(0x100 + 56)
        image[:2] = b'MZ'
        struct.pack_into('<I', image, 0x3C, 0x100)
        image[0x100:0x104] = b'PE\0\0'
        struct.pack_into('<H', image, 0x100 + 4, 0x14C)
        struct.pack_into('<H', image, 0x100 + 24, 0x10B)
        struct.pack_into('<I', image, 0x100 + 52, 0x400000)
        self.memory.put(self.base, image)
        return bytes(image)

    def test_pe_identity_accepts_relocated_x86_and_preserves_preferred_base(self):
        image = self.executable_image()
        result = subject.pe_identity(self.memory.read, self.base, image)
        self.assertEqual(result['actual_base'], 0x2500000)
        self.assertEqual(result['preferred_base'], 0x400000)
        self.assertEqual(result['loaded_header_base'], 0x400000)
        self.assertTrue(result['relocated'])
        self.memory.put(self.base + 0x100 + 4, struct.pack('<H', 0x8664))
        with self.assertRaisesRegex(ValueError, 'PE32/x86'):
            subject.pe_identity(self.memory.read, self.base, image)

    def test_loaded_header_rewritten_to_actual_base_does_not_hide_relocation(self):
        image = self.executable_image()
        self.memory.u32(self.base + 0x100 + 52, self.base)
        result = subject.pe_identity(self.memory.read, self.base, image)
        self.assertEqual(result['actual_base'], self.base)
        self.assertEqual(result['loaded_header_base'], self.base)
        self.assertEqual(result['preferred_base'], 0x400000)
        self.assertEqual(result['preferred_base_source'], 'qualified_executable_file')
        self.assertEqual(result['file_pe_offset'], 0x100)
        self.assertTrue(result['relocated'])

    def test_invalid_local_pe_is_not_replaced_by_valid_loaded_header(self):
        image = self.executable_image()
        with self.assertRaisesRegex(ValueError, 'PE32/x86'):
            subject.pe_identity(self.memory.read, self.base, image[:-1])
        with self.assertRaisesRegex(ValueError, 'MZ'):
            subject.pe_identity(self.memory.read, self.base, image[:32])
        invalid = bytearray(image)
        struct.pack_into('<H', invalid, 0x100 + 24, 0x20B)
        with self.assertRaisesRegex(ValueError, 'PE32/x86'):
            subject.pe_identity(self.memory.read, self.base, invalid)

    def test_reads_actual_event_lookup_flags_mtth_and_all_three_memberships(self):
        report = self.inspect()
        self.assertTrue(report['complete'], report['errors'])
        self.assertEqual(report['world']['world_b20_byte'], 1)
        self.assertEqual(report['world']['world_d11_byte'], 0)
        self.assertEqual(report['event']['mtth_base_fixed_i32'], 300)
        self.assertEqual(report['event']['event_1a0_byte'], 0)
        self.assertEqual(report['event']['event_1a3_byte'], 1)
        self.assertEqual(report['registry']['all_province_events']['target_indices'], [1])
        self.assertEqual(report['registry']['random_province_events']['target_indices'], [0])
        self.assertEqual(report['province']['target_indices'], [1])
        self.assertEqual([entry['event_id'] for entry in report['province']['candidates']], [70002, 24754])

    def test_empty_cache_is_observed_without_inventing_event_membership(self):
        self.memory.vector(self.province + 0x14, 0, [])
        report = self.inspect()
        self.assertTrue(report['complete'])
        self.assertTrue(report['event']['found'])
        self.assertEqual(report['province']['candidates'], [])
        self.assertEqual(report['province']['target_indices'], [])

    def test_corrupt_vector_preserves_independent_sections_and_reports_failure(self):
        self.memory.u32(self.province + 0x18, 0x73003)
        report = self.inspect()
        self.assertFalse(report['complete'])
        self.assertEqual(report['errors'][0]['section'], 'province')
        self.assertEqual(report['event']['mtth_base_fixed_i32'], 300)

    def test_hash_cycle_is_bounded_and_not_reported_as_absent_event(self):
        self.memory.u32(self.node, self.other)
        self.memory.u32(self.node + 4, self.node)
        report = self.inspect()
        self.assertFalse(report['complete'])
        self.assertTrue(any(error['section'] == 'event' and 'Цикл' in error['message'] for error in report['errors']))
        self.assertNotIn('event', report)


if __name__ == '__main__':
    unittest.main()
