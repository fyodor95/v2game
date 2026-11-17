"""Адаптер настоящей приёмки к парам культура–религия; игры запускает только явный run."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.chronology_import import engine_acceptance as old
from tools.culture_religion.validate import read


def find_events(raw,dynamic_culture=None):
    text=raw.decode('cp1251'); matches={}; dynamic=[]
    preferred={('OBO','north_german'):'protestant',('SAX','north_german'):'protestant',('GER','south_german'):'protestant'}
    for event in old.Spans(text).roots():
        if event.key!='province_event': continue
        guard=old.one(event,'option').single('if')
        if guard is None: continue
        tag=old.scalar(guard,'add_core'); trigger=old.one(event,'trigger')
        culture=old.scalar(trigger,'culture'); religion=old.scalar(trigger,'pop_majority_religion')
        limit=old.one(guard,'limit')
        old.require(re.sub(r'\s+','',text[trigger.start:trigger.end])==re.sub(r'\s+','',text[limit.start:limit.end]),'Guard отличается от trigger')
        if religion is None: raise ValueError('Требуется новая генерация с религиями')
        record=dict(id=int(old.scalar(event,'id')),tag=tag,culture=culture,religion=religion,
                    trigger=text[trigger.start:trigger.end])
        key=tag,culture
        if key in preferred and (key not in matches or religion==preferred[key]): matches[key]=record
        if tag and re.fullmatch(r'D\d\d',tag) and (dynamic_culture is None or culture==dynamic_culture): dynamic.append(record)
    old.require(len(matches)==3 and dynamic,'Нет OBO/SAX/GER/D первых событий')
    return dict(new=matches['OBO','north_german'],old=matches['SAX','north_german'],
                union=matches['GER','south_german'],dynamic=dynamic[0])

_original_make_save=old.make_save

def make_save(text,provinces,selected,foreign,sax,case,tags):
    original, count=_original_make_save(text,provinces,selected,foreign,sax,case,tags)
    modified=original.decode('cp1251'); edits=[]; religion=case['religion']
    cultures=old.identifiers(Path(case['game_root'])/'common/cultures.txt')
    religions=old.identifiers(Path(case['game_root'])/'common/religion.txt')
    for node in old.Spans(modified).roots():
        if node.key==str(selected):
            for pop in node.value:
                if not isinstance(pop.value,list): continue
                for field in pop.value:
                    if field.key in cultures and field.value in religions:
                        edits.append((field.start,field.end,'catholic' if case['name']=='religion' and religion!='catholic' else 'protestant' if case['name']=='religion' else religion))
        if node.key=='PRU' and case['name']=='civilization':
            field=old.one(node,'civilized'); edits.append((field.start,field.end,'no'))
        if node.key==case['tag'] and case['name']=='state_religion':
            field=old.one(node,'religion'); edits.append((field.start,field.end,'catholic' if religion!='catholic' else 'protestant'))
    for start,end,value in sorted(edits,reverse=True): modified=modified[:start]+value+modified[end:]
    return modified.encode('cp1251'),count+len(edits)

_original_fixture_cases=old.fixture_cases
_context={}

def fixture_cases(events):
    cases=_original_fixture_cases(events)
    for name in ('religion','state_religion','civilization'):
        cases.append(dict(name=name,event='new',allow=False,save='chr_'+name+'.v2',
                          tag=events['new']['tag'],culture=events['new']['culture']))
    for case in cases:
        case.update(religion=events[case['event']]['religion'],game_root=str(_context['game']))
    return cases


class Runner(old.Runner):
    def checked(self,response,operation):
        result=super().checked(response,operation)
        if operation=='start':
            from tools.culture_religion.native_acceptance import wait_initialisation
            wait_initialisation(self.environment,(self.environment/'session.json').stat().st_mtime)
        return result

    def save(self,name):
        path=super().save(name)
        # Дополнительная независимая фиксация культур/религий всех D в каждом сейве.
        states={}
        for node in old.Spans(path.read_bytes().decode('cp1251')).roots():
            if re.fullmatch(r'D\d\d',node.key or '') and isinstance(node.value,list):
                states[node.key]=dict(primary=old.scalar(node,'primary_culture'),religion=old.scalar(node,'religion'))
        self.record.setdefault('dynamic_states',{})[name]=states; self.checkpoint()
        if name.endswith('_after') and self.record['cases'][-1]['name']=='dynamic':
            fixture=json.loads((self.fixtures/'manifest.json').read_text(encoding='utf-8'))
            event=fixture['events']['dynamic']; actual=states[event['tag']]
            old.require(actual==dict(primary=event['culture'],religion=event['religion']),f'Неверная D-культура/религия: {actual}')
        return path


def install_adapters():
    old.find_events=find_events; old.make_save=make_save; old.fixture_cases=fixture_cases


def main():
    parser=argparse.ArgumentParser(description=__doc__); sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('prepare'); p.add_argument('--game-root',required=True,type=Path);p.add_argument('--source-save',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    p=sub.add_parser('run');p.add_argument('--environment',required=True,type=Path);p.add_argument('--fixtures',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--case',action='append',dest='cases')
    args=parser.parse_args(); install_adapters()
    if args.command=='prepare':
        _context['game']=args.game_root
        print(json.dumps(old.prepare(args.game_root,args.source_save,args.output),ensure_ascii=False)); return 0
    with old.work_path(args.output).open('x',encoding='utf-8') as stream:
        runner=Runner(args.environment,args.fixtures,stream,args.cases); ok=runner.run()
    print(json.dumps(dict(ok=ok,errors=runner.record['errors']),ensure_ascii=False));return 0 if ok else 1

if __name__=='__main__': raise SystemExit(main())
