"""Регрессии порога, сохранности POP и отказов перед исторической миграцией."""
from decimal import Decimal
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from apply_history import assignments, lab, masked_pops, rewrite, russian_names, selected


class SelectionTests(unittest.TestCase):
    def test_exact_quarter_is_excluded(self):
        self.assertFalse(selected('african_minor', Decimal('15450.0'), Decimal('61800.00')))
        self.assertFalse(selected('african_minor', Decimal('5150'), Decimal('20600.000000000000001')))
        self.assertTrue(selected('african_minor', Decimal('5150.000000000000001'), Decimal('20600')))

    def test_australia_ignores_threshold_but_not_zero(self):
        self.assertTrue(selected('aborigine', Decimal('1'), Decimal('100000')))
        self.assertFalse(selected('aborigine', Decimal('0'), Decimal('100000')))
        self.assertFalse(selected('british', Decimal('99999'), Decimal('100000')))


class RewriteTests(unittest.TestCase):
    def setUp(self):
        self.rules = {('asian_minor', 42): dict(culture='gond', source='india1901', region='Индия')}

    def test_changes_only_culture_and_preserves_fractional_other_fields(self):
        raw = b'# marker\r\r\n42 = { farmers = { culture = "asian_minor" religion = animist size = 25.000000000001 money = 7.25 literacy = 0.5 }\r\r\n artisans = { culture = british religion = catholic size = 75 } }\r\r\n'
        result, records, composition = rewrite(raw, self.rules)
        self.assertEqual(result, raw.replace(b'"asian_minor"', b'"gond"'))
        self.assertEqual(masked_pops(raw), masked_pops(result))
        self.assertEqual(records[0]['size'], '25.000000000001')
        self.assertEqual(composition[42]['gond'], Decimal('25.000000000001'))

    def test_multiple_pop_blocks_share_whole_province_threshold(self):
        raw = b'42={farmers={culture=asian_minor religion=animist size=13} soldiers={culture=asian_minor religion=animist size=13} farmers={culture=british religion=protestant size=74}}'
        result, records, _ = rewrite(raw, self.rules)
        self.assertEqual(result.count(b'culture=gond'), 2)
        self.assertEqual(records[0]['pop_blocks'], 2)
        exact = raw.replace(b'size=74', b'size=78')
        self.assertEqual(rewrite(exact, self.rules)[0], exact)

    def test_untargeted_changes_are_detectable(self):
        raw = b'42={farmers={culture=asian_minor religion=animist size=100}}'
        result = rewrite(raw, self.rules)[0]
        self.assertNotEqual(masked_pops(raw), masked_pops(result.replace(b'100', b'101')))
        self.assertNotEqual(masked_pops(raw), masked_pops(result.replace(b'animist', b'catholic')))

    def test_unknown_target_and_duplicate_target_fail(self):
        raw = b'42={farmers={culture=asian_minor religion=animist size=100}}'
        with self.assertRaises(ValueError): rewrite(raw, {})
        with self.assertRaises(ValueError): rewrite(raw+raw, self.rules)

    def test_non_target_legacy_duplicate_is_preserved(self):
        raw = b'2482={farmers={culture=british religion=protestant size=5}}2482={farmers={culture=irish religion=catholic size=7}}'
        result, records, comp = rewrite(raw,{})
        self.assertEqual(raw,result)
        self.assertEqual(records,[])
        self.assertEqual(sum(comp[2482].values()),12)


class DataTests(unittest.TestCase):
    def test_inherited_mojibake_names_are_translated(self):
        self.assertEqual(russian_names('{ Оцелотl Gonzбlez "Gutiйrrez" }'), '{ Оцелотль Гонсалес "Гутьеррес" }')
        with self.assertRaises(ValueError): russian_names('{ Unknown }')

    def test_registry_covers_expected_unique_provinces(self):
        result = assignments()
        self.assertEqual(len(result),363)
        self.assertEqual(sum(old=='aborigine' for old,_ in result),44)

    def test_lab_reference_white_black(self):
        white,black=lab((255,255,255)),lab((0,0,0))
        self.assertAlmostEqual(white[0],100,places=4)
        self.assertAlmostEqual(white[1],0,places=3)
        self.assertAlmostEqual(white[2],0,places=3)
        self.assertEqual(black,(0.0,0.0,0.0))


if __name__=='__main__':
    unittest.main()
