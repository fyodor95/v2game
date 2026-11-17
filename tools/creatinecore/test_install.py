"""Проверки установки во временную копию игры без запуска настоящих артефактов."""
from contextlib import redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

import install
import build as core_build


RESOURCE_BYTES = {
    'interface/country_politics.gui': b'guiTypes = { windowType = { name = "country_politics" } }\r\n',
    'localisation/country_state_religion.csv': (
        'CC_STATE_RELIGION_catholic;Католицизм;;;;;;;;;;;;;x\r\n').encode('cp1251'),
    'events/great_power_culture_cores.txt': (
        '# Уведомление о новых национальных провинциях\r\n'
        'country_event = { id = 3997200 is_triggered_only = yes }\r\n').encode('cp1251'),
    'localisation/great_power_culture_cores.csv': (
        '#CODE;ENGLISH;FRENCH;GERMAN;POLISH;SPANISH;ITALIAN;SWEDISH;CZECH;HUNGARIAN;DUTCH;PORTUGESE;RUSSIAN;FINNISH;x\r\n'
        'GPC_TITLE;Новые национальные провинции;;;;;;;;;;;;;x\r\n').encode('cp1251'),
}


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
            'z_CreatineCore/dll_Submods/ccore_great_power_culture_cores.dll': b'new GPC artifact',
            'z_CreatineCore/dll_Submods/ccore_state_religion_ui.dll': b'new religion UI artifact',
            'z_CreatineCore/great_power_culture_cores.ini': (
                b'[GreatPowerCultureCores]\r\nENABLE_GREAT_POWER_CULTURE_CORES=0\r\n'
                b'GREAT_POWER_CULTURE_CORES_MTTH_MONTHS=60\r\n'),
            **RESOURCE_BYTES,
        }
        self.build = self.root / '.work/creatinecore/build'
        self.build.mkdir(parents=True)
        self.build_manifest = {'artifacts': {}}
        for relative, content in self.artifacts.items():
            name = relative if relative in RESOURCE_BYTES else Path(relative).name
            path = self.build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
            self.build_manifest['artifacts'][name] = {
                'sha256': self.sha256(content), 'bytes': len(content)}
        (self.build / 'manifest.json').write_text(
            json.dumps(self.build_manifest), encoding='utf-8')
        self.settings_relative = 'z_CreatineCore/creatinecore_settings.ini'
        self.settings = self.destination / self.settings_relative
        self.gpc_settings_relative = 'z_CreatineCore/great_power_culture_cores.ini'
        self.gpc_settings = self.destination / self.gpc_settings_relative
        self.install_manifest_relative = 'z_CreatineCore/install-manifest.json'
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

    def assert_installed(self, settings_bytes, gpc_settings_bytes=None):
        if gpc_settings_bytes is None:
            gpc_settings_bytes = self.artifacts[self.gpc_settings_relative]
        for relative, content in self.artifacts.items():
            with self.subTest(artifact=relative):
                if relative == self.gpc_settings_relative:
                    content = gpc_settings_bytes
                self.assertEqual((self.destination / relative).read_bytes(), content)
        self.assertEqual(self.settings.read_bytes(), settings_bytes)
        manifest = json.loads(
            (self.destination / 'z_CreatineCore/install-manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['artifacts'], self.build_manifest['artifacts'])
        expected = {relative: self.sha256(content) for relative, content in self.artifacts.items()}
        expected[self.settings_relative] = self.sha256(settings_bytes)
        expected[self.gpc_settings_relative] = self.sha256(gpc_settings_bytes)
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
        existing_gpc = (
            '; Все настройки игрока сохраняются побайтно\r\n'
            '[GreatPowerCultureCores]\r\n'
            'ENABLE_GREAT_POWER_CULTURE_CORES=1\r\n'
            'GREAT_POWER_CULTURE_CORES_MTTH_MONTHS=120\r\n'
            'UNKNOWN_FUTURE_OPTION=значение'
        ).encode('cp1251')
        self.gpc_settings.write_bytes(existing_gpc)
        previous[self.gpc_settings_relative] = existing_gpc
        old_manifest = b'{"previous": "installation"}\n'
        (self.destination / self.install_manifest_relative).write_bytes(old_manifest)
        previous[self.install_manifest_relative] = old_manifest
        previous[self.settings_relative] = existing

        original_copy = install.shutil.copy2

        def check_backup_before_destination_write(source, destination, *args, **kwargs):
            if Path(destination).is_relative_to(self.destination):
                backups = list((self.root / '.work/install-backups').iterdir())
                self.assertEqual(len(backups), 1)
                for relative, content in previous.items():
                    self.assertEqual((backups[0] / relative).read_bytes(), content)
                self.assertTrue((backups[0] / 'before.json').is_file())
            return original_copy(source, destination, *args, **kwargs)

        with patch.object(install.shutil, 'copy2', side_effect=check_backup_before_destination_write):
            backup = self.run_installer()

        self.assert_installed(existing, existing_gpc)
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
            relative: None for relative in [*self.artifacts, self.settings_relative,
                                           self.install_manifest_relative]})
        self.assertFalse((backup / self.settings_relative).exists())
        self.assertIn(b'ENABLE_GREAT_POWER_CULTURE_CORES=0', self.gpc_settings.read_bytes())

    def test_changed_gpc_artifact_rejected_before_any_destination_write(self):
        for name in ('ccore_great_power_culture_cores.dll', 'ccore_state_religion_ui.dll', 'great_power_culture_cores.ini',
                     *RESOURCE_BYTES):
            with self.subTest(name=name):
                path = self.build / name
                original = path.read_bytes()
                path.write_bytes(original + b' changed after manifest')
                try:
                    with self.assertRaisesRegex(RuntimeError, 'Хеш артефакта'):
                        self.run_installer()
                finally:
                    path.write_bytes(original)
                self.assertEqual([p.name for p in self.destination.iterdir()], ['v2game.exe'])
                self.assertFalse((self.root / '.work/install-backups').exists())

    def test_missing_resource_rejected_before_any_destination_write(self):
        for name in (*RESOURCE_BYTES, 'ccore_state_religion_ui.dll'):
            with self.subTest(name=name):
                path = self.build / name
                content = path.read_bytes()
                path.unlink()
                try:
                    with self.assertRaisesRegex(RuntimeError, 'В пакете отсутствует'):
                        self.run_installer()
                finally:
                    path.write_bytes(content)
                self.assertEqual([p.name for p in self.destination.iterdir()], ['v2game.exe'])
                self.assertFalse((self.root / '.work/install-backups').exists())

    def test_missing_resource_manifest_entry_rejects_legacy_package(self):
        for name in (*RESOURCE_BYTES, 'ccore_state_religion_ui.dll'):
            with self.subTest(name=name):
                manifest = {'artifacts': dict(self.build_manifest['artifacts'])}
                del manifest['artifacts'][name]
                (self.build / 'manifest.json').write_text(json.dumps(manifest), encoding='utf-8')
                with self.assertRaisesRegex(RuntimeError, 'В манифесте отсутствует'):
                    self.run_installer()
                self.assertEqual([p.name for p in self.destination.iterdir()], ['v2game.exe'])
                self.assertFalse((self.root / '.work/install-backups').exists())

    def test_resource_manifest_size_mismatch_rejected_before_write(self):
        name = 'localisation/great_power_culture_cores.csv'
        self.build_manifest['artifacts'][name]['bytes'] += 1
        (self.build / 'manifest.json').write_text(json.dumps(self.build_manifest), encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'Размер артефакта'):
            self.run_installer()
        self.assertEqual([p.name for p in self.destination.iterdir()], ['v2game.exe'])
        self.assertFalse((self.root / '.work/install-backups').exists())

    def test_resource_backup_failure_prevents_all_destination_writes(self):
        previous = {}
        for relative in [*self.artifacts, self.settings_relative, self.install_manifest_relative]:
            path = self.destination / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            previous[relative] = ('previous ' + relative).encode('ascii')
            path.write_bytes(previous[relative])
        target = self.destination / 'localisation/great_power_culture_cores.csv'
        original_copy = install.shutil.copy2

        def fail_resource_backup(source, destination, *args, **kwargs):
            if Path(source) == target:
                raise OSError('simulated resource backup failure')
            return original_copy(source, destination, *args, **kwargs)

        with patch.object(install.shutil, 'copy2', side_effect=fail_resource_backup):
            with self.assertRaisesRegex(OSError, 'simulated resource backup failure'):
                self.run_installer()
        for relative, content in previous.items():
            self.assertEqual((self.destination / relative).read_bytes(), content)

    def test_partial_install_keeps_complete_backup_and_failure_report(self):
        previous = {}
        for relative in [*self.artifacts, self.settings_relative, self.install_manifest_relative]:
            path = self.destination / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            previous[relative] = ('previous ' + relative).encode('ascii')
            path.write_bytes(previous[relative])
        # Имитируется сбой после первых обновлений, с частично записанной новой DLL.
        target = self.destination / 'z_CreatineCore/dll_Submods/ccore_great_power_culture_cores.dll'
        original_copy = install.shutil.copy2

        def partial_copy(source, destination, *args, **kwargs):
            if Path(destination) == target:
                target.write_bytes(b'partial native DLL')
                raise OSError('simulated write failure')
            return original_copy(source, destination, *args, **kwargs)

        with patch.object(install.shutil, 'copy2', side_effect=partial_copy):
            with self.assertRaisesRegex(RuntimeError, 'резервная копия:'):
                self.run_installer()
        backups = list((self.root / '.work/install-backups').iterdir())
        self.assertEqual(len(backups), 1)
        backup = backups[0]
        for relative, content in previous.items():
            self.assertEqual((backup / relative).read_bytes(), content)
        before = json.loads((backup / 'before.json').read_text(encoding='utf-8'))
        self.assertEqual(before['files'], {
            relative: self.sha256(content) for relative, content in previous.items()})
        report = json.loads((backup / 'result.json').read_text(encoding='utf-8'))
        self.assertFalse(report['ok'])
        self.assertTrue(report['partial'])
        self.assertIn(str(target.relative_to(self.destination)).replace('\\', '/'), report['attempted'])
        self.assertEqual(self.settings.read_bytes(), previous[self.settings_relative])
        self.assertEqual(self.gpc_settings.read_bytes(), previous[self.gpc_settings_relative])
        self.assertEqual((self.destination / self.install_manifest_relative).read_bytes(),
                         previous[self.install_manifest_relative])


class GpcArtifactTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='vic2-gpc-build-test-')
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.source, self.product, self.destination = (root / name for name in ('source', 'product', 'build'))
        for path in (self.source, self.product, self.destination):
            path.mkdir()
        for name in ('runtime_status.h', 'scheduler.h', 'runtime.cpp', 'build_runtime.cmd', 'write_manifest.py'):
            (self.source / name).write_bytes(('fake source: ' + name).encode('ascii'))
        self.ini = b'[GreatPowerCultureCores]\nENABLE_GREAT_POWER_CULTURE_CORES=0\n'
        (self.source / 'great_power_culture_cores.ini').write_bytes(self.ini)
        binary = bytearray(128)
        binary[:2] = b'MZ'
        binary[60:64] = (64).to_bytes(4, 'little')
        binary[64:68] = b'PE\0\0'
        binary[68:70] = (0x14c).to_bytes(2, 'little')
        self.binary = bytes(binary)
        name = 'ccore_great_power_culture_cores.dll'
        (self.product / name).write_bytes(self.binary)
        self.record = {
            'schema': 1, 'machine': 'x86', 'expected_exe_sha256': 'qualified-exe',
            'head': 'informational-only',
            'artifact': {'name': name, 'bytes': len(self.binary),
                         'sha256': hashlib.sha256(self.binary).hexdigest()},
            'sources': {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in self.source.iterdir()},
        }
        (self.product / 'manifest.json').write_text(json.dumps(self.record), encoding='utf-8')

    def stage(self):
        return core_build.stage_gpc_artifacts(self.source, self.product, self.destination, 'qualified-exe')

    def test_stages_verified_module_and_disabled_ini(self):
        self.assertEqual(self.stage(), self.record)
        self.assertEqual((self.destination / 'ccore_great_power_culture_cores.dll').read_bytes(), self.binary)
        self.assertEqual((self.destination / 'great_power_culture_cores.ini').read_bytes(), self.ini)

    def test_changed_status_header_rejects_stale_binary_before_staging(self):
        (self.source / 'runtime_status.h').write_bytes(b'changed ABI after build')
        with self.assertRaisesRegex(RuntimeError, 'Исходники GPC изменились'):
            self.stage()
        self.assertEqual(list(self.destination.iterdir()), [])

    def test_wrong_binary_hash_rejected_before_staging(self):
        path = self.product / 'ccore_great_power_culture_cores.dll'
        path.write_bytes(self.binary + b'changed after manifest')
        with self.assertRaisesRegex(RuntimeError, 'Артефакт GPC отличается'):
            self.stage()
        self.assertEqual(list(self.destination.iterdir()), [])

    def write_resources(self, root):
        for relative, content in RESOURCE_BYTES.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)

    def test_stages_resources_and_hashes_without_encoding_changes(self):
        self.write_resources(self.source)
        record = core_build.stage_resources(self.source, self.destination, tuple(RESOURCE_BYTES))
        self.assertEqual(record, {
            relative: {'sha256': hashlib.sha256(content).hexdigest(), 'bytes': len(content)}
            for relative, content in RESOURCE_BYTES.items()})
        for relative, content in RESOURCE_BYTES.items():
            self.assertEqual((self.destination / relative).read_bytes(), content)
            self.assertEqual((self.source / relative).read_bytes(), content)

    def test_missing_resource_rejects_before_staging_first_file(self):
        self.write_resources(self.source)
        for relative, content in RESOURCE_BYTES.items():
            with self.subTest(relative=relative):
                path = self.source / relative
                path.unlink()
                try:
                    with self.assertRaisesRegex(RuntimeError, 'Отсутствует игровой ресурс'):
                        core_build.stage_resources(self.source, self.destination, tuple(RESOURCE_BYTES))
                finally:
                    path.write_bytes(content)
                self.assertEqual(list(self.destination.iterdir()), [])

    def test_resource_copy_corruption_rejects_staging(self):
        self.write_resources(self.source)
        original_copy = core_build.shutil.copy2

        def corrupt_resource(source, destination, *args, **kwargs):
            result = original_copy(source, destination, *args, **kwargs)
            Path(destination).write_bytes(b'corrupted resource')
            return result

        with patch.object(core_build.shutil, 'copy2', side_effect=corrupt_resource):
            with self.assertRaisesRegex(RuntimeError, 'Ошибка копирования игрового ресурса'):
                core_build.stage_gpc_resources(self.source, self.destination)

    def test_main_build_manifest_contains_gpc_binary_ini_and_source_provenance(self):
        # Проверяется общая оркестрация без Git, VS и исполнения fake DLL.
        root = self.destination / 'repository'
        work = root / '.work/creatinecore'
        output = work / 'build'
        upstream = work / 'upstream'
        tools = root / 'tools/creatinecore'
        source = tools / 'gpc'
        output.mkdir(parents=True)
        shutil.copytree(self.source, source)
        product = root / '.work/gpc-build'
        shutil.copytree(self.product, product)
        executable = b'fake qualified executable; never executed'
        (root / 'v2game.exe').write_bytes(executable)
        self.write_resources(root)
        record = {**self.record, 'expected_exe_sha256': hashlib.sha256(executable).hexdigest()}
        (product / 'manifest.json').write_text(json.dumps(record), encoding='utf-8')
        for name in ('creatinecore.dll', 'dinput8.dll', 'eventmusic.dll', 'ccore_script_fixes.dll',
                     'ccore_state_religion_ui.dll'):
            (output / name).write_bytes(self.binary)
        api = upstream / 'Source/V2/V2/V2ModAPI.h'
        api.parent.mkdir(parents=True)
        api.write_bytes(b'fake build API')
        (tools / 'v2game.patch').write_bytes(b'fake local patch')
        (tools / 'religion.patch').write_bytes(b'fake religion patch')
        (tools / 'religion').mkdir()
        (tools / 'religion/native.cpp').write_bytes(b'fake native religion source')
        (tools / 'script_fixes.vcxproj').write_bytes(b'fake project')
        replacements = {'ROOT': root, 'WORK': work, 'UPSTREAM': upstream,
                        'BUILD': output, '__file__': str(tools / 'build.py')}
        with patch.multiple(core_build, **replacements), \
                patch.object(core_build, 'prepare'), \
                patch.object(core_build, 'msbuild_path', return_value=Path('fake-MSBuild.exe')), \
                patch.object(core_build, 'core_project', return_value=tools / 'core.vcxproj'), \
                patch.object(core_build, 'generate'), \
                patch.object(core_build, 'run') as run, redirect_stdout(io.StringIO()):
            core_build.build()
        self.assertEqual(run.call_count, 6)
        self.assertTrue(all(call.args[0] == 'fake-MSBuild.exe' for call in run.call_args_list[:5]))
        self.assertEqual(run.call_args_list[-1].args,
                         ('cmd.exe', '/d', '/c', str(source / 'build_runtime.cmd')))
        aggregate = json.loads((output / 'manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(aggregate['great_power_culture_cores'], record)
        self.assertEqual(aggregate['state_religion_sources'], {
            'native.cpp': hashlib.sha256(b'fake native religion source').hexdigest()})
        self.assertEqual(aggregate['patches']['religion.patch'],
                         hashlib.sha256(b'fake religion patch').hexdigest())
        self.assertEqual(aggregate['artifacts']['ccore_great_power_culture_cores.dll']['sha256'],
                         hashlib.sha256(self.binary).hexdigest())
        self.assertEqual(aggregate['artifacts']['great_power_culture_cores.ini']['sha256'],
                         hashlib.sha256(self.ini).hexdigest())
        for relative, content in RESOURCE_BYTES.items():
            self.assertEqual((output / relative).read_bytes(), content)
            self.assertEqual(aggregate['artifacts'][relative], {
                'sha256': hashlib.sha256(content).hexdigest(), 'bytes': len(content)})


if __name__ == '__main__':
    unittest.main()
