"""Публикация session и cleanup собственного Popen; игры не запускаются."""
import hashlib
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import cli


class SessionPublicationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='vic2-session-test-')
        self.addCleanup(temporary.cleanup)
        self.environment = Path(temporary.name)
        self.session = self.environment / 'session.json'
        self.previous = b'{"pid": 17, "token": "previous"}\r\n'
        self.session.write_bytes(self.previous)
        self.state = {'pid': 23, 'token': 'a' * 32, 'game': 'Тестовая игра'}

    def test_success_replaces_old_session_only_with_complete_json(self):
        replace = cli.os.replace

        def observe(source, destination):
            self.assertEqual(self.session.read_bytes(), self.previous)
            self.assertEqual(json.loads(Path(source).read_text(encoding='utf-8')), self.state)
            replace(source, destination)

        with patch.object(cli.os, 'replace', side_effect=observe):
            cli._publish_session(self.environment, self.state)
        self.assertEqual(json.loads(self.session.read_text(encoding='utf-8')), self.state)
        self.assertEqual(list(self.environment.glob('.session-*.tmp')), [])

    def test_partial_json_write_preserves_previous_session_and_removes_own_temporary(self):
        def incomplete(_state, stream, **_kwargs):
            stream.write('{"pid":')
            raise OSError('fake disk full')

        with patch.object(cli.json, 'dump', side_effect=incomplete), self.assertRaises(OSError):
            cli._publish_session(self.environment, self.state)
        self.assertEqual(self.session.read_bytes(), self.previous)
        self.assertEqual(list(self.environment.glob('.session-*.tmp')), [])

    def test_failed_replace_preserves_previous_session_and_removes_own_temporary(self):
        with patch.object(cli.os, 'replace', side_effect=PermissionError('fake readonly session')), \
                self.assertRaises(PermissionError):
            cli._publish_session(self.environment, self.state)
        self.assertEqual(self.session.read_bytes(), self.previous)
        self.assertEqual(list(self.environment.glob('.session-*.tmp')), [])

    def test_temporary_name_collision_does_not_remove_another_file(self):
        temporary = self.environment / ('.session-' + self.state['token'] + '.tmp')
        temporary.write_bytes(b'not owned by this call')
        with self.assertRaises(FileExistsError):
            cli._publish_session(self.environment, self.state)
        self.assertEqual(temporary.read_bytes(), b'not owned by this call')
        self.assertEqual(self.session.read_bytes(), self.previous)


class StartOwnershipTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='vic2-start-owned-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.environment = self.root / '.work' / 'environment'
        self.game = self.environment / 'game'
        self.userdata = self.environment / 'userdata'
        self.game.mkdir(parents=True)
        (self.userdata / 'save games').mkdir(parents=True)
        (self.userdata / 'save games/fixture.v2').write_bytes(b'never loaded')
        executable = b'fake executable; never launched'
        (self.game / 'v2game.exe').write_bytes(executable)
        (self.environment / 'environment.json').write_text(json.dumps({
            'game': str(self.game), 'userdata': str(self.userdata),
        }), encoding='utf-8')
        self.process = Mock(pid=32101)
        self.process.poll.return_value = None
        self.process.wait.return_value = 0
        self.addCleanup(patch.stopall)
        patch.object(cli, 'ROOT', self.root).start()
        patch.object(cli, 'EXE_HASH', hashlib.sha256(executable).hexdigest()).start()
        patch.object(cli, 'mute_test_settings').start()
        patch.object(cli, 'archive_logs', return_value={}).start()
        patch.object(cli.subprocess, 'STARTUPINFO', return_value=SimpleNamespace(), create=True).start()
        self.popen = patch.object(cli.subprocess, 'Popen', return_value=self.process).start()
        self.load = patch.object(cli, 'load_campaign', return_value={'ok': True, 'campaign_loaded': True}).start()
        self.hide = patch.object(cli, 'hide_windows').start()

    def test_failed_session_publication_stops_owned_process_before_load(self):
        with patch.object(cli.os, 'replace', side_effect=PermissionError('fake publication failure')), \
                self.assertRaises(PermissionError):
            cli.start(self.environment, False, 'fixture.v2')
        self.popen.assert_called_once()
        self.process.terminate.assert_called_once_with()
        self.process.wait.assert_called_once_with(timeout=30)
        self.load.assert_not_called()
        self.hide.assert_not_called()
        self.assertFalse((self.environment / 'session.json').exists())
        self.assertEqual(list(self.environment.glob('.session-*.tmp')), [])

    def test_failed_popen_does_not_attempt_cleanup_of_another_process(self):
        self.popen.side_effect = OSError('fake process creation failure')
        with patch.object(cli, '_publish_session') as publish, self.assertRaises(OSError):
            cli.start(self.environment, False, 'fixture.v2')
        publish.assert_not_called()
        self.process.poll.assert_not_called()
        self.process.terminate.assert_not_called()
        self.load.assert_not_called()

    def test_interruption_before_publication_also_cleans_up(self):
        with patch.object(cli, '_publish_session', side_effect=KeyboardInterrupt), \
                self.assertRaises(KeyboardInterrupt):
            cli.start(self.environment, False, 'fixture.v2')
        self.process.terminate.assert_called_once_with()
        self.process.wait.assert_called_once_with(timeout=30)
        self.load.assert_not_called()

    def test_already_exited_process_is_reaped_without_terminate(self):
        self.process.poll.return_value = 2
        with patch.object(cli, '_publish_session', side_effect=OSError('fake write failure')), \
                self.assertRaises(OSError):
            cli.start(self.environment, False, 'fixture.v2')
        self.process.terminate.assert_not_called()
        self.process.wait.assert_called_once_with(timeout=30)

    def test_cleanup_failure_keeps_publication_error_as_cause(self):
        original = PermissionError('fake publication failure')
        self.process.terminate.side_effect = PermissionError('fake terminate failure')
        with patch.object(cli, '_publish_session', side_effect=original), \
                self.assertRaisesRegex(OSError, 'pid=32101.*не подтверждено') as caught:
            cli.start(self.environment, False, 'fixture.v2')
        self.assertIs(caught.exception.__cause__, original)
        self.load.assert_not_called()

    def test_published_load_failure_remains_available_for_callers_owned_cleanup(self):
        self.load.return_value = {'ok': False, 'error': 'E_LOAD_TIMEOUT'}
        result = cli.start(self.environment, False, 'fixture.v2')
        self.assertEqual(result['error'], 'E_LOAD_TIMEOUT')
        session = json.loads((self.environment / 'session.json').read_text(encoding='utf-8'))
        self.assertEqual(session['pid'], self.process.pid)
        self.assertEqual(len(session['token']), 32)
        self.process.terminate.assert_not_called()
        self.process.wait.assert_not_called()


if __name__ == '__main__':
    unittest.main()
