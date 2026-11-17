"""Фикстуры религий не запускают игру в тестах."""
from pathlib import Path
import tempfile
import unittest
from tools.culture_religion import engine
from tools.chronology_import.test_engine_acceptance import event

class EngineFixtures(unittest.TestCase):
    def events(self):
        result=''
        for i,(tag,culture,religion) in enumerate([('OBO','north_german','catholic'),('OBO','north_german','protestant'),('SAX','north_german','protestant'),('GER','south_german','catholic'),('D01','north_german','jewish')],24001):
            result+=event(tag,culture,i).replace('controlled_by_rebels = yes','controlled_by_rebels = yes pop_majority_religion = '+religion)
        return result.encode()
    def test_select_pair_without_ambiguity(self):
        result=engine.find_events(self.events())
        self.assertEqual(result['new']['religion'],'protestant')
        self.assertEqual(result['union']['religion'],'catholic')
        self.assertEqual(result['dynamic']['religion'],'jewish')
    def test_guard_mismatch_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'Guard'):
            engine.find_events(self.events().replace(b'limit = { controlled_by_rebels = yes',b'limit = { controlled_by_rebels = no',1))
    def test_old_output_rejected(self):
        with self.assertRaises(ValueError): engine.find_events(event('OBO','north_german',1).encode())

if __name__=='__main__': unittest.main()
