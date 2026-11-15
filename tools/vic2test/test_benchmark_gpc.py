"""Fingerprint и трактовка замера на временных данных, без запуска игры."""
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import benchmark_gpc as benchmark
import cli


class FingerprintTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='gpc-benchmark-input-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.environment = self.root / '.work' / 'environment'
        self.game = self.environment / 'game'
        self.userdata = self.environment / 'userdata'
        self.game.mkdir(parents=True)
        (self.game / 'gfx').mkdir()
        self.userdata.mkdir()
        (self.environment / 'environment.json').write_text(json.dumps({
            'game': str(self.game), 'userdata': str(self.userdata),
        }), encoding='utf-8')
        (self.game / 'userdir.txt').write_text(self.userdata.as_posix() + '/', encoding='ascii')
        files = {
            'v2game.exe': b'fake exe; never launched',
            'dinput8.dll': b'fake DLL; never loaded',
            'common/defines.lua': b'POP_SIZE = 100',
            'events/test.txt': b'country_event = { id = 9 }',
            'map/default.map': b'max_provinces=100',
            'unknown_active_directory/rules': b'unknown input is included',
            'settings.txt': b'fullScreen=no\r\nmaster_volume=0.000000\r\n',
            'messagetypes_custom.txt': b'notice={popup=no}\n',
            'z_CreatineCore/great_power_culture_cores.ini': b'ENABLE_GREAT_POWER_CULTURE_CORES=1\r\n',
        }
        for name, data in files.items():
            path = self.game / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (self.userdata / 'settings.txt').write_bytes(b'fullScreen=no\r\nmaster_volume=0.000000\r\n')
        (self.userdata / 'messagetypes_custom.txt').write_bytes(b'notice={popup=no}\n')
        self.patcher = patch.object(cli, 'ROOT', self.root)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)

    def fingerprint(self):
        return benchmark.input_fingerprint(self.environment)

    def test_game_scripts_map_unknown_inputs_and_local_settings_all_change_fingerprint(self):
        baseline = self.fingerprint()
        names = ['game/dinput8.dll', 'game/common/defines.lua', 'game/events/test.txt',
                 'game/map/default.map', 'game/unknown_active_directory/rules',
                 'game/settings.txt', 'game/messagetypes_custom.txt',
                 'userdata/settings.txt', 'userdata/messagetypes_custom.txt']
        for name in names:
            with self.subTest(name=name):
                path = self.environment / name
                original = path.read_bytes()
                path.write_bytes(original + b'\nchanged\n')
                try:
                    current = self.fingerprint()
                    self.assertNotEqual(current['fingerprint'], baseline['fingerprint'])
                    self.assertEqual([item['path'] for item in benchmark.input_differences(baseline, current)], [name])
                finally:
                    path.write_bytes(original)

    def test_only_expected_gpc_mode_bytes_are_allowed(self):
        baseline = self.fingerprint()
        key = 'game/z_CreatineCore/great_power_culture_cores.ini'
        path = self.environment / key
        off = benchmark.mode_config(path.read_bytes(), 0)
        path.write_bytes(off)
        current = self.fingerprint()
        self.assertEqual(benchmark.input_differences(baseline, current, config_path=key, config_bytes=off), [])
        (self.game / 'events/test.txt').write_bytes(b'country_event = { id = 10 }')
        changed = benchmark.input_differences(baseline, self.fingerprint(), config_path=key, config_bytes=off)
        self.assertEqual([item['path'] for item in changed], ['game/events/test.txt'])

    def test_appearance_of_previously_absent_settings_override_is_detected(self):
        path = self.userdata / 'messagetypes_custom.txt'
        path.unlink()
        baseline = self.fingerprint()
        self.assertEqual(baseline['entries']['userdata/messagetypes_custom.txt'], {'kind': 'missing'})
        path.write_bytes(b'notice={popup=yes}\n')
        changed = benchmark.input_differences(baseline, self.fingerprint())
        self.assertEqual([item['path'] for item in changed], ['userdata/messagetypes_custom.txt'])

    def test_logs_and_derived_cache_are_excluded_but_userdir_bytes_are_recorded(self):
        baseline = self.fingerprint()
        for name in ('logs/output.log', 'z_CreatineCore/Logs/v2dll.log', 'map/cache/generated.bin', 'gfx/cache/generated.bin'):
            path = self.game / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'local derived output')
        self.assertEqual(self.fingerprint()['fingerprint'], baseline['fingerprint'])
        baseline = self.fingerprint()
        path = self.game / 'userdir.txt'
        path.write_bytes(path.read_bytes() + b'\n')
        changed = benchmark.input_differences(baseline, self.fingerprint())
        self.assertEqual([item['path'] for item in changed], ['game/userdir.txt'])

    def test_redirect_outside_environment_is_rejected(self):
        (self.game / 'userdir.txt').write_text(self.root.as_posix(), encoding='ascii')
        with self.assertRaises(benchmark.mp_preflight.PreflightError):
            self.fingerprint()

    def test_changed_script_blocks_mode_before_starting_a_process(self):
        args = SimpleNamespace(environment=self.environment, days=100, samples=3, order='off-on',
                               timeout=600, transition_timeout=600, save='fixture.v2', no_cpu=True)
        controller = benchmark.Benchmark(args, io.StringIO(), self.root / 'report.json')
        controller.game = self.game
        controller.config = self.game / 'z_CreatineCore/great_power_culture_cores.ini'
        controller.original_ini = controller.config.read_bytes()
        controller.save = self.userdata / 'fixture.v2'
        controller.save.write_bytes(b'unchanged source save')
        controller.artifacts.mkdir()
        controller.record['source_save'] = {'sha256': cli.digest(controller.save)}
        controller.record['initial_artifacts'] = benchmark.artifact_manifest(self.game)
        controller.input_baseline = self.fingerprint()
        (self.game / 'events/test.txt').write_bytes(b'country_event = { id = 10 }')
        with patch.object(cli, 'start') as start, self.assertRaisesRegex(benchmark.BenchmarkError, 'game/events/test.txt'):
            controller.run_mode(0)
        start.assert_not_called()


class MeasurementDescriptionTests(unittest.TestCase):
    def test_summary_keeps_observed_medians_and_does_not_claim_one_sided_bias(self):
        args = SimpleNamespace(environment=Path('unused'), days=100, samples=3, order='off-on',
                               timeout=600, transition_timeout=600, save='fixture.v2', no_cpu=True)
        controller = benchmark.Benchmark(args, io.StringIO(), Path('unused.json'))
        controller.record['modes'] = [
            {'name': name, 'complete': True, 'samples': [
                {'complete': True, 'wall_seconds_per_day': value,
                 'auto_event_choices': {'warmup': 0, 'timed': 0}}
                for value in values]}
            for name, values in (('off', (1.0, 1.1, .9)), ('on', (1.04, 1.03, 1.05)))
        ]
        controller.summarize()
        self.assertTrue(controller.record['ok'])
        self.assertAlmostEqual(controller.record['comparison']['overhead_percent'], 4)
        self.assertEqual(controller.record['boundary_uncertainty'],
                         {'direction': 'unknown', 'numeric_correction_applied': False})
        self.assertIn('в обе стороны', controller.record['warnings'][-1])


if __name__ == '__main__':
    unittest.main()
