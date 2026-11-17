"""Регрессии точечных замен сейвов; игра и нативный мост не запускаются."""
from collections import Counter
from decimal import Decimal
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.culture_religion import decision_fixture as subject
from tools.chronology_import.engine_acceptance import Spans, scalar


class DecisionFixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / 'game'
        (self.game / 'common').mkdir(parents=True)
        (self.game / 'poptypes').mkdir()
        (self.game / 'common/cultures.txt').write_text(
            'germanic={north_german={} south_german={}} british_group={british={}} other={french={}}')
        (self.game / 'common/religion.txt').write_text('christian={catholic={} protestant={}} other={jewish={}}')
        (self.game / 'poptypes/farmers.txt').write_text('')
        (self.game / 'poptypes/clerks.txt').write_text('')
        # Защищённые 549/546 намеренно меньше и больше кандидатов соответственно.
        self.raw = ('''player="PRU"
date="1836.1.1"
546={owner="PRU" controller="PRU" life_rating=40 core="PRU" name="Защищённая"
 farmers={size=1000000 north_german=protestant literacy=0.7}}
549={owner="PRU" controller="PRU" life_rating=40 core="PRU"
 farmers={size=1 north_german=protestant}}
1={owner="PRU" controller="PRU" life_rating=40 core="PRU" core="D50" fort=2
 farmers={size=10.5 french=catholic literacy=0.3}}
2={owner="PRU" controller="PRU" life_rating=40 core="GER"
 farmers={size=20 north_german=protestant literacy=0.4}
 clerks={size=10.75 french=jewish literacy=0.8}}
3={owner="AUS" controller="AUS" life_rating=40 core="D50" core="AUS"
 farmers={size=1000000 north_german=protestant}}
4={owner="PRU" controller="REB" life_rating=40 core="PRU"
 farmers={size=2000000 south_german=catholic}}
5={owner="PRU" controller="PRU" core="D50"}
PRU={primary_culture="north_german" religion="protestant" capital=549 money=123
 army={name="Первая армия" location=2}}
D50={primary_culture="british" religion="protestant" culture={"french"} money=42}
}
''').replace('\n', '\r\n').encode('cp1251')

    def roots(self, raw):
        return {node.key: node for node in Spans(raw.decode('cp1251')).roots()}

    def test_dynamic_uses_owned_primary_population_not_capital_or_world(self):
        outputs, manifest = subject.transform(self.raw, self.game)
        scenario = manifest['scenarios']['cr_d_primary.v2']
        self.assertEqual(scenario['provinces'], [1, 2])
        self.assertEqual(scenario['capital'], 1)
        self.assertEqual(scenario['totals'], {'catholic': '30.75', 'protestant': '10.5'})
        self.assertEqual(scenario['expected_religion'], 'catholic')
        self.assertEqual(scenario['world_religion'], 'protestant')
        nodes = self.roots(outputs['cr_d_primary.v2'])
        self.assertEqual(nodes['player'].value, 'D50')
        self.assertEqual(scalar(nodes['D50'], 'capital'), '1')
        self.assertEqual(scalar(nodes['D50'], 'primary_culture'), 'british')
        self.assertEqual(scalar(nodes['D50'], 'religion'), 'jewish')
        self.assertEqual([field.value for field in nodes['D50'].single('culture').value], ['french'])
        for number in ('1', '2'):
            self.assertEqual(scalar(nodes[number], 'owner'), 'D50')
            self.assertEqual(scalar(nodes[number], 'controller'), 'D50')
            self.assertIn('D50', [field.value for field in nodes[number].fields('core')])
            for pop in nodes[number].value:
                if pop.key in ('farmers', 'clerks'):
                    self.assertEqual(scalar(pop, 'north_german'), 'protestant' if number == '1' else 'catholic')
        self.assertEqual([field.value for field in nodes['3'].fields('core')], ['AUS'])
        self.assertFalse(nodes['5'].fields('core'))
        for number in ('546', '549'):
            self.assertEqual(scalar(nodes[number], 'owner'), 'PRU')
        self.assertIn(b'army={name="', outputs['cr_d_primary.v2'])
        self.assertIn(b'location=2', outputs['cr_d_primary.v2'])
        self.assertIn(b'fort=2', outputs['cr_d_primary.v2'])
        self.assertIn(b'literacy=0.8', outputs['cr_d_primary.v2'])

    def test_mp_only_changes_player_relative_to_dynamic_case(self):
        outputs, manifest = subject.transform(self.raw, self.game)
        self.assertEqual(outputs['cr_d_primary.v2'].replace(b'player="D50"', b'player="PRU"', 1), outputs['cr_mp_base.v2'])
        self.assertEqual(manifest['scenarios']['cr_mp_base.v2']['player'], 'PRU')
        self.assertEqual(scalar(self.roots(outputs['cr_mp_base.v2'])['546'], 'owner'), 'PRU')

    def test_tie_has_exactly_two_primary_pops_and_preserves_other_countries(self):
        outputs, manifest = subject.transform(self.raw, self.game)
        nodes = self.roots(outputs['cr_tie.v2'])
        primary, sums = [], Counter()
        for node in nodes.values():
            if not node.key.isdigit() or scalar(node, 'owner') != 'PRU':
                continue
            for pop in node.value:
                if isinstance(pop.value, list) and pop.single('north_german') is not None:
                    primary.append(pop)
                    sums[scalar(pop, 'north_german')] += Decimal(scalar(pop, 'size'))
        self.assertEqual(len(primary), 2)
        self.assertEqual(sums, {'catholic': Decimal(100), 'protestant': Decimal(100)})
        self.assertEqual(scalar(nodes['549'].single('farmers'), 'south_german'), 'protestant')
        self.assertEqual(scalar(nodes['3'].single('farmers'), 'north_german'), 'protestant')
        self.assertEqual(scalar(nodes['3'].single('farmers'), 'size'), '1000000')
        self.assertEqual(scalar(nodes['D50'], 'religion'), 'protestant')
        self.assertEqual(scalar(nodes['PRU'], 'capital'), '549')
        self.assertEqual(manifest['scenarios']['cr_tie.v2']['expected_by_current_religion'],
                         {'jewish': 'catholic', 'protestant': 'protestant'})

    def test_existing_capital_replaced_and_fractional_sizes_preserved(self):
        raw = self.raw.replace(b'D50={primary_culture', b'D50={capital="0" primary_culture')
        outputs, _ = subject.transform(raw, self.game)
        dynamic = self.roots(outputs['cr_d_primary.v2'])['D50']
        self.assertEqual(len(dynamic.fields('capital')), 1)
        self.assertEqual(scalar(dynamic, 'capital'), '1')
        self.assertIn(b'size=10.5', outputs['cr_d_primary.v2'])
        self.assertIn(b'size=10.75', outputs['cr_d_primary.v2'])

    def test_deterministic_and_every_change_is_declared(self):
        first, manifest = subject.transform(self.raw, self.game)
        second, again = subject.transform(self.raw, self.game)
        self.assertEqual((first, manifest), (second, again))
        for name, scenario in manifest['scenarios'].items():
            text = self.raw.decode('cp1251')
            for mutation in reversed(scenario['mutations']):
                self.assertEqual(text[mutation['start']:mutation['end']], mutation['before'])
                text = text[:mutation['start']] + mutation['after'] + text[mutation['end']:]
            self.assertEqual(text.encode('cp1251'), first[name])
            self.assertEqual(scenario['output_sha256'], subject.sha(first[name]))
        self.assertEqual(manifest['source_sha256'], subject.sha(self.raw))

    def test_rejects_owned_dynamic_missing_primary_pops_and_bad_population(self):
        for before, after, message in [
            (b'owner="AUS"', b'owner="D50"', 'D50'),
            (b'north_german=protestant', b'south_german=protestant', 'двух'),
            (b'size=10.5', b'size=-1', 'численность'),
            (b'player="PRU"', b'player="AUS"', 'player'),
        ]:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                subject.transform(self.raw.replace(before, after), self.game)

    def test_prepare_keeps_source_and_refuses_existing_or_outside_output(self):
        source = self.root / 'original.v2'
        source.write_bytes(self.raw)
        work = self.root / '.work'
        output = work / 'fixtures'
        report = subject.prepare(self.game, source, output, work_root=work)
        self.assertEqual(source.read_bytes(), self.raw)
        self.assertEqual(set(path.name for path in (output / 'save games').iterdir()), set(report['scenarios']))
        self.assertTrue((output / 'manifest.json').is_file())
        with self.assertRaisesRegex(ValueError, 'существует'):
            subject.prepare(self.game, source, output, work_root=work)
        with self.assertRaisesRegex(ValueError, 'внутри'):
            subject.prepare(self.game, source, self.root / 'outside', work_root=work)

    def test_changed_source_during_preparation_blocks_output(self):
        source = self.root / 'original.v2'
        source.write_bytes(self.raw)
        output = self.root / '.work/fixtures'
        original_transform = subject.transform
        def concurrent_change(raw, game):
            result = original_transform(raw, game)
            source.write_bytes(raw + b'\r\n# external change')
            return result
        with patch.object(subject, 'transform', side_effect=concurrent_change):
            with self.assertRaisesRegex(ValueError, 'изменился'):
                subject.prepare(self.game, source, output, work_root=self.root / '.work')
        self.assertFalse(output.exists())
        self.assertTrue(source.read_bytes().endswith(b'# external change'))


if __name__ == '__main__':
    unittest.main()
