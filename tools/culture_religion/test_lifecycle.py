"""Подготовка и порядок дополнительных сценариев; игры и нативные вызовы отсутствуют."""
import copy
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from tools.culture_religion import lifecycle as subject


def event(number, tag, culture, religion):
    condition = f'controlled_by_rebels=yes culture={culture} pop_majority_religion={religion} owner={{{tag}={{exists=no}}}}'
    assignment = f'owner={{{tag}={{primary_culture={culture} religion={religion}}}}}' if tag == 'D45' else ''
    return (f'province_event={{id={number} trigger={{{condition}}} option={{name=ok '
            f'if={{limit={{{condition}}} {assignment} add_core={tag}}}}}}}')


FIRST = ''.join(event(*values) for values in ((24207, 'PHI', 'filipino', 'catholic'),
                                             (24533, 'PAN', 'panjabi', 'sikh'),
                                             (24001, 'D45', 'aborigine', 'animist'),
                                             (24368, 'D45', 'malay', 'sunni'))).encode()
REMOVE = b'country_event={id=145000 option={any_country={limit={tag=D45 exists=no} all_core={remove_core=D45} primary_culture=british}}}'
FORM = b'country_event={id=6119 trigger={D45={exists=no} any_owned_province={is_core=D45}} option={release=D45}}'
PRIMARY = (b'political_decisions={D_primary_aborigine={'
           b'potential={primary_culture=british capital_scope={culture=aborigine} OR={tag=D45}} '
           b'allow={capital_scope={culture=aborigine}} '
           b'effect={primary_culture=aborigine set_religion_from_primary_culture=yes any_pop={militancy=-10}}}}')


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.game = Path(self.directory.name)
        (self.game / 'common').mkdir(); (self.game / 'poptypes').mkdir()
        (self.game / 'common/cultures.txt').write_text('x={filipino={} panjabi={} aborigine={} malay={} british={} north_german={}}')
        (self.game / 'common/religion.txt').write_text('x={catholic={} sikh={} animist={} sunni={} protestant={}}')
        (self.game / 'poptypes/farmers.txt').write_text('')
        self.raw = (b'player=PRU date="1836.1.1"\r\n'
                    b'546={owner=PRU controller=PRU life_rating=40 core=D45 farmers={size=42.75 north_german=protestant cash=10}}\r\n'
                    b'547={owner=PRU controller=PRU life_rating=40 farmers={size=100 north_german=catholic}}\r\n'
                    b'549={owner=PRU controller=PRU life_rating=40}\r\n'
                    b'700={owner=PAN controller=PAN core=PAN farmers={size=50 panjabi=sikh}}\r\n'
                    b'PRU={capital=549 primary_culture=north_german religion=protestant}\r\n'
                    b'PHI={primary_culture=filipino religion=catholic culture={}}\r\n'
                    b'PAN={primary_culture=panjabi religion=sikh culture={}}\r\n'
                    b'D45={primary_culture=british religion=protestant culture={irish}}\r\n}\r\n')

    def test_selects_exact_pairs_and_rejects_divergent_guard(self):
        selected = subject.events(FIRST)
        self.assertEqual({key: value['id'] for key, value in selected.items()},
                         {'phi': 24207, 'pan': 24533, 'a': 24001, 'b': 24368})
        with self.assertRaisesRegex(ValueError, 'Guard'):
            subject.events(FIRST.replace(b'limit={controlled_by_rebels=yes', b'limit={controlled_by_rebels=no', 1))
        with self.assertRaisesRegex(ValueError, 'Повторный'):
            subject.events(FIRST + event(1, 'D45', 'malay', 'sunni').encode())

    def test_native_reset_contract_and_real_formation_are_required(self):
        parsed = subject.country_events(REMOVE, FORM)
        self.assertEqual(parsed['formation']['id'], 6119)
        self.assertEqual(parsed['removing']['id'], 145000)
        with self.assertRaises(ValueError): subject.country_events(REMOVE.replace(b'primary_culture=british', b'religion=animist primary_culture=british'), FORM)
        with self.assertRaises(ValueError): subject.country_events(REMOVE, FORM.replace(b'release=D45', b'release_vassal=D45'))

    def test_native_primary_decision_requires_original_conditions_and_effect_order(self):
        parsed = subject.primary_decision(PRIMARY)
        self.assertEqual(parsed['key'], 'D_primary_aborigine')
        with self.assertRaisesRegex(ValueError, 'порядок'):
            subject.primary_decision(PRIMARY.replace(
                b'primary_culture=aborigine set_religion_from_primary_culture=yes',
                b'set_religion_from_primary_culture=yes primary_culture=aborigine'))
        with self.assertRaisesRegex(ValueError, 'условия'):
            subject.primary_decision(PRIMARY.replace(b'primary_culture=british', b'primary_culture=malay'))
        with self.assertRaisesRegex(ValueError, 'единственного'):
            subject.primary_decision(PRIMARY + PRIMARY)

    def test_minimal_save_changes_remove_pan_and_preserve_population_sizes(self):
        self.assertEqual(subject.select_provinces(self.raw, self.game), (546, 547))
        modified, details = subject.fixture_save(self.raw, self.game, 'PAN', {546: ('panjabi', 'sikh')})
        expected = self.raw.replace(b'546={owner=PRU controller=PRU', b'546={owner=PRU controller="REB"')
        expected = expected.replace(b'size=42.75 north_german=protestant', b'size=42.75 panjabi=sikh')
        expected = expected.replace(b'700={owner=PAN controller=PAN core=PAN', b'700={owner="PRU" controller="PRU" ')
        self.assertEqual(modified, expected)
        self.assertEqual(details['transferred'], [700])
        self.assertEqual(details['removed_core_provinces'], [700])
        self.assertEqual(subject.state(modified, 'PAN')['country']['religion'], 'sikh')

    def test_two_province_dynamic_fixture_does_not_change_country_pair(self):
        modified, _ = subject.fixture_save(self.raw, self.game, 'D45', {546: ('aborigine', 'animist'), 547: ('malay', 'sunni')})
        self.assertIn(b'size=42.75 aborigine=animist cash=10', modified)
        self.assertIn(b'size=100 malay=sunni', modified)
        state = subject.state(modified, 'D45')
        self.assertEqual(state['country'], dict(primary='british', religion='protestant', accepted=['irish']))
        self.assertFalse(state['cores']); self.assertFalse(state['owned'])

    def test_stale_mutator_checks_original_allow_and_uses_direct_effects(self):
        selected = subject.events(FIRST)
        files = subject.controls(selected, dict(a=546, b=547), subject.country_events(REMOVE, FORM)['formation'])
        text = files[subject.DECISIONS].decode('cp1251')
        root = next(subject.old.Spans(text).roots())
        nodes = {node.key: node for node in root.value}
        form = subject.old.one(nodes['cr_life_form_check'], 'allow')
        self.assertEqual([node.key for node in form.value], ['D45', 'any_owned_province'])
        claim = nodes['cr_life_claim_a']
        allow = subject.old.one(subject.old.one(claim, 'allow'), '546')
        self.assertEqual(text[allow.start:allow.end], selected['a']['trigger'])
        effect = subject.old.one(claim, 'effect')
        self.assertEqual([node.key for node in effect.value], ['546', 'D45', 'set_country_flag'])
        province = subject.old.one(effect, '546')
        self.assertEqual([(node.key, node.value) for node in province.value], [('add_core', 'D45')])
        country = subject.old.one(effect, 'D45')
        self.assertEqual([(node.key, node.value) for node in country.value],
                         [('primary_culture', 'aborigine'), ('religion', 'animist')])
        self.assertEqual(subject.old.scalar(effect, 'set_country_flag'), 'cr_life_claimed')
        british = subject.old.one(nodes['cr_life_british_core'], 'effect')
        self.assertEqual([node.key for node in british.value], ['546', 'set_country_flag'])
        self.assertEqual([(node.key, node.value) for node in subject.old.one(british, '546').value], [('add_core', 'D45')])
        self.assertNotIn('chr_barrier_check', nodes)

    def test_player_copy_changes_only_header_and_never_overwrites(self):
        for quoted in (False, True):
            with self.subTest(quoted=quoted):
                raw = self.raw.replace(b'player=PRU', b'player="PRU"') if quoted else self.raw
                source = self.game / ('native' + str(quoted) + '.v2')
                target = self.game / ('player' + str(quoted) + '.v2')
                source.write_bytes(raw)
                result = subject.switch_player_copy(source, target)
                self.assertEqual(source.read_bytes(), raw)
                expected = raw.replace(b'player="PRU"' if quoted else b'player=PRU',
                                       b'player="D45"' if quoted else b'player=D45', 1)
                self.assertEqual(target.read_bytes(), expected)
                self.assertEqual(result['copy_sha256'], subject.sha(expected))
                target.write_bytes(b'existing')
                with self.assertRaises(FileExistsError): subject.switch_player_copy(source, target)
                self.assertEqual(target.read_bytes(), b'existing')
                self.assertEqual(source.read_bytes(), raw)

    def test_player_copy_rejects_ambiguous_player_and_other_directory(self):
        source, target = self.game / 'native.v2', self.game / 'player.v2'
        source.write_bytes(b'player=PRU\r\n' + self.raw)
        with self.assertRaisesRegex(ValueError, 'единственного'): subject.switch_player_copy(source, target)
        self.assertFalse(target.exists())
        source.write_bytes(self.raw.replace(b'player=PRU', b'player=D45'))
        with self.assertRaisesRegex(ValueError, 'единственного'): subject.switch_player_copy(source, target)
        source.write_bytes(self.raw)
        with self.assertRaisesRegex(ValueError, 'соседним'): subject.switch_player_copy(source, self.game / 'other/player.v2')

    def test_formed_population_checks_actual_capital_owner_pair_and_positive_size(self):
        raw = (b'546={owner=D45 farmers={size=42.75 aborigine=animist}} '
               b'D45={capital=546 primary_culture=british religion=protestant}')
        result = subject.formation_population(raw, self.game, 546)
        self.assertEqual(result['total'], '42.75')
        for before, after in ((b'capital=546', b'capital=549'), (b'owner=D45', b'owner=PRU'),
                              (b'aborigine=animist', b'aborigine=protestant'), (b'size=42.75', b'size=0')):
            with self.subTest(after=after), self.assertRaises(ValueError):
                subject.formation_population(raw.replace(before, after), self.game, 546)

    def test_state_rejects_duplicate_core_and_pair_assertion_checks_accepted(self):
        with self.assertRaisesRegex(ValueError, 'Повторная корка'):
            subject.state(self.raw.replace(b'core=D45', b'core=D45 core=D45'), 'D45')
        observed = subject.state(self.raw, 'D45')
        with self.assertRaisesRegex(ValueError, 'признанные'):
            subject.expect_pair(observed, dict(culture='british', religion='protestant'), [], [546])


