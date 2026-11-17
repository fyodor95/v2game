"""Статическая карта и палитра для ревью; не заменяют GUI-приёмку игры."""
from collections import Counter
from decimal import Decimal
import argparse
from html import escape
import json
from pathlib import Path
import struct
import zlib

from apply_history import ROOT, HERE, NEW, Spans, culture_data, rgb, scalar


def chunk(kind, data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)


def png(path, width, rows):
    data=b''.join(b'\x00'+row for row in rows)
    output=b'\x89PNG\r\n\x1a\n'
    output+=chunk(b'IHDR',struct.pack('>IIBBBBB',width,len(rows),8,2,0,0,0))
    output+=chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b'')
    path.write_bytes(output)


def render(root, date, output, scale=3):
    _,_,cultures=culture_data((root/'common/cultures.txt').read_bytes())
    counts={}
    for file in (root/'history/pops'/date).glob('*.txt'):
        for province in Spans(file.read_bytes().decode('cp1251')).roots():
            total=counts.setdefault(int(province.key),Counter())
            for pop in province.value:
                if isinstance(pop.value,list) and pop.single('size'):
                    total[scalar(pop,'culture')]+=Decimal(scalar(pop,'size'))
    main={pid:max(sorted(c),key=c.get) for pid,c in counts.items() if c}
    codes={}
    for line in (root/'map/definition.csv').read_bytes().decode('cp1251').splitlines():
        parts=line.split(';')
        if parts[0].isdigit():
            r,g,b=(int(Decimal(x)) for x in parts[1:4]);codes[bytes((b,g,r))]=int(parts[0])
    fills={code:bytes(rgb(cultures[main[pid]])) if pid in main else b'\x28\x34\x42' for code,pid in codes.items()}
    bmp=(root/'map/provinces.bmp').read_bytes()
    offset=struct.unpack_from('<I',bmp,10)[0]
    width,height=struct.unpack_from('<ii',bmp,18)
    stride=(width*3+3)&~3
    rows=[]
    for y in range(0,height,scale):
        # Карта V2 хранит первый ряд текстуры с севера; обычный BMP-viewer
        # интерпретирует положительную высоту иначе и переворачивает географию.
        base=offset+y*stride
        rows.append(b''.join(fills.get(bmp[base+x*3:base+x*3+3],b'\x28\x34\x42') for x in range(0,width,scale)))
    png(output,len(range(0,width,scale)),rows)


def swatches(output):
    report=json.loads((HERE/'registry.json').read_text(encoding='utf-8'))
    rows=(len(NEW)+2)//3
    text=[f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{rows*34+70}" viewBox="0 0 1200 {rows*34+70}">',
          '<rect width="100%" height="100%" fill="white"/>',
          '<text x="16" y="28" font-family="sans-serif" font-size="19">Новые культуры: палитра CIELAB по соседству реальных провинций</text>']
    for i,(code,(name,_,_)) in enumerate(sorted(NEW.items())):
        col,row=divmod(i,rows);x=col*400+16;y=row*34+48
        color=','.join(map(str,report['colors'][code]['rgb']))
        text.append(f'<rect x="{x}" y="{y}" width="30" height="25" fill="rgb({color})" stroke="#333"/>')
        text.append(f'<text x="{x+40}" y="{y+17}" font-family="sans-serif" font-size="13">{escape(name)}</text>')
    text.append('</svg>')
    output.write_text('\n'.join(text)+'\n',encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--date',choices=('1836.1.1','1861.4.14'),default='1836.1.1')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--swatches',type=Path)
    args=parser.parse_args()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    render(ROOT,args.date,args.output)
    if args.swatches:swatches(args.swatches)
