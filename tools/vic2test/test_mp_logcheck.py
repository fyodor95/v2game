"""Проверка парных журналов: совпадение, ошибки и недостаточность доказательств."""
from pathlib import Path
import tempfile
import unittest

from mp_logcheck import compare_logs


VERSION = 'GPC runtime: version=1 primary_only=1 all_ties=1 enabled=1 months=60 threshold=555\n'
INIT = 'GPC runtime: initialized day=100 seed=123\n'
PASS = 'GPC runtime: pass=1 day=150 additions=2 elapsed_ms=9 rng=456 cores=012345AB state=AABBCCDD\n'
SYNC = 'SYNC n=20 date=1891-01-20 00:00 sum=50245/50245 aux=0/0 rec=0 xor=00000000 first_real=-\n'


class LogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.paths = [self.root / name for name in ('host.log', 'client.log', 'host.oos', 'client.oos')]
        for path, content in zip(self.paths, (VERSION + INIT + PASS, VERSION + INIT + PASS, SYNC, SYNC)):
            path.write_text(content, encoding='utf-8')

    def replace(self, index, old, new):
        path = self.paths[index]
        path.write_text(path.read_text(encoding='utf-8').replace(old, new), encoding='utf-8')

    def check(self, **kwargs):
        return compare_logs(*self.paths, **kwargs)

    def test_matching_passes_and_native_checksums(self):
        result = self.check(require_native_sync=True, require_gpc_passes=1)
        self.assertTrue(result['ok'] and result['complete'])
        self.assertEqual(result['proof']['gpc']['pass_count'], 1)
        self.assertEqual(result['proof']['native_sync']['common_dates'], ['1891-01-20 00:00'])

    def test_elapsed_and_local_sync_counter_are_excluded(self):
        self.replace(1, 'elapsed_ms=9', 'elapsed_ms=99999')
        self.replace(3, 'SYNC n=20', 'SYNC n=99')
        self.assertTrue(self.check()['complete'])

    def test_changed_synchronous_state_fails(self):
        for old, new in (('rng=456', 'rng=457'), ('cores=012345AB', 'cores=012345AC'),
                         ('state=AABBCCDD', 'state=AABBCCDE'), ('additions=2', 'additions=3'),
                         ('day=150', 'day=151'), ('seed=123', 'seed=124')):
            with self.subTest(field=old):
                self.paths[1].write_text((VERSION + INIT + PASS).replace(old, new), encoding='utf-8')
                result = self.check()
                self.assertFalse(result['ok'])
                self.assertFalse(result['complete'])

    def test_gpc_error_fails_even_without_required_passes(self):
        with self.paths[0].open('a', encoding='utf-8') as output:
            output.write('GPC runtime: ERROR partial mutation\n')
        self.assertFalse(self.check()['ok'])

    def test_install_error_fails(self):
        with self.paths[0].open('a', encoding='utf-8') as output:
            output.write('GPC runtime: install ERROR signature\n')
        self.assertFalse(self.check()['ok'])

    def test_missing_peer_is_inconclusive_or_required_failure(self):
        self.paths[1].unlink()
        result = self.check()
        self.assertTrue(result['ok'])
        self.assertFalse(result['complete'])
        self.assertEqual(result['sources']['client_gpc']['status'], 'missing')
        required = self.check(require_gpc_passes=1)
        self.assertFalse(required['ok'])
        self.assertEqual(required['proof']['gpc']['status'], 'inconclusive')

    def test_empty_native_log_is_inconclusive(self):
        self.paths[3].write_text('', encoding='utf-8')
        result = self.check()
        self.assertTrue(result['ok'])
        self.assertFalse(result['complete'])
        self.assertFalse(self.check(require_native_sync=True)['ok'])
        self.assertEqual(self.check(require_native_sync=True)['proof']['native_sync']['status'], 'inconclusive')

    def test_partial_native_date_coverage_does_not_prove_sync(self):
        with self.paths[2].open('a', encoding='utf-8') as output:
            output.write(SYNC.replace('1891-01-20', '1891-01-21'))
        result = self.check(require_native_sync=True)
        self.assertFalse(result['ok'] or result['complete'])
        self.assertEqual(result['proof']['native_sync']['missing_client_dates'], ['1891-01-21 00:00'])

    def test_native_local_mismatch_fails(self):
        self.replace(2, 'sum=50245/50245', 'sum=50245/50246')
        result = self.check()
        self.assertFalse(result['ok'])
        self.assertEqual(result['proof']['native_sync']['status'], 'invalid')

    def test_native_peer_difference_fails_even_if_locally_equal(self):
        self.replace(3, 'sum=50245/50245', 'sum=7/7')
        self.assertFalse(self.check()['ok'])

    def test_first_real_and_native_oos_fails(self):
        for bad in ('first_real=1891-01-20', 'OOS hit=1 after 2 sync date=1891-01-20', 'FIRST real OOS'):
            with self.subTest(marker=bad):
                self.paths[3].write_text(SYNC + bad + '\n', encoding='utf-8')
                self.assertFalse(self.check()['ok'])

    def test_disabled_configuration_needs_no_passes(self):
        disabled = VERSION.replace('enabled=1', 'enabled=0')
        self.paths[0].write_text(disabled, encoding='utf-8')
        self.paths[1].write_text(disabled, encoding='utf-8')
        result = self.check(require_native_sync=True)
        self.assertTrue(result['ok'] and result['complete'])
        self.assertTrue(result['proof']['gpc']['disabled'])
        self.assertFalse(self.check(require_gpc_passes=1)['ok'])

    def test_unexpected_disabled_pass_is_an_error(self):
        for path in self.paths[:2]:
            path.write_text(VERSION.replace('enabled=1', 'enabled=0') + PASS, encoding='utf-8')
        self.assertFalse(self.check()['ok'])

    def test_duplicate_pass_is_not_counted_as_more_evidence(self):
        for path in self.paths[:2]:
            with path.open('a', encoding='utf-8') as output:
                output.write(PASS)
        self.assertFalse(self.check(require_gpc_passes=2)['ok'])

    def test_truncated_native_line_is_inconclusive(self):
        self.paths[3].write_text('SYNC n=20 date=1891-01-20 00:00 sum=', encoding='utf-8')
        result = self.check(require_native_sync=True)
        self.assertFalse(result['ok'] or result['complete'])
        self.assertEqual(result['proof']['native_sync']['client']['incomplete_lines'], [1])

    def test_no_passes_on_enabled_feature_is_inconclusive(self):
        for path in self.paths[:2]:
            path.write_text(VERSION + INIT, encoding='utf-8')
        self.assertFalse(self.check()['complete'])
        self.assertFalse(self.check(require_gpc_passes=1)['ok'])

    def test_missing_configuration_does_not_prove_gpc(self):
        for path in self.paths[:2]:
            path.write_text(INIT + PASS, encoding='utf-8')
        result = self.check()
        self.assertTrue(result['ok'])
        self.assertFalse(result['complete'])
        self.assertEqual(result['proof']['gpc']['status'], 'inconclusive')


if __name__ == '__main__':
    unittest.main()
