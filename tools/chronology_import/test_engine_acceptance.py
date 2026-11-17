"""Автономные проверки фикстур; игровой движок здесь не запускается."""
import tempfile
from pathlib import Path
import unittest

from tools.chronology_import import engine_acceptance as subject


def event(tag, culture, number):
    trigger = ('{ controlled_by_rebels = yes culture = ' + culture +
               ' owner = { ' + tag + ' = { exists = no NOT = { any_core = { always = yes } } } } }')
    return (f'province_event = {{ id = {number} trigger = {trigger} option = {{ '
            f'if = {{ limit = {trigger} add_core = {tag} }} }} }}\n')


class FixtureTests(unittest.TestCase):
    def test_extracts_real_ids_and_rejects_old_or_divergent_guard(self):
        raw = ''.join(event(tag, culture, number) for number, (tag, culture) in enumerate([
            ('OBO', 'north_german'), ('SAX', 'north_german'), ('GER', 'south_german'), ('D36', 'aborigine')], 24001)).encode()
        parsed = subject.find_events(raw)
        self.assertEqual(parsed['new']['id'], 24001)
        self.assertEqual(parsed['union']['culture'], 'south_german')
        self.assertEqual(parsed['dynamic']['tag'], 'D36')
        with self.assertRaises(ValueError):
            subject.find_events(b'province_event={id=24000 option={add_core=OBO}}')
        with self.assertRaises(ValueError):
            subject.find_events(raw.replace(b'limit = { controlled_by_rebels = yes',
                                            b'limit = { controlled_by_rebels = no', 1))

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        game = Path(self.temporary.name)
        (game / 'common').mkdir()
        (game / 'poptypes').mkdir()
        (game / 'common/cultures.txt').write_text('germanic={north_german={} south_german={}} other={aborigine={}}')
        (game / 'common/religion.txt').write_text('christian={protestant={}}')
        (game / 'poptypes/farmers.txt').write_text('')
        self.raw = (b'player="PRU"\r\n'
                    b'1={owner="PRU" controller="PRU" core="GER" farmers={size=100 north_german=protestant}}\r\n'
                    b'2={owner="AUS" controller="AUS" core="OBO" core="AUS"}\r\n'
                    b'3={owner="SAX" controller="SAX" core="SAX"}\r\n'
                    b'SAX={primary_culture="north_german" culture={"south_german"}}\r\n'
                    b'D36={primary_culture="czech" culture={}}\r\n}\r\n')
        self.parsed = subject.inspect_source(self.raw, game)
        self.game = game
        self.tags = {'GER', 'OBO', 'SAX', 'D36'}

    def transform(self, name, culture='north_german'):
        data, count = subject.make_save(*self.parsed, {'name': name, 'culture': culture}, self.tags)
        return data.decode('cp1251')

    def test_positive_removes_every_recipient_core_and_preserves_other_data(self):
        text = self.transform('new')
        roots = {node.key: node for node in subject.Spans(text).roots()}
        self.assertEqual(subject.scalar(roots['1'], 'controller'), 'REB')
        self.assertEqual(subject.scalar(roots['3'], 'owner'), 'PRU')
        self.assertEqual(subject.scalar(roots['3'], 'controller'), 'PRU')
        self.assertFalse(roots['1'].fields('core'))
        self.assertFalse(roots['3'].fields('core'))
        self.assertEqual([x.value for x in roots['2'].fields('core')], ['AUS'])
        self.assertIn('D36={primary_culture="czech" culture={}}', text)

    def test_exists_retains_owner_but_removes_its_cores(self):
        roots = {node.key: node for node in subject.Spans(self.transform('exists')).roots()}
        self.assertEqual(subject.scalar(roots['3'], 'owner'), 'SAX')
        self.assertFalse(roots['3'].fields('core'))

    def test_accepted_changes_primary_and_accepted_without_ownership(self):
        roots = {node.key: node for node in subject.Spans(self.transform('accepted')).roots()}
        self.assertEqual(subject.scalar(roots['SAX'], 'primary_culture'), 'south_german')
        self.assertEqual([x.value for x in roots['SAX'].single('culture').value], ['north_german'])
        self.assertEqual(subject.scalar(roots['3'], 'owner'), 'PRU')

    def test_controller_culture_dynamic_and_foreign_core(self):
        roots = {node.key: node for node in subject.Spans(self.transform('controller')).roots()}
        self.assertEqual(subject.scalar(roots['1'], 'controller'), 'PRU')
        for case, culture in [('culture', 'south_german'), ('dynamic', 'aborigine')]:
            text = self.transform(case, culture)
            roots = {node.key: node for node in subject.Spans(text).roots()}
            self.assertEqual(subject.scalar(roots['1'].single('farmers'), culture), 'protestant')
        roots = {node.key: node for node in subject.Spans(self.transform('foreign')).roots()}
        self.assertEqual(subject.scalar(roots['2'], 'owner'), 'AUS')
        self.assertEqual([x.value for x in roots['2'].fields('core')], ['AUS', 'OBO'])

    def test_case_matrix_has_ten_independent_names(self):
        events = {key: {'tag': tag, 'culture': culture} for key, tag, culture in [
            ('new', 'OBO', 'north_german'), ('old', 'SAX', 'north_german'),
            ('union', 'GER', 'south_german'), ('dynamic', 'D36', 'aborigine')]}
        cases = subject.fixture_cases(events)
        self.assertEqual(len(cases), 10)
        self.assertEqual(len({case['save'] for case in cases}), 10)
        self.assertEqual([case['name'] for case in cases if not case['allow']],
                         ['controller', 'culture', 'exists', 'foreign', 'accepted'])

    def test_diagnostic_parts_keep_nested_country_scope(self):
        full = '{ controlled_by_rebels = yes culture = north_german }'
        files, probes = subject.diagnostic_files({'province': 1, 'events': {
            'new': {'tag': 'OBO', 'culture': 'north_german', 'trigger': full}}})
        text = files['decisions/chronology_core_diagnostics.txt'].decode('cp1251')
        root = next(subject.Spans(text).roots())
        nodes = {entry.key: entry for entry in root.value}
        self.assertEqual(len(nodes), 11)
        primary = nodes[probes['new']['primary']['decision']]
        province = primary.single('allow').single('1')
        self.assertEqual(subject.scalar(province.single('owner').single('OBO'), 'primary_culture'), 'north_german')
        absent = nodes[probes['new']['not_exists']['decision']].single('allow').single('1')
        self.assertEqual(subject.scalar(absent.single('owner').single('OBO'), 'exists'), 'no')
        external = nodes[probes['new']['exists_external']['decision']].single('allow').single('1')
        self.assertEqual(subject.scalar(external.single('owner').single('NOT'), 'exists'), 'OBO')
        self.assertTrue(all(not node.single('effect').value for node in nodes.values()))

    def test_saved_diagnostic_state_distinguishes_missing_country_from_primary(self):
        path = self.game / 'sample.v2'
        path.write_bytes(self.raw)
        state = subject.saved_diagnostic_state(path, self.game, 1, 'OBO')
        self.assertFalse(state['country_block_present'])
        self.assertIsNone(state['primary_culture'])
        self.assertEqual(state['core_provinces'], [2])
        self.assertEqual(state['province']['largest_cultures'], ['north_german'])
        self.assertEqual(state['province']['population'], {'north_german': '100'})
        path.write_bytes(self.raw[:-3] + b'OBO={primary_culture="south_german" culture={"north_german"}}\r\n}\r\n')
        state = subject.saved_diagnostic_state(path, self.game, 1, 'OBO')
        self.assertTrue(state['country_block_present'])
        self.assertEqual(state['primary_culture'], 'south_german')
        self.assertEqual(state['accepted_cultures'], ['north_german'])

    def test_diagnostic_runner_saves_first_and_collects_false_without_mutation(self):
        path = self.game / 'sample.v2'
        path.write_bytes(self.raw)
        calls = []
        runner = subject.Runner.__new__(subject.Runner)
        runner.record = {'diagnostics': {'probes': {'new': {
            name: {'decision': 'probe_' + name} for name in ('full', 'primary', 'rebels')}}}}
        runner.checkpoint = lambda: None
        def save(name):
            calls.append('save ' + name)
            return path
        def request(command):
            calls.append(command)
            if command == 'status':
                return {'ok': True, 'date_raw': 100, 'paused': True}
            self.assertTrue(command.startswith('decision-check PRU '))
            return {'ok': True, 'potential': True, 'allow': command.endswith('rebels')}
        runner.save, runner.request = save, request
        record = {}
        runner.diagnostic_case({'event': 'new', 'tag': 'OBO'}, {}, {'province': 1}, self.game,
                               record, 'sample', delay=0)
        self.assertTrue(calls[0].startswith('save '))
        self.assertEqual(sum(command.startswith('save ') for command in calls), 3)
        self.assertTrue(record['complete'])
        self.assertTrue(record['observations_stable'])
        self.assertTrue(all(not sample['full_condition'] for sample in record['samples']))
        self.assertEqual(path.read_bytes(), self.raw)


if __name__ == '__main__':
    unittest.main()
