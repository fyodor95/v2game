"""Границы и идентичности read-only снимка проверяются до нативной квалификации."""
import struct
import unittest
from tools.culture_religion.snapshot import inspect

class SnapshotTests(unittest.TestCase):
    def setUp(self):
        self.memory={}; self.base=0x10000
        self.put(self.base+0xe587e4,0x1000);self.put(0x1000,2);self.put(0x1004,0x1100)
        self.put(0x1104,0x2000);self.put(0x201c,int.from_bytes(b'D01\0','little'));self.put(0x2020,1)
        self.put(0x2e64,0x4000);self.put(0x2e68,0x5000)
        self.string(0x4018,'north_german');self.string(0x5010,'protestant');self.put(0x5008,2)
    def put(self,at,value): self.write(at,struct.pack('<I',value))
    def write(self,at,raw): self.memory.update({at+i:b for i,b in enumerate(raw)})
    def string(self,at,value):
        raw=value.encode();self.write(at,raw+b'\0'*(16-len(raw))+struct.pack('<II',len(raw),15))
    def read(self,at,size):return bytes(self.memory[at+i] for i in range(size))
    def test_reads_primary_and_religion(self):
        result=inspect(self.read,self.base,['D01'])['D01']
        self.assertEqual(result['primary'],'north_german');self.assertEqual(result['religion'],'protestant')
    def test_country_identity_rejected(self):
        self.put(0x2020,2)
        with self.assertRaises(ValueError):inspect(self.read,self.base,['D01'])
    def test_missing_tag_rejected(self):
        with self.assertRaises(ValueError):inspect(self.read,self.base,['D02'])
    def test_bad_string_rejected(self):
        self.put(0x4018+16,1025)
        with self.assertRaises(ValueError):inspect(self.read,self.base,['D01'])
    def test_oversized_manager_rejected(self):
        self.put(0x1000,1000000)
        with self.assertRaises(ValueError):inspect(self.read,self.base,['D01'])

if __name__=='__main__':unittest.main()