def observed(culture='british', religion='protestant', cores=(), owned=()):
    return dict(country=dict(primary=culture, religion=religion, accepted=[]), cores=list(cores),
                owned=list(owned), all_cores={(int(number), 'D45') for number in cores} | {(999, 'PRU')})


class ProtocolTests(unittest.TestCase):
    def runner(self, states):
        runner = object.__new__(subject.Runner)
        runner.observe = Mock(side_effect=[(copy.deepcopy(value), Path('observed.v2')) for value in states])
        runner.decision = Mock(); runner.request = Mock(); runner.wait = Mock(); runner.action = Mock()
        runner.fire = Mock(); runner.settle = Mock(); runner.load = Mock(); runner.country_event = Mock()
        runner.wait_pair = Mock(); runner.checkpoint = Mock()
        return runner

    def manifest(self):
        native = subject.country_events(REMOVE, FORM)
        native['primary_decision'] = subject.primary_decision(PRIMARY)
        return dict(events=subject.events(FIRST), provinces=dict(a=546, b=547), native=native)

    def test_stale_open_before_competing_effect_then_selection_preserves_pair_and_cores(self):
        initial = observed(); claimed = observed('aborigine', 'animist', [546])
        runner = self.runner([initial, claimed, claimed]); record = {}
        calls = Mock()
        for key in ('request', 'action', 'wait'): calls.attach_mock(getattr(runner, key), key)
        runner.stale(self.manifest(), record, 'test')
        self.assertEqual([(call[0], call[1]) for call in calls.mock_calls], [
            ('request', ('event-fire-province-chosen 547 24368',)),
            ('action', ('cr_life_claim_a', 'cr_life_claimed')),
            ('wait', ('event-select-province 547 0',))])
        self.assertTrue(record['one_open_event_with_test_competitor_mutation'])
        runner = self.runner([initial, claimed, observed('malay', 'sunni', [546])])
        with self.assertRaisesRegex(ValueError, 'Устаревшее'): runner.stale(self.manifest(), {}, 'test')

    def test_mutator_without_core_stops_before_selecting_real_b(self):
        runner = self.runner([observed(), observed('aborigine', 'animist')])
        with self.assertRaisesRegex(ValueError, 'корки'):
            runner.stale(self.manifest(), {}, 'test')
        runner.wait.assert_not_called()
        runner.settle.assert_not_called()

    def test_formation_requires_reload_before_and_after_native_release(self):
        initial = observed(); first = observed('aborigine', 'animist', [546]); formed = observed('aborigine', 'animist', [546], [546])
        runner = self.runner([initial, first, first, formed, formed]); record = {}
        runner.lifecycle(dict(name='formation'), self.manifest(), record, 'test')
        runner.fire.assert_called_once_with(24001, 546)
        runner.country_event.assert_called_once_with(6119)
        self.assertEqual(runner.load.call_count, 2)
        self.assertTrue(record['first_reload_formation_reload'])

    def test_reuse_requires_existence_protection_annex_reset_and_different_religion(self):
        initial = observed(); first = observed('aborigine', 'animist', [546]); formed = observed('aborigine', 'animist', [546], [546])
        reset = observed('british', 'animist'); reused = observed('malay', 'sunni', [547])
        runner = self.runner([initial, first, formed, formed, first, reset, reused]); record = {}
        runner.lifecycle(dict(name='reuse'), self.manifest(), record, 'test')
        self.assertEqual([call.args for call in runner.country_event.call_args_list], [(6119,), (145000,), (145000,)])
        runner.action.assert_called_once_with('cr_life_annex', 'cr_life_annexed')
        self.assertEqual([call.args for call in runner.fire.call_args_list], [(24001, 546), (24368, 547)])
        self.assertTrue(record['exists_protected_then_annex_reset_reuse'])

    def test_reset_changing_religion_fails_before_reuse(self):
        first = observed('aborigine', 'animist', [546]); formed = observed('aborigine', 'animist', [546], [546])
        runner = self.runner([observed(), first, formed, formed, first, observed('british', 'protestant')])
        with self.assertRaisesRegex(ValueError, 'религия'):
            runner.lifecycle(dict(name='reuse'), self.manifest(), {}, 'test')
        runner.fire.assert_called_once_with(24001, 546)

    def test_british_formation_switches_native_save_and_executes_real_decision_once(self):
        initial = observed(); seeded = observed(cores=[546]); formed = observed(cores=[546], owned=[546])
        changed = observed('aborigine', 'animist', [546], [546])
        runner = self.runner([initial, seeded, formed, formed, changed, changed])
        with tempfile.TemporaryDirectory() as directory:
            runner.environment = Path(directory).resolve()
            saves = runner.environment / 'userdata/save games'; saves.mkdir(parents=True)
            native = saves / 'native.v2'
            native.write_bytes(b'player="PRU" D45={capital=546 primary_culture=british religion=protestant}')
            runner.observe.side_effect = [(copy.deepcopy(value), native) for value in
                                         [initial, seeded, formed, formed, changed, changed]]
            runner.request.return_value = dict(potential=True, allow=True)
            record = {}
            with patch.object(subject, 'formation_population', return_value=dict(capital=546, total='42')) as population:
                runner.british_decision(self.manifest(), record, 'test')
            population.assert_called_once()
            runner.action.assert_called_once_with('cr_life_british_core', 'cr_life_british_core_done')
            runner.country_event.assert_called_once_with(6119)
            self.assertEqual([call.args for call in runner.load.call_args_list], [('test_d45.v2',), ('native.v2',)])
            self.assertEqual([call.args for call in runner.request.call_args_list],
                             [('decision-check D45 D_primary_aborigine',), ('decision-execute D45 D_primary_aborigine',)])
            self.assertEqual([call.args[0] for call in runner.wait_pair.call_args_list],
                             [dict(culture='british', religion='protestant'), self.manifest()['native']['primary_decision'],
                              self.manifest()['native']['primary_decision']])
            self.assertEqual((saves / 'test_d45.v2').read_bytes(), native.read_bytes().replace(b'player="PRU"', b'player="D45"'))
            self.assertTrue(record['native_british_formation_primary_decision_reload'])
            runner.fire.assert_not_called()

    def test_british_formation_changed_pair_aborts_before_decision(self):
        runner = self.runner([observed(), observed(cores=[546]), observed('aborigine', 'animist', [546], [546])])
        with self.assertRaisesRegex(ValueError, 'культура'):
            runner.british_decision(self.manifest(), {}, 'test')
        runner.request.assert_not_called()
        runner.load.assert_not_called()

    def test_pair_poll_requires_both_fields_and_does_not_execute_mutations(self):
        runner = object.__new__(subject.Runner)
        runner.environment, runner.owned = Path('environment'), object()
        runner.record = dict(session={}); runner.checkpoint = Mock(); runner.request = Mock()
        partial = dict(countries={'D45': dict(primary='aborigine', religion='protestant')})
        complete = dict(countries={'D45': dict(primary='aborigine', religion='animist')})
        record = {}
        with patch.object(subject, 'capture', side_effect=[partial, complete]) as capture, patch.object(subject.time, 'sleep'):
            runner.wait_pair(dict(culture='aborigine', religion='animist'), record)
        self.assertEqual(capture.call_count, 2)
        self.assertEqual(record['runtime_pairs'], [dict(attempts=2, snapshot=complete)])
        runner.request.assert_not_called()


if __name__ == '__main__':
    unittest.main()
