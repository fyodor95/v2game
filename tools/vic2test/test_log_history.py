"""Архивация только тестовых журналов перед запуском нового экземпляра."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import cli


class LogHistoryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.environment = self.root / '.work' / 'стенд лёд'
        self.game = self.environment / 'game'
        self.logs = self.game / 'z_CreatineCore' / 'Logs'
        self.logs.mkdir(parents=True)
        self.contents = {
            'v2dll.log': 'Предыдущая сессия\r\n'.encode() + 'Старое ядро\n'.encode('cp1251') + b'\x00\xff\r\n',
            'v2dll_oos.log': b'SYNC sum=321/321\r\n' + 'Диагностика\n'.encode(),
        }
        for name, raw in self.contents.items():
            (self.logs / name).write_bytes(raw)
        root_patch = patch.object(cli, 'ROOT', self.root)
        root_patch.start()
        self.addCleanup(root_patch.stop)

    def assert_sources_unchanged(self):
        for name, raw in self.contents.items():
            self.assertEqual((self.logs / name).read_bytes(), raw)

    def test_both_logs_are_moved_byte_exactly_without_creating_new_logs(self):
        unrelated = self.logs / 'v2dll_oos.previous.log'
        unrelated.write_bytes(b'keep unrelated diagnostic')
        original = self.root / 'original-user-log.log'
        original.write_bytes(b'original outside sandbox')
        saved = cli.archive_logs(self.environment, self.game, 'session-001')
        self.assertEqual(set(saved), set(self.contents))
        for name, raw in self.contents.items():
            expected = self.environment / 'log-history' / 'session-001' / name
            self.assertEqual(Path(saved[name]), expected)
            self.assertEqual(expected.read_bytes(), raw)
            self.assertFalse((self.logs / name).exists())
        self.assertEqual(unrelated.read_bytes(), b'keep unrelated diagnostic')
        self.assertEqual(original.read_bytes(), b'original outside sandbox')

    def test_existing_archive_is_never_overwritten(self):
        history = self.environment / 'log-history' / 'same-session'
        history.mkdir(parents=True)
        existing = history / 'v2dll.log'
        existing.write_bytes(b'previous archived bytes')
        with self.assertRaises(FileExistsError):
            cli.archive_logs(self.environment, self.game, 'same-session')
        self.assertEqual(existing.read_bytes(), b'previous archived bytes')
        self.assert_sources_unchanged()

    def test_new_session_keeps_previous_archive_and_collects_only_new_logs(self):
        first = cli.archive_logs(self.environment, self.game, 'first')
        fresh = {'v2dll.log': b'new process main log', 'v2dll_oos.log': b'new process OOS log'}
        for name, raw in fresh.items():
            (self.logs / name).write_bytes(raw)
        second = cli.archive_logs(self.environment, self.game, 'second')
        for name in self.contents:
            self.assertEqual(Path(first[name]).read_bytes(), self.contents[name])
            self.assertEqual(Path(second[name]).read_bytes(), fresh[name])
            self.assertFalse((self.logs / name).exists())

    def test_absent_logs_are_noop_and_one_log_is_allowed(self):
        (self.logs / 'v2dll_oos.log').unlink()
        saved = cli.archive_logs(self.environment, self.game, 'one')
        self.assertEqual(set(saved), {'v2dll.log'})
        self.assertEqual(cli.archive_logs(self.environment, self.game, 'empty'), {})
        self.assertFalse((self.environment / 'log-history' / 'empty').exists())

    def test_environment_outside_work_is_rejected_without_moving_data(self):
        outside = self.root / 'outside'
        (outside / 'game').mkdir(parents=True)
        with self.assertRaises(ValueError):
            cli.archive_logs(outside, outside / 'game', 'outside')
        self.assertFalse((outside / 'log-history').exists())
        self.assert_sources_unchanged()

    def test_other_environments_game_and_history_escape_are_rejected(self):
        other_game = self.root / '.work' / 'other' / 'game'
        other_game.mkdir(parents=True)
        with self.assertRaises(ValueError):
            cli.archive_logs(self.environment, other_game, 'wrong-game')
        for token in ('../../outside', str(self.root / 'outside-history')):
            with self.subTest(token=token), self.assertRaises(ValueError):
                cli.archive_logs(self.environment, self.game, token)
        self.assert_sources_unchanged()
        self.assertFalse((self.root / 'outside-history').exists())

    @unittest.skipUnless(os.name == 'nt', 'Проверка Windows junction')
    def test_log_directory_reparse_outside_game_is_rejected(self):
        environment = self.root / '.work' / 'linked-source'
        game = environment / 'game'
        (game / 'z_CreatineCore').mkdir(parents=True)
        external = self.root / 'original-logs'
        external.mkdir()
        for name, raw in self.contents.items():
            (external / name).write_bytes(raw)
        junction = game / 'z_CreatineCore' / 'Logs'
        subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(junction), str(external)],
                       check=True, capture_output=True)
        try:
            with self.assertRaises(ValueError):
                cli.archive_logs(environment, game, 'bad-source')
            for name, raw in self.contents.items():
                self.assertEqual((external / name).read_bytes(), raw)
            self.assertFalse((environment / 'log-history').exists())
        finally:
            # Удаляется только junction, без рекурсивного обхода её цели.
            os.rmdir(junction)

    @unittest.skipUnless(os.name == 'nt', 'Проверка Windows junction')
    def test_empty_external_log_directory_is_rejected_before_future_logger_can_write(self):
        environment = self.root / '.work' / 'empty-linked-source'
        game = environment / 'game'
        (game / 'z_CreatineCore').mkdir(parents=True)
        external = self.root / 'empty-original-logs'
        external.mkdir()
        junction = game / 'z_CreatineCore' / 'Logs'
        subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(junction), str(external)],
                       check=True, capture_output=True)
        try:
            # Проверка границы нужна и без старого файла: иначе helper разрешит
            # последующему logger создать новый файл за пределами game/.
            with self.assertRaises(ValueError):
                cli.archive_logs(environment, game, 'empty-source')
            self.assertEqual(list(external.iterdir()), [])
            self.assertFalse((environment / 'log-history').exists())
        finally:
            os.rmdir(junction)

    @unittest.skipUnless(os.name == 'nt', 'Проверка Windows junction')
    def test_history_reparse_outside_environment_is_rejected(self):
        external = self.root / 'external-history'
        external.mkdir()
        sentinel = external / 'keep.bin'
        sentinel.write_bytes(b'keep external history')
        junction = self.environment / 'log-history'
        subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(junction), str(external)],
                       check=True, capture_output=True)
        try:
            with self.assertRaises(ValueError):
                cli.archive_logs(self.environment, self.game, 'bad-destination')
            self.assert_sources_unchanged()
            self.assertEqual(list(external.iterdir()), [sentinel])
            self.assertEqual(sentinel.read_bytes(), b'keep external history')
        finally:
            os.rmdir(junction)

    def test_start_archives_before_fake_popen_and_records_zero_offset(self):
        userdata = self.environment / 'userdata'
        userdata.mkdir()
        (self.environment / 'environment.json').write_text(json.dumps({
            'game': str(self.game), 'userdata': str(userdata),
        }), encoding='utf-8')
        executable = self.game / 'v2game.exe'
        executable.write_bytes(b'fake executable; never launched')
        observed = {}

        def fake_popen(*args, **kwargs):
            self.assertEqual(kwargs['cwd'], self.game)
            token = kwargs['env']['VIC2_TEST_SESSION']
            for name, raw in self.contents.items():
                self.assertFalse((self.logs / name).exists())
                archive = self.environment / 'log-history' / token / name
                self.assertEqual(archive.read_bytes(), raw)
            # Имитируется logger, создающий файл непосредственно при старте.
            (self.logs / 'v2dll.log').write_bytes(b'fresh process log')
            observed['token'] = token
            return SimpleNamespace(pid=12345)

        with patch.object(cli, 'EXE_HASH', hashlib.sha256(executable.read_bytes()).hexdigest()), \
                patch.object(cli.subprocess, 'Popen', side_effect=fake_popen) as launch:
            result = cli.start(self.environment, hidden=False)
        launch.assert_called_once()
        session = json.loads((self.environment / 'session.json').read_text(encoding='utf-8'))
        self.assertTrue(result['ok'])
        self.assertEqual(session['token'], observed['token'])
        self.assertEqual(session['log_offsets']['v2dll.log'], 0)
        self.assertEqual(set(session['log_history']), set(self.contents))
        self.assertEqual((self.logs / 'v2dll.log').read_bytes(), b'fresh process log')
        self.assertFalse((self.logs / 'v2dll_oos.log').exists())


if __name__ == '__main__':
    unittest.main()
