"""Регрессии MP-пакета и протокола без запуска игры и нативных вызовов."""
import copy
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import Mock, patch

from tools.culture_religion import mp_acceptance as subject


def first(number=24001, tag='D45', culture='aborigine', religion='animist'):
    trigger = f'controlled_by_rebels=yes culture={culture} pop_majority_religion={religion}'
    return (f'province_event={{id={number} trigger={{{trigger}}} '
            'mean_time_to_happen={months=8} '
            f'option={{name=ok if={{limit={{{trigger}}} owner={{{tag}={{primary_culture={culture} religion={religion}}}}} '
            f'add_core={tag}}}}}}}')


class PackageTests(unittest.TestCase):
    def test_acceleration_preserves_exact_guards_effects_and_spread(self):
        spread = 'province_event={id=26001 trigger={culture=aborigine} mean_time_to_happen={months=4} option={add_core=D45}}'
        raw = ('# source\r\n' + first() + '\r\n' + first(24002, 'D46', 'yoruba') + '\r\n' + spread).encode()
        actual, report = subject.accelerate(raw)
        self.assertEqual(report['disabled_first_ids'], [24002])
        self.assertIn(spread.encode(), actual)
        before = list(subject.engine.Spans(raw.decode()).roots())
        after = list(subject.engine.Spans(actual.decode()).roots())
        self.assertEqual(len(after), 3)
        for left, right in zip(before, after):
            for key in ('trigger', 'option'):
                a, b = subject.engine.one(left, key), subject.engine.one(right, key)
                self.assertEqual(raw[a.start:a.end], actual[b.start:b.end])
        self.assertIsNone(after[0].single('is_triggered_only'))
        self.assertEqual(subject.engine.scalar(after[1], 'is_triggered_only'), 'yes')
        self.assertEqual(subject.engine.scalar(subject.engine.one(after[0], 'mean_time_to_happen'), 'days'), '1')

    def test_acceleration_rejects_changed_mapping_and_guard(self):
        for text in (first(culture='yoruba'), first(tag='D44'), first(number=24002),
                     first().replace('limit={controlled_by_rebels=yes', 'limit={controlled_by_rebels=no')):
            with self.subTest(text=text):
                with self.assertRaises(ValueError): subject.accelerate(text.encode())

    def test_decision_effect_is_copied_exactly_and_marker_appended(self):
        body = '\r\n primary_culture = north_german\r\n set_religion_from_primary_culture = yes\r\n any_pop={militancy=-10}\r\n'
        raw = ('political_decisions={D_primary_north_german={effect={' + body + '}}}').encode()
        result, report = subject.decision_event(raw)
        self.assertIn(body.encode(), result)
        self.assertEqual(report['effects_sha256'], subject.sha(body.encode()))
        event = next(subject.engine.Spans(result.decode()).roots())
        option = subject.engine.one(event, 'option')
        self.assertEqual([field.key for field in option.value],
                         ['name', 'primary_culture', 'set_religion_from_primary_culture', 'any_pop', 'set_country_flag'])
        self.assertEqual(subject.engine.scalar(subject.engine.one(event, 'trigger'), 'date'), '1836.1.2')

    def test_rebel_save_only_changes_selected_fields_and_relocates_army(self):
        raw = (b'date="1836.1.1" player="PRU"\r\n'
               b'546={owner=PRU controller=PRU core=D45 life_rating=40 farmers={size=123.5 north_german=protestant cash=10}}\r\n'
               b'549={owner=PRU controller=PRU life_rating=40 core=D45}\r\n'
               b'594={owner=D50 controller=D50 farmers={size=100 north_german=protestant}}\r\n'
               b'PRU={army={location=546 regiment={home=546}}}\r\n'
               b'D45={primary_culture=british religion=catholic}\r\n'
               b'D50={primary_culture=british religion=jewish}\r\n}\r\n')
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory)
            (game / 'common').mkdir(); (game / 'poptypes').mkdir()
            (game / 'common/cultures.txt').write_text('x={aborigine={} north_german={}}')
            (game / 'common/religion.txt').write_text('x={animist={} protestant={}}')
            (game / 'poptypes/farmers.txt').write_text('')
            actual, report = subject.rebel_save(raw, game)
            expected = raw.replace(b'546={owner=PRU controller=PRU', b'546={owner=PRU controller="REB"')
            expected = expected.replace(b'core=D45', b'')
            expected = expected.replace(b'size=123.5 north_german=protestant', b'size=123.5 aborigine=animist')
            expected = expected.replace(b'army={location=546', b'army={location=549')
            self.assertEqual(actual, expected)
            self.assertEqual(report['pop_count'], 1)
            self.assertEqual(report['removed_core_provinces'], [546, 549])
            self.assertEqual(report['army_relocation']['moved_army_count'], 1)
            with self.assertRaisesRegex(ValueError, 'уже существует'):
                subject.rebel_save(raw.replace(b'549={owner=PRU', b'549={owner=D45'), game)


