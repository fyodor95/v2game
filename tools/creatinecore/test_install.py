"""Проверки установки во временную копию игры без запуска настоящих артефактов."""
from contextlib import redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import install


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='vic2-install-test-')
        self.addCleanup(self.temporary.cleanup)
        temporary = Path(self.temporary.name)
        self.root = temporary / 'repository'
        self.destination = temporary / 'game'
        self.destination.mkdir()
        executable = b'fake Victoria II executable, never executed'
        (self.destination / 'v2game.exe').write_bytes(executable)
        self.addCleanup(patch.stopall)
        patch.object(install, 'ROOT', self.root).start()
        patch.object(install, 'EXPECTED_EXE', self.sha256(executable)).start()

        self.artifacts = {
            'dinput8.dll': b'new proxy artifact',
            'z_CreatineCore/creatinecore.dll': b'new core artifact',
            'z_CreatineCore/eventmusic.dll': b'new music artifact',
            'z_CreatineCore/dll_Submods/ccore_script_fixes.dll': b'new script fixes artifact',
        }
        self.build = self.root / '.work/creatinecore/build'
        self.build.mkdir(parents=True)
        self.build_manifest = {'artifacts': {}}
        for relative, content in self.artifacts.items():
            name = Path(relative).name
            (self.build / name).write_bytes(content)
            self.build_manifest['artifacts'][name] = {'sha256': self.sha256(content)}
        (self.build / 'manifest.json').write_text(
            json.dumps(self.build_manifest), encoding='utf-8')
        self.settings_relative = 'z_CreatineCore/creatinecore_settings.ini'
        self.settings = self.destination / self.settings_relative
        self.profile = self.root / 'tools/creatinecore-profile/creatinecore_settings.ini'
        self.profile.parent.mkdir(parents=True)
        self.profile_bytes = (
            '; Стандартный профиль\r\n'
            'BUILDING_GOODS_CONSUMPTION=1\r\n'
            'ENABLE_VASSAL_COLORS=1\r\n'
        ).encode('cp1251')
        self.profile.write_bytes(self.profile_bytes)

    @staticmethod
    def sha256(content):
        return hashlib.sha256(content).hexdigest()

    def run_installer(self):
        output = io.StringIO()
        with patch.object(sys, 'argv', [
                'install.py', '--destination', str(self.destination)]), redirect_stdout(output):
            install.main()
        result = json.loads(output.getvalue())
        self.assertTrue(result['ok'])
        self.assertEqual(Path(result['destination']), self.destination)
        backup = Path(result['backup'])
        self.assertEqual(backup.parent, self.root / '.work/install-backups')
        self.assertTrue(backup.is_dir())
        return backup

    def assert_installed(self, settings_bytes):
        for relative, content in self.artifacts.items():
            with self.subTest(artifact=relative):
                self.assertEqual((self.destination / relative).read_bytes(), content)
        self.assertEqual(self.settings.read_bytes(), settings_bytes)
        manifest = json.loads(
            (self.destination / 'z_CreatineCore/install-manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['artifacts'], self.build_manifest['artifacts'])
        expected = {relative: self.sha256(content) for relative, content in self.artifacts.items()}
        expected[self.settings_relative] = self.sha256(settings_bytes)
        self.assertEqual(manifest['installed'], expected)

    def test_update_preserves_existing_settings_bytes_and_backups(self):
        existing = (
            '; Настройки игрока: сохранённые значения\r\n'
            'BUILDING_GOODS_CONSUMPTION=0\r\n'
            'ENABLE_VASSAL_COLORS=0\r\n'
            '\r\n'
            'CUSTOM_UNKNOWN_OPTION = значение\r\n'
            '; Последняя строка без перевода'
        ).encode('cp1251')
        self.settings.parent.mkdir(parents=True)
        self.settings.write_bytes(existing)
        previous = {}
        for relative in self.artifacts:
            path = self.destination / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            content = ('old ' + relative).encode('ascii')
            path.write_bytes(content)
            previous[relative] = content

        backup = self.run_installer()

        self.assert_installed(existing)
        self.assertEqual(self.profile.read_bytes(), self.profile_bytes)
        previous[self.settings_relative] = existing
        for relative, content in previous.items():
            with self.subTest(backup=relative):
                self.assertEqual((backup / relative).read_bytes(), content)
        before = json.loads((backup / 'before.json').read_text(encoding='utf-8'))
        self.assertEqual(before['destination'], str(self.destination))
        self.assertEqual(before['files'], {
            relative: self.sha256(content) for relative, content in previous.items()})

    def test_fresh_install_copies_profile(self):
        backup = self.run_installer()

        self.assert_installed(self.profile_bytes)
        self.assertEqual(self.profile.read_bytes(), self.profile_bytes)
        before = json.loads((backup / 'before.json').read_text(encoding='utf-8'))
        self.assertEqual(before['files'], {
            relative: None for relative in [*self.artifacts, self.settings_relative]})
        self.assertFalse((backup / self.settings_relative).exists())


if __name__ == '__main__':
    unittest.main()
