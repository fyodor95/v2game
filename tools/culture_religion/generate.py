"""Изолированная двойная генерация с проверкой исходных файлов перед установкой."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]
ROOT_FILES=('autogen.cfg','events_id_list.txt','decisions_id_list.txt')
FOLDERS=('common','history','map','poptypes','technologies','inventions','events','decisions','localisation','units')
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def hashes(root): return {p.relative_to(root).as_posix():sha(p) for p in root.rglob('*') if p.is_file()}
def work_path(path):
    result=path.resolve()
    if not result.is_relative_to(ROOT/'.work'): raise ValueError('Результаты только внутри .work')
    return result

def prepare(destination):
    destination=work_path(destination)
    if destination.exists(): raise ValueError('Каталог уже существует')
    game=destination/'game'
    for folder in FOLDERS: shutil.copytree(ROOT/folder,game/folder)
    for rel in ROOT_FILES:
        shutil.copy2(ROOT/rel,game/rel)
    config=destination/'config';(config/'run').mkdir(parents=True)
    (config/'autogen.cfg').write_text('game_path = "'+game.as_posix()+'"\n',encoding='utf-8')
    (destination/'before.json').write_text(json.dumps(hashes(game),indent=2),encoding='utf-8')
    return {'ok':True,'destination':str(destination)}

def run(destination,generator):
    destination=work_path(destination);game=destination/'game'
    original=json.loads((destination/'before.json').read_text(encoding='utf-8'))
    if hashes(game)!=original: raise ValueError('Изменены входные данные подготовленной генерации')
    results=[]
    for number in (1,2):
        with (destination/f'run-{number}.log').open('xb') as log:
            subprocess.run([str(generator.resolve())],cwd=destination/'config/run',stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
        result=hashes(game); results.append(result)
        (destination/f'after-{number}.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    if results[0]!=results[1]: raise ValueError('Повторная генерация отличается')
    changed=[p for p,h in results[1].items() if original.get(p)!=h]
    removed=sorted(set(original)-set(results[1]))
    if removed: raise ValueError('Генерация удалила файлы: '+str(removed))
    report={'ok':True,'files':len(results[1]),'changed':changed,'generator':str(generator.resolve()),'generator_sha256':sha(generator)}
    (destination/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    return report

def install(destination):
    destination=work_path(destination);game=destination/'game'
    before=json.loads((destination/'before.json').read_text(encoding='utf-8'))
    after=json.loads((destination/'after-2.json').read_text(encoding='utf-8'))
    report=json.loads((destination/'result.json').read_text(encoding='utf-8'))
    if not report['ok'] or hashes(game)!=after: raise ValueError('Генерация не прошла проверку либо изменена')
    # Проверяем всё до первой записи, включая входы, которые сам генератор не изменял.
    current={}
    for folder in FOLDERS:
        current.update({folder+'/'+rel:checksum for rel,checksum in hashes(ROOT/folder).items()})
    current.update({rel:sha(ROOT/rel) for rel in ROOT_FILES if (ROOT/rel).is_file()})
    if current!=before:
        delta=sorted(rel for rel in current.keys()|before.keys() if current.get(rel)!=before.get(rel))
        raise ValueError('Изменён набор исходных данных: '+str(delta))
    for rel,checksum in before.items():
        if not (ROOT/rel).is_file() or sha(ROOT/rel)!=checksum: raise ValueError('Исходный файл изменён: '+rel)
    for rel in set(after)-set(before):
        if (ROOT/rel).exists(): raise ValueError('Новый файл уже существует: '+rel)
    for rel in report['changed']:
        target=ROOT/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(game/rel,target)
    return {'ok':True,'installed':len(report['changed'])}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation',choices=('prepare','run','install'));parser.add_argument('--destination',type=Path,required=True);parser.add_argument('--generator',type=Path)
    args=parser.parse_args()
    if args.operation=='prepare': result=prepare(args.destination)
    elif args.operation=='run':
        if not args.generator: parser.error('Нужен --generator')
        result=run(args.destination,args.generator)
    else: result=install(args.destination)
    print(json.dumps(result,ensure_ascii=False,indent=2))