class CoreReaderTests(unittest.TestCase):
    def setUp(self):
        self.memory = {}
        self.base, self.world, self.manager, self.table, self.province = 0x1000000, 0x1000, 0x2000, 0x3000, 0x5000
        self.put(self.base + 0xe588e8, self.world)
        self.put(self.base + 0xe587e4, self.manager)
        self.put(self.manager, 4, self.table)
        self.put(self.world + 0xacc, 0x4000, 0x400c)
        self.put(0x4008, self.province)
        self.put(self.province + 0x58, 2)
        self.put(self.world + 0xb0c, 777)
        for index, tag in enumerate(('PRU', 'REB', 'D45'), 1):
            pointer = 0x6000 + index * 0x100
            self.put(self.table + index * 4, pointer)
            self.put(pointer + 0x1c, self.tag(tag), index)
        self.put(self.province + 0x128, self.tag('PRU'), 1, self.tag('REB'), 2)
        self.put(self.province + 0x148, 0x8000, 0x8010)
        self.put(0x8000, self.tag('PRU'), 1, self.tag('D45'), 3)

    @staticmethod
    def tag(value): return int.from_bytes(value.encode() + b'\0', 'little')
    def put(self, address, *values):
        raw = struct.pack('<' + 'I' * len(values), *values)
        self.memory.update({address + index: value for index, value in enumerate(raw)})
    def read(self, address, size): return bytes(self.memory[address + index] for index in range(size))

    def test_eight_byte_core_identities_are_resolved_against_country_table(self):
        self.assertEqual(subject.inspect_cores(self.read, self.base, 2),
                         dict(province=2, owner='PRU', controller='REB', cores=['D45', 'PRU'], date_raw=777))

    def test_wrong_identity_and_misaligned_vector_rejected(self):
        self.put(0x800c, 2)
        with self.assertRaisesRegex(ValueError, 'Identity'): subject.inspect_cores(self.read, self.base, 2)
        self.put(self.province + 0x14c, 0x800c)
        with self.assertRaisesRegex(ValueError, 'vector'): subject.inspect_cores(self.read, self.base, 2)


