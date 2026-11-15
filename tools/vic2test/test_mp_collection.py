"""Сбор журналов двух временных MP-сред без запуска игры и IPC."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import cli
from mp_driver import Driver


VERSION = b'GPC runtime: version=1 primary_only=1 all_ties=1 enabled=1 months=60 threshold=555\r\n'
INIT = b'GPC runtime: initialized day=100 seed=123\r\n'
PASS = b'GPC runtime: pass=1 day=150 additions=2 elapsed_ms=9 rng=456 cores=012345AB state=AABBCCDD\r\n'
SYNC = b'SYNC n=20 date=1891-01-20 00:00 sum=50245/50245 aux=0/0 first_real=-\r\n'


class CollectionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.host, self.client = (self.root / '.work' / role for role in ('host', 'client'))
        self.environments = {'host': self.host, 'client': self.client}
        self.pids = {'host': 4100, 'client': 4200}
        root_patch = patch.object(cli, 'ROOT', self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)
        ipc_patch = patch.object(cli, 'request', side_effect=AssertionError('Сбор логов не должен вызывать IPC'))
        ipc_patch.start()
        self.addCleanup(ipc_patch.stop)
        for role, environment in self.environments.items():
            (environment / 'game/z_CreatineCore/Logs').mkdir(parents=True)
            (environment / 'userdata').mkdir()
            (environment / 'environment.json').write_text(json.dumps({
                'game': str(environment / 'game'), 'userdata': str(environment / 'userdata'),
            }), encoding='utf-8')
            self.set_session(role, offset=0)
            self.log(role).write_bytes(self.current_log(role))
            self.log(role, 'v2dll_oos.log').write_bytes(SYNC)

    def log(self, role, filename='v2dll.log'):
        return self.environments[role] / 'game/z_CreatineCore/Logs' / filename

    def line(self, role, text):
        return f'[{self.pids[role]}] '.encode('ascii') + text

    def current_log(self, role):
        return b''.join(self.line(role, line) for line in (VERSION, INIT, PASS))

    def set_session(self, role, *, offset=None, missing=False):
        session = {'pid': self.pids[role], 'token': 'fixture-' + role}
        if not missing:
            session['log_offsets'] = {'v2dll.log': offset}
        (self.environments[role] / 'session.json').write_text(json.dumps(session), encoding='utf-8')

    def driver(self, name='report'):
        driver = Driver(self.host, self.client, self.root / (name + '.json'))
        # До сбора логов bootstrap/advance уже могли считаться завершёнными.
        driver.record.update(ok=True, complete=True, requested_target=12345)
        return driver

    def artifacts(self, driver):
        return self.root / (driver.output.stem + '-logs')

    def persisted(self, driver):
        return json.loads(driver.output.read_text(encoding='utf-8'))

    def test_byte_offset_excludes_old_error_even_when_pid_was_reused(self):
        current = {}
        for role in self.environments:
            previous = self.line(role, 'Предыдущий процесс\r\n'.encode('utf-8'))
            previous += self.line(role, b'GPC runtime: ERROR stale failure from reused PID\r\n')
            # Чужие PID после offset тоже не относятся к выбранной сессии.
            foreign = b'[9999] GPC runtime: ERROR other process\r\n'
            foreign += f'[{self.pids[role]}0] GPC runtime: ERROR similar PID\r\n'.encode('ascii')
            current[role] = self.current_log(role)
            self.log(role).write_bytes(previous + foreign + current[role])
            self.set_session(role, offset=len(previous))
        driver = self.driver()
        driver.collect_logs()
        proof = driver.record['log_proof']
        self.assertTrue(proof['ok'] and proof['complete'])
        self.assertEqual(proof['proof']['gpc']['pass_count'], 1)
        self.assertEqual(proof['errors'], [])
        for role in self.environments:
            selected = (self.artifacts(driver) / (role + '-v2dll.log')).read_text(encoding='utf-8')
            self.assertEqual(selected, current[role].decode('ascii').replace('\r\n', '\n'))
            self.assertNotIn('ERROR', selected)
        self.assertNotIn('log_scope_warnings', self.persisted(driver))

    def test_mixed_encoding_preserves_ascii_protocol_and_full_original_bytes(self):
        original = {}
        for role in self.environments:
            prefix = self.line(role, b'GPC runtime: ERROR old session\r\n')
            cp1251 = self.line(role, 'INFO: старое сообщение ядра\r\n'.encode('cp1251'))
            utf8 = self.line(role, 'INFO: новый русский текст\r\n'.encode('utf-8'))
            # CP1251 после ASCII-полей той же строки не должен повреждать digest.
            mixed_pass = PASS.rstrip(b'\r\n') + ' ; корки выданы\r\n'.encode('cp1251')
            selected = cp1251 + utf8 + self.line(role, VERSION) + self.line(role, INIT) + self.line(role, mixed_pass)
            original[role] = prefix + selected
            self.log(role).write_bytes(original[role])
            self.set_session(role, offset=len(prefix))
        driver = self.driver()
        driver.collect_logs()
        self.assertTrue(driver.record['log_proof']['complete'])
        for role in self.environments:
            files = self.artifacts(driver)
            self.assertEqual((files / (role + '-v2dll.full.log')).read_bytes(), original[role])
            self.assertEqual(self.log(role).read_bytes(), original[role])
            self.assertEqual((files / (role + '-v2dll_oos.log')).read_bytes(), SYNC)
            selected = (files / (role + '-v2dll.log')).read_text(encoding='utf-8')
            self.assertIn('\ufffd', selected)
            self.assertIn('новый русский текст', selected)
            self.assertIn('cores=012345AB state=AABBCCDD', selected)
            state = driver.record['log_proof']['proof']['gpc'][role]['passes'][0]
            self.assertEqual((state['rng'], state['cores'], state['state']), (456, '012345AB', 'AABBCCDD'))

    def test_invalid_offsets_raise_instead_of_selecting_arbitrary_bytes(self):
        for index, offset in enumerate((-1, 1.5, '0', [], {}, True, False)):
            with self.subTest(offset=offset):
                self.set_session('host', offset=offset)
                driver = self.driver('invalid-' + str(index))
                with self.assertRaisesRegex(RuntimeError, 'выделить журнал'):
                    driver.collect_logs()

    def test_truncation_after_recorded_offset_raises(self):
        self.set_session('host', offset=len(self.log('host').read_bytes()) + 100)
        truncated = b'[4100] short replacement\r\n'
        self.log('host').write_bytes(truncated)
        driver = self.driver()
        with self.assertRaisesRegex(RuntimeError, 'усечён'):
            driver.collect_logs()
        self.assertEqual((self.artifacts(driver) / 'host-v2dll.full.log').read_bytes(), truncated)

    def test_missing_offset_is_reported_as_explicit_pid_only_warning(self):
        for role in self.environments:
            self.set_session(role, missing=True)
        driver = self.driver()
        driver.collect_logs()
        warnings = self.persisted(driver)['log_scope_warnings']
        self.assertEqual(len(warnings), 2)
        for role in self.environments:
            self.assertTrue(any(message.startswith(role + ':') and 'PID' in message and 'byte-offset' in message
                                for message in warnings))
        self.assertTrue(driver.record['log_proof']['ok'])

    def test_current_session_error_is_not_hidden_by_byte_offset(self):
        previous = self.line('host', b'INFO: old session\r\n')
        self.log('host').write_bytes(previous + self.current_log('host') +
                                    self.line('host', b'GPC runtime: ERROR current failure\r\n'))
        self.set_session('host', offset=len(previous))
        driver = self.driver()
        with self.assertRaises(RuntimeError):
            driver.collect_logs()
        self.assertFalse(driver.record['ok'] or driver.record['complete'])
        self.assertTrue(any('GPC ERROR' in error for error in driver.record['log_proof']['errors']))

    def test_gpc_digest_mismatch_invalidates_previously_complete_report(self):
        for name, old, new in (('cores', b'cores=012345AB', b'cores=012345AC'),
                               ('state', b'state=AABBCCDD', b'state=AABBCCDE')):
            with self.subTest(digest=name):
                self.log('client').write_bytes(self.current_log('client').replace(old, new))
                driver = self.driver('mismatch-' + name)
                with self.assertRaisesRegex(RuntimeError, 'сетевых журналов'):
                    driver.collect_logs()
                persisted = self.persisted(driver)
                self.assertFalse(persisted['ok'] or persisted['complete'])
                self.assertFalse(persisted['log_proof']['ok'] or persisted['log_proof']['complete'])
                self.assertEqual(persisted['log_proof']['proof']['gpc']['status'], 'invalid')
                self.assertTrue(any('GPC:' in error for error in persisted['log_proof']['errors']))
                self.assertEqual(persisted['log_proof']['proof']['native_sync']['status'], 'complete')


if __name__ == '__main__':
    unittest.main()
