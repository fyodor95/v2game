"""Точная проверка добавлений и сохранности старых корок."""
from copy import deepcopy
import json
from pathlib import Path
import tempfile
import unittest

from check_delta import check_delta, compare_delta
from save_oracle import preview


class DeltaTests(unittest.TestCase):
    def setUp(self):
        self.before = {'ok': True, 'complete': True, 'great_powers': ['AUS', 'NGF'],
                       'great_powers_source': 'explicit_qualified_tags', 'save_sha256': 'a' * 64,
                       'provinces': [{'id': 1, 'owner': 'ENG', 'cores': ['ENG']},
                                     {'id': 2, 'owner': 'CHI', 'cores': ['CHI']}],
                       'additions': [{'province': 1, 'country': 'AUS'}, {'province': 1, 'country': 'NGF'}]}
        self.after = deepcopy(self.before)
        self.after['save_sha256'] = 'b' * 64
        self.after['provinces'][0]['cores'] += ['NGF', 'AUS']
        self.after['additions'] = []

    def test_exact_shared_or_tied_recipients(self):
        result = compare_delta(self.before, self.after, require_no_remaining=True)
        self.assertTrue(result['ok'])
        self.assertEqual(result['additions'], self.before['additions'])
        self.assertEqual(result['removed'], [])
        self.assertEqual(result['before_sha256'], 'a' * 64)
        self.assertEqual(result['after_sha256'], 'b' * 64)

    def test_missing_core(self):
        self.after['provinces'][0]['cores'].remove('NGF')
        result = compare_delta(self.before, self.after)
        self.assertFalse(result['ok'])
        self.assertEqual(result['missing'], [{'province': 1, 'country': 'NGF'}])

    def test_unrelated_province_and_non_gp_core_are_not_ignored(self):
        self.after['provinces'][1]['cores'].append('FRA')
        result = compare_delta(self.before, self.after)
        self.assertFalse(result['ok'])
        self.assertEqual(result['unexpected'], [{'province': 2, 'country': 'FRA'}])

    def test_deleted_old_core(self):
        self.after['provinces'][1]['cores'] = []
        result = compare_delta(self.before, self.after)
        self.assertFalse(result['ok'])
        self.assertEqual(result['removed'], [{'province': 2, 'country': 'CHI'}])

    def test_remaining_candidates_are_explicitly_optional(self):
        self.after['additions'] = [{'province': 2, 'country': 'NGF'}]
        self.assertTrue(compare_delta(self.before, self.after)['ok'])
        result = compare_delta(self.before, self.after, require_no_remaining=True)
        self.assertFalse(result['ok'])
        self.assertEqual(result['remaining_candidates'], [{'province': 2, 'country': 'NGF'}])

    def test_duplicate_province_or_core_is_rejected(self):
        for target in ('before', 'after'):
            for mode in ('province', 'core'):
                before, after = deepcopy(self.before), deepcopy(self.after)
                state = before if target == 'before' else after
                if mode == 'province':
                    state['provinces'].append(deepcopy(state['provinces'][0]))
                else:
                    state['provinces'][0]['cores'].append('ENG')
                with self.subTest(target=target, mode=mode), self.assertRaises(ValueError):
                    compare_delta(before, after)

    def test_duplicate_expected_pairs_and_invalid_tags(self):
        self.before['additions'].append(self.before['additions'][0])
        with self.assertRaises(ValueError):
            compare_delta(self.before, self.after)
        self.before['additions'].pop()
        for tag in ('aus', 'AU', 'AUS ', '---', 123):
            bad = deepcopy(self.after)
            bad['provinces'][0]['cores'].append(tag)
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                compare_delta(self.before, bad)

    def test_disappeared_empty_province_fails_even_without_core_delta(self):
        self.before['provinces'].append({'id': 3, 'owner': None, 'cores': []})
        result = compare_delta(self.before, self.after)
        self.assertFalse(result['ok'])
        self.assertEqual(result['missing_provinces'], [3])

    def test_gp_sets_must_match(self):
        self.after['great_powers'] = ['AUS']
        with self.assertRaises(ValueError):
            compare_delta(self.before, self.after)

    def real_fixture(self, root, shared=False):
        for folder in ('common', 'poptypes', 'technologies'):
            (root / folder).mkdir()
        (root / 'common/cultures.txt').write_text('g={south_german={} north_german={}}')
        (root / 'common/religion.txt').write_text('g={catholic={}}')
        (root / 'poptypes/farmers.txt').write_text('color={1 2 3}')
        (root / 'technologies/culture.txt').write_text('nationalism_n_imperialism={year=1850}')
        north = 'south_german' if shared else 'north_german'
        pop = 'farmers={size=50 south_german=catholic} farmers={size=50 %s=catholic}' % north
        source = ('AUS={primary_culture="south_german" technology={nationalism_n_imperialism={1 0}}}\n'
                  'NGF={primary_culture="%s" technology={nationalism_n_imperialism={1 0}}}\n' % north +
                  'ENG={primary_culture="south_german"}\n'
                  '1={owner="ENG" core="ENG" %s}\n' % pop +
                  '2={owner="AUS"} 3={owner="NGF"}\n')
        saved = root / 'before.v2'
        saved.write_bytes(source.encode('cp1251'))
        oracle = preview(saved, root, ['AUS', 'NGF'])
        before = root / 'oracle.json'
        before.write_text(json.dumps(oracle), encoding='utf-8')
        after = root / 'after.v2'
        after.write_bytes(source.replace('core="ENG"', 'core="ENG" core="AUS" core="NGF"').encode('cp1251'))
        return before, after

    def test_real_parser_shared_and_tie(self):
        for shared in (False, True):
            with self.subTest(shared=shared), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                before, after = self.real_fixture(root, shared)
                result = check_delta(before, after, root, ['AUS', 'NGF'], require_no_remaining=True)
                self.assertTrue(result['ok'])
                self.assertEqual(result['additions'], self.before['additions'])

    def test_raw_duplicate_core_rejected_before_oracle_normalization(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            before, after = self.real_fixture(root)
            after.write_bytes(after.read_bytes().replace(b'core="ENG"', b'core="ENG" core="ENG"'))
            with self.assertRaisesRegex(ValueError, 'Повторная корка'):
                check_delta(before, after, root, ['AUS', 'NGF'])


if __name__ == '__main__':
    unittest.main()