class ProtocolTests(unittest.TestCase):
    def driver(self):
        driver = object.__new__(subject.EventDriver)
        driver.host, driver.client = Path('host'), Path('client')
        driver.advancing, driver.moving_requested, driver.handling_events = False, False, False
        driver.selected = set(); driver.record = {'option_selections': []}
        driver.save = Mock()
        return driver

    @staticmethod
    def event(number, country, scope, province):
        return dict(event_id=number, country=country, scope=scope, province=province, options=1)

    def test_both_events_use_their_own_native_mp_queue(self):
        driver = self.driver()
        a = self.event(24001, 'PRU', 'province', 546)
        b = self.event(3997400, 'D50', 'country', 0)
        driver.wait = Mock(side_effect=[{'events': [a]}, {'events': []}, {'events': [b]}, {'events': []}])
        driver.send = Mock(side_effect=[dict(ok=True, queued=True, option=0, **a), dict(ok=True, queued=True, option=0, **b)])
        self.assertTrue(driver.drain())
        self.assertEqual([call.args for call in driver.send.call_args_list],
                         [(Path('host'), 'mp-option 24001 0'), (Path('client'), 'mp-option 3997400 0')])
        self.assertEqual(driver.selected, {24001, 3997400})

    def test_unknown_outcome_stops_without_second_option(self):
        driver = self.driver()
        driver.wait = Mock(return_value={'events': [self.event(24001, 'PRU', 'province', 546)]})
        driver.send = Mock(side_effect=RuntimeError('outcome_unknown'))
        with self.assertRaisesRegex(RuntimeError, 'outcome_unknown'): driver.drain()
        self.assertEqual(driver.send.call_count, 1)
        self.assertFalse(driver.handling_events)
        self.assertEqual(driver.selected, set())

    def test_wrong_scope_is_rejected_before_mutation(self):
        driver = self.driver()
        driver.wait = Mock(return_value={'events': [self.event(24001, 'PRU', 'country', 0)]})
        driver.send = Mock()
        with self.assertRaisesRegex(ValueError, 'scope'): driver.drain()
        driver.send.assert_not_called()

    def test_event_handling_during_advance_only_after_unpause_was_sent(self):
        driver = self.driver(); driver.advancing = True
        driver.drain = Mock(return_value=True); driver.pause = Mock()
        with patch.object(subject.Driver, 'wait', side_effect=lambda env, wire, predicate: predicate({'paused': True})):
            subject.EventDriver.wait(driver, driver.host, 'mp-state', lambda state: False)
            driver.drain.assert_not_called()
            driver.moving_requested = True
            subject.EventDriver.wait(driver, driver.host, 'mp-state', lambda state: False)
            driver.drain.assert_called_once()
            driver.pause.assert_called_once_with(False)

    def test_result_requires_both_pairs_core_and_equal_date(self):
        result = dict(date_raw=777, countries=copy.deepcopy(subject.EXPECTED),
                      province=dict(date_raw=777, province=546, owner='PRU', controller='REB', cores=['D45']))
        subject.check_results(result, copy.deepcopy(result), 777)
        other = copy.deepcopy(result); other['countries']['D50']['religion'] = 'protestant'
        with self.assertRaisesRegex(ValueError, 'культура/религия'): subject.check_results(result, other, 777)
        other = copy.deepcopy(result); other['province']['cores'] = []
        with self.assertRaisesRegex(ValueError, 'корки'): subject.check_results(result, other, 777)
        with self.assertRaisesRegex(ValueError, 'дата'): subject.check_results(result, result, 778)

    def test_second_startup_failure_stops_both_owned_processes(self):
        with tempfile.TemporaryDirectory(dir=subject.engine.ROOT / '.work') as directory:
            root = Path(directory)
            host, client = root / 'host', root / 'client'
            host.mkdir(); client.mkdir()
            processes = [Mock(), Mock()]
            driver = Mock(); driver.record = {}

            def start(environment, hidden):
                session = dict(token=('1' if environment == host else '2') * 32,
                               pid=101 if environment == host else 102, game=str(environment / 'game'))
                (environment / 'session.json').write_text(json.dumps(session), encoding='utf-8')
                return dict(ok=True, **session)

            with patch.object(subject, 'EventDriver', return_value=driver), \
                    patch.object(subject, 'verify_package', return_value=({}, 'a' * 64)), \
                    patch.object(subject, 'compare', return_value={'ok': True}), \
                    patch.object(subject.engine.cli, 'start', side_effect=start), \
                    patch.object(subject.engine, 'OwnedProcess', side_effect=processes), \
                    patch.object(subject, 'wait_initialisation', side_effect=[None, TimeoutError('initialisation')]):
                record = subject.run(host, client, root / 'package', root / 'report.json')
            self.assertFalse(record['ok'])
            self.assertFalse(record['complete'])
            driver.bootstrap.assert_not_called()
            for process in processes: process.stop.assert_called_once()
            self.assertEqual(record['cleanup'], {'client': {'stopped': True}, 'host': {'stopped': True}})
            self.assertEqual(record['errors'][0]['message'], 'initialisation')


if __name__ == '__main__':
    unittest.main()
