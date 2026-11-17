"""Проверки сравнения/целостности/cleanup инструмента; игровые процессы не создаются."""
import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from tools.chronology_import import benchmark_core_events as b


def fingerprint(files):
    entries = {name: {'kind': 'file', 'size': len(data), 'sha256': b.sha_bytes(data)} for name, data in files.items()}
    return {'entries': entries, 'fingerprint': b.sha_bytes(json.dumps(entries, sort_keys=True).encode())}


def arguments(root):
    return argparse.Namespace(environment=root / 'environment', output=root / 'report.json',
                              saves=['normal.v2', 'rebels.v2'], samples=3, days=100, dry_run=False,
                              order='old-new', new_events=root / 'new.txt', visible=False, warmup_days=33,
                              rebel_save=None, rebel_province=546)


class BenchmarkTests(unittest.TestCase):
    def test_rebel_control_guard_records_actual_state_and_rejects_retake(self):
        runner = b.Benchmark(arguments(Path('fixture')))
        runner.args.rebel_save = 'rebels.v2'
        process = Mock(pid=123)
        province = dict(id=546, owner='PRU', controller='REB', candidate_vector={'count': 215})
        probe = dict(complete=True, province=province, world={'date_raw': 59884152})
        with patch.object(b, 'session', return_value={}), patch.object(b.native_event_probe, 'capture', return_value=probe) as capture:
            self.assertIsNone(runner.rebel_control(process, 'normal.v2'))
            capture.assert_not_called()
            self.assertEqual(runner.rebel_control(process, 'rebels.v2')['controller'], 'REB')
            province['controller'] = 'PRU'
            with self.assertRaisesRegex(b.BenchmarkError, 'повстанцами'):
                runner.rebel_control(process, 'rebels.v2')

    def test_empty_or_unqualified_event_registry_rejects_measurement(self):
        runner = b.Benchmark(arguments(Path('fixture')))
        status = dict(ok=True, pid=123, multiplayer=False, campaign_loaded=True, paused=True,
                      advance_target=0, date_raw=59883360, auto_events_selected=0,
                      all_province_events=2362, random_province_events=2343)
        gpc = dict(ok=True, installed=True, enabled=0, failed=0, busy=0)
        with patch.object(b.cli, 'request', side_effect=[status, gpc]):
            self.assertEqual(runner.state(Mock(pid=123)), status)
        for total, random in [(2362, 0), (None, None), (2362, None), (0, 1), (2362, True)]:
            with self.subTest(total=total, random=random):
                invalid = dict(status, all_province_events=total, random_province_events=random)
                with patch.object(b.cli, 'request', side_effect=[invalid, gpc]):
                    with self.assertRaisesRegex(b.BenchmarkError, 'реестр'):
                        runner.state(Mock(pid=123))

    def test_compact_report_does_not_publish_manifest(self):
        source = fingerprint({'data': b'bytes'})
        result = b.compact(source)
        self.assertEqual(set(result), {'sha256', 'entries'})
        self.assertEqual(result['entries'], 1)
        self.assertIsInstance(source['entries'], dict)

    def test_only_expected_event_and_ini_replacements_are_allowed(self):
        files = {'game/' + b.EVENT: b'old', 'game/' + b.GPC_INI: b'on', 'game/' + b.CORE_INI: b'core'}
        before = fingerprint(files)
        replacements = {'game/' + b.EVENT: b'new', 'game/' + b.GPC_INI: b'off'}
        changed = {**files, **replacements}
        self.assertEqual(b.differences(before, fingerprint(changed), replacements), [])
        changed['game/' + b.CORE_INI] = b'corrupt'
        self.assertEqual([item['path'] for item in b.differences(before, fingerprint(changed), replacements)],
                         ['game/' + b.CORE_INI])
        self.assertEqual(before, fingerprint(files))
        with self.assertRaises(b.BenchmarkError):
            b.differences(before, before, {'nonexistent': b'x'})

    def test_comparison_separates_saves_and_uses_medians(self):
        modes = []
        for name, factor in [('old', 1), ('new', 1.2)]:
            samples = []
            for save, scale in [('normal.v2', 1), ('rebels.v2', 10)]:
                for value in [1, 2, 100]:
                    samples.append(dict(save=save, complete=True, wall_seconds_per_day=scale * value * factor,
                                        cpu_seconds_per_day=scale * value * factor / 2))
            modes.append(dict(name=name, samples=samples))
        result = b.comparison(modes, ['normal.v2', 'rebels.v2'], 3)
        self.assertAlmostEqual(result['rebels.v2']['change_percent']['wall_seconds_per_day'], 20)
        self.assertEqual(result['normal.v2']['medians']['old']['wall_seconds_per_day'], 2)
        modes[1]['samples'][0]['complete'] = False
        with self.assertRaises(b.BenchmarkError):
            b.comparison(modes, ['normal.v2', 'rebels.v2'], 3)

    def test_start_failure_after_session_publication_stops_only_owned_process(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runner = b.Benchmark(arguments(root))
            runner.game, runner.exe = root / 'game', root / 'game/v2game.exe'
            (runner.game / 'events').mkdir(parents=True)
            (runner.game / 'z_CreatineCore').mkdir()
            runner.events = {'old': b'old'}
            runner.disabled_gpc = b'off'
            runner.verify_inputs = Mock(return_value={})
            new = {'token': 'b' * 32, 'pid': 123, 'game': str(runner.game)}
            owned = Mock()
            with patch.object(b, 'require_idle'), patch.object(b, 'session', side_effect=[{'token': 'a' * 32}, new]), \
                    patch.object(b.cli, 'start', side_effect=RuntimeError('after publication')), \
                    patch.object(b, 'OwnedProcess', return_value=owned) as acquire:
                with self.assertRaisesRegex(RuntimeError, 'after publication'):
                    runner.run_mode('old')
            acquire.assert_called_once_with(123, runner.exe)
            owned.stop.assert_called_once()
            self.assertTrue(runner.record['modes'][0]['process_stopped'])

    def test_failure_restores_both_ini_and_events(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runner = b.Benchmark(arguments(root))
            runner.game = root / 'game'
            runner.originals = {b.EVENT: b'original events', b.CORE_INI: b'original core', b.GPC_INI: b'original gpc'}
            for name, data in runner.originals.items():
                (runner.game / name).parent.mkdir(parents=True, exist_ok=True)
                (runner.game / name).write_bytes(data)
            runner.root_ini_hashes = {}
            runner.events = {'old': b'old', 'new': b'new'}
            runner.record['event_sets'] = {}
            runner.preflight = Mock()
            runner.verify_saves = Mock()

            def failing_mode(name):
                for path in runner.originals:
                    (runner.game / path).write_bytes(b'changed')
                raise RuntimeError('sample failed')

            runner.run_mode = failing_mode
            snapshot = lambda env: fingerprint({'game/' + name: (runner.game / name).read_bytes()
                                               for name in runner.originals})
            with patch.object(b.cli, 'mute_test_settings'), patch.object(b, 'input_fingerprint', side_effect=snapshot):
                result, code = runner.run()
            self.assertEqual(code, 1)
            self.assertFalse(result['complete'])
            self.assertEqual({name: (runner.game / name).read_bytes() for name in runner.originals}, runner.originals)
            record = json.loads(runner.output.read_text(encoding='utf-8'))
            self.assertEqual(record['inputs_original'], record['inputs_restored'])
            self.assertEqual(len(record['restored_files']), 3)


if __name__ == '__main__':
    unittest.main()
