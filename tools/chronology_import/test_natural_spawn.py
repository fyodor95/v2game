"""Автономные регрессии подготовки естественного прогона; игра не запускается."""
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from unittest.mock import Mock

from tools.chronology_import import natural_spawn as subject
from tools.chronology_import.test_engine_acceptance import event


class NaturalPreparationTests(unittest.TestCase):
    def test_observes_false_allow_and_actual_lost_rebel_control_without_failing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'common').mkdir()
            (root / 'poptypes').mkdir()
            (root / 'common/cultures.txt').write_text('germanic={north_german={}}')
            (root / 'common/religion.txt').write_text('christian={protestant={}}')
            (root / 'poptypes/farmers.txt').write_text('')
            saved = root / 'after.v2'
            saved.write_text('546={owner="PRU" controller="PRU" '
                             'farmers={size=100 north_german=protestant}}\n'
                             'OBO={primary_culture="north_german" culture={}}\n}\n')
            runner = object.__new__(subject.NaturalRunner)
            runner.request = Mock(return_value={'ok': True, 'potential': True, 'allow': False})
            manifest = {'province': 546, 'events': {'new': {'decision': 'chr_check_new'}}}
            observation = runner.observe_conditions(saved, root, manifest)
            self.assertFalse(observation['conditions_held'])
            self.assertFalse(observation['native_condition']['allow'])
            self.assertEqual(observation['saved_state']['province']['controller'], 'PRU')
            self.assertEqual(observation['saved_state']['province']['largest_cultures'], ['north_german'])
            self.assertEqual(observation['saved_state']['primary_culture'], 'north_german')
            runner.request.assert_called_once_with('decision-check PRU chr_check_new')

    def source(self):
        values = [('OBO', 'north_german'), ('SAX', 'north_german'),
                  ('GER', 'south_german'), ('D36', 'aborigine')]
        return ''.join(event(tag, culture, number).replace('option =',
                       'mean_time_to_happen={months=8} option =')
                       for number, (tag, culture) in enumerate(values, 24746)).encode()

    def test_changes_only_target_mtth_and_other_scheduling(self):
        raw = self.source()
        modified, report = subject.accelerate(raw)
        original = list(subject.engine.Spans(raw.decode()).roots())
        actual = list(subject.engine.Spans(modified.decode()).roots())
        self.assertEqual(report['n'], 2)
        self.assertEqual(report['unique_candidates'], ['OBO', 'SAX'])
        self.assertEqual(len(report['disabled_first_event_ids']), 3)
        for left, right in zip(original, actual):
            for key in ('trigger', 'option'):
                before, after = subject.engine.one(left, key), subject.engine.one(right, key)
                self.assertEqual(raw[before.start:before.end], modified[after.start:after.end])
        self.assertFalse(actual[0].fields('is_triggered_only'))
        self.assertEqual(subject.engine.scalar(subject.engine.one(actual[0], 'mean_time_to_happen'), 'months'), '0.01')
        for item in actual[1:]:
            self.assertEqual(subject.engine.scalar(item, 'is_triggered_only'), 'yes')

    def test_rejects_unscaled_mtth_and_deduplicates_candidates(self):
        with self.assertRaisesRegex(ValueError, '4n'):
            subject.accelerate(self.source().replace(b'months=8', b'months=4'))
        duplicate = event('SAX', 'north_german', 29000).replace('option =',
                       'mean_time_to_happen={months=8} option =').encode()
        # Идентичные пары получатель/культура сами являются ошибкой исходного генератора.
        with self.assertRaises(ValueError):
            subject.accelerate(self.source() + duplicate)

    def test_log_requires_exact_auto_choice_and_handles_both_encodings(self):
        text = ('vic2test: запрос=event-fire-province-chosen 546 24746\n'
                'vic2test: автоматический выбор первой опции события id=24746, всего=1\n'
                'vic2test: автоматический выбор первой опции события id=24001, всего=2\n')
        for encoding in ('utf-8', 'cp1251'):
            self.assertEqual(subject.auto_selected_ids(text.encode(encoding)), [24746, 24001])
        self.assertEqual(subject.auto_selected_ids(text.encode('utf-8') + b'\xd0'), [24746, 24001])
        self.assertEqual(subject.auto_selected_ids(b'id=24746 auto_events_selected=1'), [])

    def test_restores_all_bytes_on_exception_after_partial_setup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / 'first.txt'
            first.write_bytes(b'original\r\n')
            absent = root / 'absent.txt'
            artifacts = root / 'artifacts'
            artifacts.mkdir()
            report = {}
            with self.assertRaises(FileNotFoundError):
                with subject.temporary_files([(first, b'test'), (absent, b'fail')], artifacts, report):
                    self.fail('Не должен доходить до тела')
            self.assertEqual(first.read_bytes(), b'original\r\n')
            self.assertIn(str(first), report['restored_files'])
            with self.assertRaises(RuntimeError):
                with subject.temporary_files([(first, b'second')], artifacts, report):
                    raise RuntimeError('Имитируем отказ запуска')
            self.assertEqual(first.read_bytes(), b'original\r\n')

    def test_snr_fixture_is_guarded_localised_and_refuses_id_collision(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'events').mkdir()
            with patch.object(subject.engine, 'work_path', side_effect=lambda value: Path(value)):
                manifest = subject.prepare_snr(root, root / 'package')
                body = (root / 'package/events/chronology_snr_test.txt').read_text(encoding='cp1251')
                parsed = list(subject.engine.Spans(body).roots())
                self.assertEqual(len(parsed), 1)
                self.assertEqual(manifest['event_id'], 3997301)
                self.assertIn('546 = { add_core = SNR secede_province = SNR }', body)
                self.assertIn('tag = PRU NOT = { exists = SNR }', body)
                self.assertIn('Сеннар', (root / 'package/localisation/chronology_snr_test.csv').read_text(encoding='cp1251'))
                (root / 'events/collision.txt').write_text('country_event={\n id=3997301\n}')
                with self.assertRaisesRegex(ValueError, 'занят'):
                    subject.prepare_snr(root, root / 'second')
                self.assertFalse((root / 'second').exists())


if __name__ == '__main__':
    unittest.main()
