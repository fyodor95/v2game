"""Проверки расписания fixture, формата флагов и сохранности исходных байтов."""
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from make_fixture import Spans
from make_schedule_fixture import (EPOCH, checksum, create_fixture, day_from_date, encode,
                                   inverse_xorshift32, transform, validate_existing, xorshift32)


class ScheduleFixtureTests(unittest.TestCase):
    SOURCE = ('# Сохранить ё и CRLF\r\ndate="1891.1.1"\r\nflags={\r\n'
              '\tuser_flag=yes # чужой флаг\r\n\tuser_disabled=no\r\n}\r\n'
              '1={owner="ENG" flags={cc_gpc_v2_ready=yes} farmers={size=123 south_german=catholic}}\r\n'
              'ENG={primary_culture="south_german" flags={user_flag=yes}}\r\n}\r\n').encode('cp1251')

    def flags(self, data):
        node = next(node for node in Spans(data.decode('cp1251')).roots() if node.key == 'flags')
        return {entry.key: entry.value for entry in node.value}

    def test_actual_date_and_raw_boundary(self):
        self.assertEqual(day_from_date('1891.1.1'), 690215)
        self.assertEqual(EPOCH + day_from_date('1891.1.1') * 24, 60365160)
        self.assertEqual(EPOCH + 690219 * 24, 60365256)
        self.assertEqual(day_from_date('1891.3.1') - day_from_date('1891.2.28'), 1)
        for invalid in ('1892.2.29', '1891.13.1', '1891.1.0', 'bad'):
            with self.subTest(date=invalid), self.assertRaises(ValueError):
                day_from_date(invalid)

    def test_inverse_matches_independent_known_vectors(self):
        for before, after in ((1, 270369), (0x12345678, 2274908837), (0xffffffff, 253983), (0xf2b58529, 1)):
            self.assertEqual(xorshift32(before), after)
            self.assertEqual(inverse_xorshift32(after), before)

    def test_fnv_known_cpp_vector_and_independent_byte_encoding(self):
        state = {'schema': 1, 'seed': 1, 'rng': 1, 'last_day': 100, 'pass_sequence': 0}
        self.assertEqual(checksum(state), 0xf77e80f0)  # scheduler_test.cpp::Codec
        value = 2166136261
        for byte in struct.pack('<IIIII', 1, 1, 1, 100, 0):
            value = ((value ^ byte) * 16777619) % (2 ** 32)
        self.assertEqual(checksum(state), value)
        flags = encode(state)
        self.assertEqual(len(flags), 193)
        self.assertIn('cc_gpc_v1_schema_b00_1', flags)
        self.assertIn('cc_gpc_v1_schema_b31_0', flags)
        self.assertIn('cc_gpc_v1_checksum_b00_0', flags)
        self.assertIn('cc_gpc_v1_checksum_b04_1', flags)
        self.assertIn('cc_gpc_v1_checksum_b31_1', flags)
        self.assertNotIn('cc_gpc_v1_writing', flags)
        validate_existing(flags)

    def test_exact_bytes_and_first_production_draw(self):
        result, manifest = transform(self.SOURCE)
        self.assertEqual(manifest['active_flags'], 193)
        self.assertEqual(manifest['next_attempt_day'], 690217)
        self.assertEqual(manifest['next_raw_draw'], 1)
        self.assertEqual(manifest['next_draw'], 0)
        self.assertEqual(manifest['threshold'], 555)
        self.assertFalse(manifest['test_always'])
        insertion = manifest['replacements'][0]
        self.assertEqual(result, self.SOURCE[:insertion['start']] + insertion['after'].encode('cp1251') + self.SOURCE[insertion['end']:])
        self.assertIn(b'user_disabled=no', result)
        # Вложенный флаг провинции должен сохраниться вне корневого namespace.
        self.assertIn(b'flags={cc_gpc_v2_ready=yes} farmers={size=123', result)
        flags = {key: value for key, value in self.flags(result).items() if key.startswith('cc_gpc_')}
        validate_existing(flags)

    def test_existing_valid_state_replaced_without_growth(self):
        once, _ = transform(self.SOURCE)
        twice, manifest = transform(once)
        flags = self.flags(twice)
        self.assertEqual(sum(name.startswith('cc_gpc_') for name in flags), 193)
        self.assertEqual(manifest['removed_existing_flags'], 193)
        self.assertEqual(flags['user_flag'], 'yes')
        self.assertEqual(flags['user_disabled'], 'no')

    def test_faults_and_explicit_test_state_replacement(self):
        for fault, count in (('unknown-version', 194), ('partial', 192)):
            with self.subTest(fault=fault):
                result, manifest = transform(self.SOURCE, fault=fault)
                self.assertEqual(manifest['fault'], fault)
                self.assertEqual(manifest['active_flags'], count)
                bad = {key: value for key, value in self.flags(result).items() if key.startswith('cc_gpc_')}
                with self.assertRaises(ValueError):
                    validate_existing(bad)
                with self.assertRaises(ValueError):
                    transform(result)
                fixed, report = transform(result, replace_existing_test_state=True)
                self.assertEqual(report['active_flags'], 193)
                validate_existing({key: value for key, value in self.flags(fixed).items() if key.startswith('cc_gpc_')})

    def test_malformed_or_duplicate_root_flags_rejected(self):
        for text in ('date="1891.1.1" flags={} flags={}',
                     'date="1891.1.1" flags={a=yes a=no}',
                     'date="1891.1.1" flags={a={yes}}',
                     'date="1891.1.1" flags={a=maybe}',
                     'date="1891.1.1" flags={cc_gpc_user_flag=yes}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                transform(text.encode('cp1251'))

    def test_exclusive_output_and_source_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'original.v2'
            source.write_bytes(self.SOURCE)
            work = root / '.work'
            output = work / 'test/userdata/save games/production.v2'
            output.parent.mkdir(parents=True)
            result = create_fixture(source, output, work_root=work)
            self.assertEqual(source.read_bytes(), self.SOURCE)
            self.assertEqual(result['source_sha256'], hashlib.sha256(self.SOURCE).hexdigest())
            self.assertEqual(result['output_sha256'], hashlib.sha256(output.read_bytes()).hexdigest())
            self.assertTrue(output.with_suffix('.v2.json').is_file())
            with self.assertRaises(FileExistsError):
                create_fixture(source, output, work_root=work)
            with self.assertRaises(ValueError):
                create_fixture(source, root / 'outside.v2', work_root=work)


if __name__ == '__main__':
    unittest.main()
