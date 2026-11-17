"""Установка результата не перезаписывает параллельные изменения."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.culture_religion import generate as g

class GenerationInstall(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.dest=self.root/'.work/run';self.game=self.dest/'game';self.game.mkdir(parents=True)
        for name in ('a.txt','b.txt'):
            (self.root/name).write_text('original');(self.game/name).write_text('original')
        (self.root/'common').mkdir();(self.game/'common').mkdir()
        self.before=g.hashes(self.game)
        (self.game/'a.txt').write_text('generated')
        self.after=g.hashes(self.game)
        for name,obj in [('before.json',self.before),('after-2.json',self.after),('result.json',dict(ok=True,changed=['a.txt']))]:
            (self.dest/name).write_text(json.dumps(obj),encoding='utf-8')
    def install(self):
        with patch.object(g,'ROOT',self.root), patch.object(g,'FOLDERS',('common',)), patch.object(g,'ROOT_FILES',('a.txt','b.txt')): return g.install(self.dest)
    def test_installs_verified_output(self):
        self.assertEqual(self.install()['installed'],1)
        self.assertEqual((self.root/'a.txt').read_text(),'generated')
    def test_unmodified_input_drift_blocks_all_writes(self):
        (self.root/'b.txt').write_text('other agent')
        with self.assertRaisesRegex(ValueError,'b.txt'): self.install()
        self.assertEqual((self.root/'a.txt').read_text(),'original')
        self.assertEqual((self.root/'b.txt').read_text(),'other agent')
    def test_changed_artifact_blocks_install(self):
        (self.game/'a.txt').write_text('tampered')
        with self.assertRaises(ValueError): self.install()
        self.assertEqual((self.root/'a.txt').read_text(),'original')
    def test_new_input_blocks_all_writes(self):
        (self.root/'common/new.txt').write_text('other agent')
        with self.assertRaisesRegex(ValueError,'common/new.txt'): self.install()
        self.assertEqual((self.root/'a.txt').read_text(),'original')
    def test_new_file_collision_blocks_all_writes(self):
        (self.game/'new.txt').write_text('generated');(self.root/'new.txt').write_text('other agent')
        (self.dest/'after-2.json').write_text(json.dumps(g.hashes(self.game)),encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'new.txt'): self.install()
        self.assertEqual((self.root/'a.txt').read_text(),'original')

if __name__=='__main__':unittest.main()
