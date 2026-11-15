"""Проверки точечных замен fixture без игровых процессов."""
from pathlib import Path
import hashlib
import tempfile
import unittest

from make_fixture import Spans, create_fixture, partition, transform, validate_output
from save_oracle import read_entries


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.work = self.root / '.work'
        self.savedir = self.work / 'test/userdata/save games'
        self.savedir.mkdir(parents=True)
        for folder in ('common', 'poptypes'):
            (self.root / folder).mkdir()
        (self.root / 'common/cultures.txt').write_text('g={south_german={} north_german={} other={}}')
        (self.root / 'common/religion.txt').write_text('g={catholic={} protestant={}}')
        for poptype in ('farmers', 'labourers'):
            (self.root / f'poptypes/{poptype}.txt').write_text('color={1 2 3}')

    def source_bytes(self, sizes=None):
        if sizes is None:
            sizes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 11]
        pops = []
        for index, size in enumerate(sizes):
            quoted = '"other"' if index % 2 else 'other'
            poptype = 'farmers' if index % 2 else 'labourers'
            pops.append(f'\t{poptype}={{id={index+1} size={size} {quoted}=catholic '
                        'money=123.450 employment={ {province_pop_id={province_id=9 index=1} count=1} } '
                        'ideology={1=0.5 2=0.5}}')
        return ('# Проверка ё, сохранить точные байты\r\n'
                'date="1891.1.1"\r\n'
                'AUS={primary_culture="south_german" culture={other}}\r\n'
                'NGF={primary_culture="north_german" culture={other}}\r\n'
                'CHI={primary_culture="other"}\r\n'
                '1={owner="AUS" farmers={size=50 other=catholic} farmers={size=50 other=catholic}}\r\n'
                '2={owner="CHI" core="NGF" farmers={size=50 other=catholic} farmers={size=50 other=catholic}}\r\n'
                '9={name="Точный тест" owner="CHI" controller="AUS" core="CHI" core="CHI"\r\n'
                + '\r\n'.join(pops) + '\r\n}\r\n}\r\n').encode('cp1251')

    def test_partition(self):
        sizes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 11]
        selected = partition(sizes)
        self.assertIsNotNone(selected)
        self.assertEqual(sum(sizes[n] for n in selected), sum(sizes) // 2)
        for impossible in ([1, 1, 4], [1, 2], [0, 0], [100001, 100001], [2] * 41):
            self.assertIsNone(partition(impossible))

    def test_exact_bytes_tie(self):
        original = self.source_bytes()
        result, manifest = transform(original, self.root, 'tie')
        self.assertEqual(manifest['province'], 9)
        self.assertEqual(manifest['old_owner'], 'CHI')
        self.assertEqual(manifest['population'], {'south_german': 28, 'north_german': 28})
        self.assertEqual(manifest['pop_count'], 10)
        self.assertEqual([p['country'] for p in manifest['expected_direct_pairs']], ['AUS', 'NGF'])
        expected = original
        for replacement in reversed(manifest['replacements']):
            start, end = replacement['start'], replacement['end']
            self.assertEqual(original[start:end].decode('cp1251'), replacement['before'])
            expected = expected[:start] + replacement['after'].encode('cp1251') + expected[end:]
            self.assertEqual(replacement['kind'], 'POP_culture_key')
        self.assertEqual(result, expected)
        # Обратная замена по смещённым span доказывает сохранность всех прочих байтов.
        shift = 0
        restored = bytearray(result)
        adjusted = []
        for item in manifest['replacements']:
            adjusted.append((item['start'] + shift, len(item['after'].encode('cp1251')), item['before'].encode('cp1251')))
            shift += len(item['after'].encode('cp1251')) - (item['end'] - item['start'])
        for start, length, before in reversed(adjusted):
            restored[start:start+length] = before
        self.assertEqual(bytes(restored), original)

    def test_shared_changes_only_primary_and_culture(self):
        result, manifest = transform(self.source_bytes(), self.root, 'shared')
        self.assertEqual(manifest['population'], {'south_german': 56, 'north_german': 0})
        self.assertEqual(sum(item['kind'] == 'NGF_primary_culture' for item in manifest['replacements']), 1)
        self.assertIn(b'NGF={primary_culture="south_german" culture={other}}', result)
        self.assertIn(b'controller="AUS" core="CHI" core="CHI"', result)

    def test_create_preserves_source_and_no_overwrite(self):
        source = self.root / 'source.v2'
        source.write_bytes(self.source_bytes())
        original = hashlib.sha256(source.read_bytes()).hexdigest()
        output = self.savedir / 'tie.v2'
        manifest = create_fixture(source, self.root, output, 'tie', work_root=self.work)
        self.assertEqual(original, hashlib.sha256(source.read_bytes()).hexdigest())
        self.assertEqual(manifest['output_sha256'], hashlib.sha256(output.read_bytes()).hexdigest())
        self.assertTrue(output.with_suffix('.v2.json').is_file())
        with self.assertRaises(FileExistsError):
            create_fixture(source, self.root, output, 'shared', work_root=self.work)

    def test_invalid_output(self):
        for path in (self.root / 'outside.v2', self.work / 'bad.v2', self.savedir / 'bad.txt'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                validate_output(path, self.work)

    def test_impossible_without_population_edits(self):
        with self.assertRaisesRegex(ValueError, 'точным равным'):
            transform(self.source_bytes([1, 1, 4]), self.root, 'tie')

    def test_span_parser_keeps_repeated_blocks(self):
        roots = list(Spans('1={farmers={size=2 other=catholic} farmers={size=3 other=catholic}} }').roots())
        self.assertEqual(len(roots[0].fields('farmers')), 2)


if __name__ == '__main__':
    unittest.main()
