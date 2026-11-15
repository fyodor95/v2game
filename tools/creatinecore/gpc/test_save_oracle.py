"""Проверка независимого oracle без запуска игры и изменения исходных сейвов."""
from pathlib import Path
import tempfile
import unittest

from save_oracle import Block, Parser, preview


class OracleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for folder in ['common', 'poptypes', 'technologies']:
            (self.root / folder).mkdir()
        self.write('common/cultures.txt', 'g={a={color={1 2 3}} b={} c={}}')
        self.write('common/religion.txt', 'g={r={icon=1}}')
        self.write('poptypes/farmers.txt', 'color={1 2 3}')
        self.write('poptypes/labourers.txt', 'color={3 2 1}')
        self.write('technologies/culture.txt', 'nationalism_n_imperialism={year=1850}')

    def write(self, name, text):
        path = self.root / name
        path.write_bytes(text.encode('cp1251'))
        return path

    def scenario(self, extra='', tech=True):
        researched = 'nationalism_n_imperialism={1 0.000}' if tech else ''
        return self.write('scenario.v2', '''date="1891.1.1"
AAA={primary_culture="a" culture={"a" "b" "b"} technology={%s}}
BBB={primary_culture="c" culture={"b"} technology={nationalism_n_imperialism={1 0}}}
CCC={primary_culture="c" technology={}}
1={name="Проверка ё" owner="CCC" core="CCC" core="CCC"
 farmers={size=20 a=r} farmers={size=35 b=r}
 labourers={size=20 a=r} farmers={size=25 c=r}}
2={owner="CCC" farmers={size=40 c=r} farmers={size=30 a=r} farmers={size=30 b=r}}
3={owner="AAA" farmers={size=100 a=r}}
4={owner="CCC" core="AAA" farmers={size=100 b=r}}
5={farmers={size=100 a=r}}
6={owner="BBB" farmers={size=0 a=r}}
7={owner="CCC" farmers={size=100 b=r}}
%s
''' % (researched, extra))

    def test_repeated_and_list(self):
        block = list(Parser('a={core=A core=B names={"x" "y"} pop={size=1} pop={size=2} employees={{id=1}{id=2}}}').entries())[0].value
        self.assertIsInstance(block, Block)
        self.assertEqual(block.values('core'), ['A', 'B'])
        self.assertEqual(block.single('names').atoms(), ['x', 'y'])
        self.assertEqual(len(block.values('pop')), 2)
        self.assertEqual([entry.value.single('id') for entry in block.single('employees').items], ['1', '2'])

    def test_exact_pairs(self):
        result = preview(self.scenario(), self.root, ['BBB', 'AAA'])
        self.assertTrue(result['complete'])
        self.assertEqual(result['additions'], [
            {'province': 1, 'country': 'AAA'},
            {'province': 2, 'country': 'BBB'},
            {'province': 4, 'country': 'BBB'},
            {'province': 7, 'country': 'AAA'},
            {'province': 7, 'country': 'BBB'}])
        self.assertEqual(result['provinces'][0]['population'], {'a': 40, 'b': 35, 'c': 25})
        self.assertEqual(result['provinces'][0]['cores'], ['CCC'])

    def test_technology_and_absence(self):
        save = self.scenario(tech=False)
        result = preview(save, self.root, ['AAA'])
        self.assertEqual(result['additions'], [])
        self.write('technologies/culture.txt', 'other_technology={year=1850}')
        result = preview(save, self.root, ['AAA'])
        self.assertFalse(result['technology_present'])
        self.assertEqual([p['province'] for p in result['additions']], [1, 7])

    def test_tie_is_incomplete(self):
        result = preview(self.scenario('8={owner="CCC" farmers={size=50 a=r} farmers={size=50 b=r}}'),
                         self.root, ['AAA'])
        self.assertFalse(result['complete'])
        self.assertEqual(result['needs_qualification'], [{'province': 8, 'cultures': ['a', 'b']}])
        self.assertNotIn(8, [p['province'] for p in result['additions']])

    def test_unknown_gp(self):
        with self.assertRaises(ValueError):
            preview(self.scenario(), self.root, ['ZZZ'])

    def test_malformed(self):
        for text in ('a={', 'a="broken', 'a=}', '}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                list(Parser(text).entries())

    def test_native_save_implicit_root_close(self):
        save = self.scenario()
        save.write_bytes(save.read_bytes() + b'}\r\n')
        self.assertTrue(preview(save, self.root, ['AAA'])['complete'])
        with self.assertRaises(ValueError):
            list(Parser('a=1 } b=2', implicit_root=True).entries())

    def test_bad_pop_rejected(self):
        for content in ('size=-1 a=r', 'size=1.5 a=r', 'size=1 unknown=r', 'size=1 a=r b=r'):
            with self.subTest(pop=content), self.assertRaises(ValueError):
                preview(self.scenario('8={owner="CCC" farmers={%s}}' % content), self.root, ['AAA'])

    def test_extinct_gp_ineligible(self):
        save = self.scenario('DDD={primary_culture="a" technology={nationalism_n_imperialism={1 0}}}')
        result = preview(save, self.root, ['DDD'])
        self.assertFalse(result['countries']['DDD']['eligible'])
        self.assertEqual(result['additions'], [])


if __name__ == '__main__':
    unittest.main()
