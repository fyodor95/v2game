"""Захват экрана не нужен для проверки формата PNG и барьера очереди."""
import tempfile
from pathlib import Path
import struct
import unittest
from unittest.mock import patch
import zlib
from tools.culture_religion import gui,native_acceptance as n

class NativeToolsTests(unittest.TestCase):
    def test_png_channels_and_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'pixels.png';gui.write_png(p,2,1,bytes((0,0,255,0,0,255,0,0)))
            raw=p.read_bytes();self.assertEqual(raw[:8],b'\x89PNG\r\n\x1a\n');at=8;pixels=None
            while at<len(raw):
                size=struct.unpack_from('>I',raw,at)[0];kind=raw[at+4:at+8];data=raw[at+8:at+8+size]
                self.assertEqual(struct.unpack_from('>I',raw,at+8+size)[0],zlib.crc32(kind+data)&0xffffffff)
                if kind==b'IDAT':pixels=zlib.decompress(data)
                at+=12+size
            self.assertEqual(pixels,b'\0\xff\0\0\0\xff\0')
    def test_stale_marker_is_cleared_before_execute(self):
        replies=[dict(ok=True),dict(ok=True,allow=True),dict(ok=True,allow=False),dict(ok=True),dict(ok=True,allow=False),dict(ok=True,allow=True)]
        with patch.object(n.cli,'request',side_effect=replies) as request,patch.object(n.time,'sleep'):
            n.execute(Path('own-environment'),'cr_native_apply')
        commands=[c.args[1] for c in request.call_args_list]
        self.assertEqual(commands.count('decision-execute PRU cr_native_apply'),1)
        self.assertEqual(commands[0],'decision-execute PRU cr_native_apply_clear')
        self.assertEqual(commands.index('decision-execute PRU cr_native_apply'),3)

if __name__=='__main__':unittest.main()
