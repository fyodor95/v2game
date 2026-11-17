"""Отрицательные проверки независимого валидатора первой корки."""
import copy
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.culture_religion import validate as v
from tools.chronology_import.test_validate_core_events import FIRST, SPREAD, LOC

class ValidationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root/'events').mkdir(); (self.root/'localisation').mkdir()
        (self.root/'localisation/adding_cores.csv').write_bytes(LOC.encode('cp1251'))
        self.row = dict(culture='north_german',religion='protestant',main='SAX',union='GER',
                        dynamic_fallback=None,first=['SAX'],spread=['SAX'])
        self.model = dict(rows=[self.row],spread={('north_german','SAX')},
                          countries={'SAX':dict(religion='protestant',civilized=False)})
        self.first = re.sub(r'(?<!\w)culture = north_german', 'culture = north_german pop_majority_religion = protestant', FIRST)
        self.first = self.first.replace('exists = no primary_culture', 'country_religion = protestant exists = no primary_culture')
    def check(self, first=None, spread=SPREAD):
        (self.root/'events/adding_cores.txt').write_bytes(((self.first if first is None else first)+spread).encode('cp1251'))
        with patch.object(v,'build_model',return_value=self.model): return v.validate(self.root)
    def test_reserved_country_scope_requires_quotes(self):
        self.row.update(main='PHI',first=['PHI']);self.model['spread']={('north_german','PHI')}
        self.model['countries']={'PHI':dict(religion='protestant',civilized=False)}
        first=self.first.replace('SAX','PHI');spread=SPREAD.replace('SAX','PHI')
        self.assertFalse(self.check(first,spread)['ok'])
        self.assertTrue(self.check(first.replace('PHI = {','"PHI" = {'),spread.replace('PHI = {','"PHI" = {'))['ok'])
    def test_country_event_rejected(self):
        self.assertFalse(self.check(self.first.replace('province_event','country_event'))['ok'])
    def test_triggered_only_rejected(self):
        self.assertFalse(self.check(self.first.replace('id = 24001','is_triggered_only = yes id = 24001'))['ok'])
    def test_localization_references_checked(self):
        for name in ('EVTNAME','EVTDESC','EVTOPTA'):
            self.assertFalse(self.check(self.first.replace(name+'24001',name+'wrong'))['ok'])
    def test_valid(self): self.assertTrue(self.check()['ok'])
    def test_religion_missing(self): self.assertFalse(self.check(self.first.replace('pop_majority_religion = protestant',''))['ok'])
    def test_changed_country_religion_guard_missing(self): self.assertFalse(self.check(self.first.replace('country_religion = protestant',''))['ok'])
    def test_partial_recheck(self):
        self.assertFalse(self.check(self.first.replace('pop_majority_religion = protestant','',1))['ok'])
    def test_civilization_required(self):
        self.model['countries']['SAX']['civilized']=True
        self.assertFalse(self.check()['ok'])
        self.assertTrue(self.check(self.first.replace('owner = { NOT','owner = { civilized = yes NOT'))['ok'])
    def test_per_pair_normalization(self):
        self.row['first'].append('GER')
        result=self.check(); self.assertFalse(result['ok'])
        self.assertTrue(any('MTTH' in error for error in result['errors']))
    def test_spread_ignores_religion(self):
        self.assertFalse(self.check(spread=SPREAD.replace('controlled_by_rebels = yes','controlled_by_rebels = yes pop_majority_religion = protestant'))['ok'])
    def test_spread_dedup(self): self.assertFalse(self.check(spread=SPREAD+SPREAD.replace('24002','24003'))['ok'])
    def test_dynamic_primary_and_religion(self):
        self.row.update(main='D01',first=['D01'],dynamic_fallback='D01'); self.model['spread']={('north_german','D01')}
        first=self.first.replace('SAX','D01').replace('country_religion = protestant ','').replace('primary_culture = north_german ','')
        first=first.replace('add_core = D01','add_core = D01 owner = { D01 = { primary_culture = north_german religion = protestant } }')
        self.assertTrue(self.check(first,SPREAD.replace('SAX','D01'))['ok'])
        self.assertFalse(self.check(first.replace('primary_culture = north_german','add_accepted_culture = north_german'),SPREAD.replace('SAX','D01'))['ok'])
        self.assertFalse(self.check(first.replace('religion = protestant }','religion = catholic }'),SPREAD.replace('SAX','D01'))['ok'])

if __name__=='__main__': unittest.main()
