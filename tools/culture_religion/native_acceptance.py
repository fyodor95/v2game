"""Явная квалификация нового скриптового эффекта на настоящей SP-игре."""
from __future__ import annotations
import argparse
import json
import re
import hashlib
from pathlib import Path
import sys
import time
import traceback
import uuid
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.culture_religion import engine as fixtures
from tools.culture_religion.snapshot import capture
old=fixtures.old
cli=old.cli

def prepare(environment):
    game=environment/'game'
    decisions=['political_decisions = {']
    effects={
      'apply':'primary_culture = north_german religion = jewish set_religion_from_primary_culture = yes',
      'tie_current':'primary_culture = north_german religion = protestant set_religion_from_primary_culture = yes',
      'nopop':'primary_culture = polar_bears religion = jewish set_religion_from_primary_culture = yes',
    }
    for religion in sorted(old.identifiers(game/'common/religion.txt')):effects['relig_'+religion]='religion = '+religion
    effects['gov_hms']='government = hms_government plurality = 100'
    effects['gov_prussian']='government = prussian_constitutionalism plurality = 100'
    loc=[]
    for name,effect in effects.items():
        decisions.append(f'cr_native_{name} = {{ potential = {{ tag = PRU }} allow = {{ always = yes }} effect = {{ {effect} set_country_flag = cr_native_{name}_done }} ai_will_do = {{ factor = 0 }} }}')
        decisions.append(f'cr_native_{name}_clear = {{ potential = {{ tag = PRU }} allow = {{ always = yes }} effect = {{ clr_country_flag = cr_native_{name}_done }} ai_will_do = {{ factor = 0 }} }}')
        decisions.append(f'cr_native_{name}_done = {{ potential = {{ tag = PRU }} allow = {{ has_country_flag = cr_native_{name}_done }} effect = {{ }} ai_will_do = {{ factor = 0 }} }}')
        for key in (f'cr_native_{name}',f'cr_native_{name}_done'): loc.append(key+';Проверка государственной религии;;;;;;;;;;;;;x')
    decisions.append('}')
    outputs={'decisions/culture_religion_native_test.txt':'\r\n'.join(decisions)+'\r\n',
             'localisation/culture_religion_native_test.csv':'\r\n'.join(loc)+'\r\n'}
    for rel,text in outputs.items():
        path=game/rel;raw=text.encode('cp1251')
        if path.exists() and path.name not in ('culture_religion_native_test.txt','culture_religion_native_test.csv'): raise ValueError('Файл фикстуры уже существует: '+str(path))
        path.write_bytes(raw)


def wait_initialisation(environment,started_at,timeout=900):
    log=environment/'userdata/logs/time.log';deadline=time.monotonic()+timeout
    while True:
        if log.exists() and log.stat().st_mtime>=started_at and 'Total loadtime after app init' in log.read_text(encoding='cp1251',errors='replace'): return
        old.require(time.monotonic()<deadline,'Начальная загрузка не завершилась')
        time.sleep(1)


def execute(environment,decision):
    cleared=cli.request(environment,'decision-execute PRU '+decision+'_clear',60);old.require(cleared.get('ok'),str(cleared))
    deadline=time.monotonic()+30
    while True:
        result=cli.request(environment,'decision-check PRU '+decision+'_done',60);old.require(result.get('ok'),str(result))
        if result.get('allow') is False:break
        old.require(time.monotonic()<deadline,'Не сброшен маркер эффекта');time.sleep(.1)
    checked=cli.request(environment,'decision-execute PRU '+decision,60);old.require(checked.get('ok'),str(checked))
    deadline=time.monotonic()+30
    while True:
        result=cli.request(environment,'decision-check PRU '+decision+'_done',60)
        old.require(result.get('ok'),str(result))
        if result.get('allow') is True:return
        old.require(time.monotonic()<deadline,'Не исполнен маркер после эффекта')
        time.sleep(.1)


def run(environment,save,output,winner='protestant',gui=False):
    environment=old.work_path(environment);output=old.work_path(output)
    old.require(not output.exists(),'Отчёт уже существует')
    prepare(environment);record=dict(ok=False,steps=[]);owned=None
    previous=json.loads((environment/'session.json').read_text()) if (environment/'session.json').exists() else {}
    config=environment/'game/z_CreatineCore/great_power_culture_cores.ini'
    original_config=config.read_bytes()
    disabled,count=re.subn(rb'(?m)^(\s*ENABLE_GREAT_POWER_CULTURE_CORES\s*=\s*)[01]',rb'\g<1>0',original_config)
    old.require(count==1,'Нет единственной настройки GPC')
    record['gpc_original_sha256']=hashlib.sha256(original_config).hexdigest()
    try:
        config.write_bytes(disabled)
        started_at=time.time()
        try: record['started']=cli.start(environment,hidden=True)
        finally:
            session=json.loads((environment/'session.json').read_text())
            if session.get('token')!=previous.get('token'):
                owned=old.OwnedProcess(session['pid'],environment/'game/v2game.exe')
        old.require(record['started'].get('ok'),str(record['started']))
        wait_initialisation(environment,started_at)
        record['loaded']=cli.load_campaign(environment,save,1200);old.require(record['loaded'].get('ok'),str(record['loaded']))
        if gui:
            from tools.culture_religion.gui import capture_politics
            try:record['gui_initial']=capture_politics(environment,session,owned,output.with_name(output.stem+'-initial.png'))
            except Exception as error:record['gui_error']=str(error);gui=False
        actions=[('apply',winner),('nopop','jewish'),('apply',winner)]
        if winner=='catholic':actions.extend([('tie_current','protestant'),('apply',winner)])
        for index,(action,expected) in enumerate(actions):
            execute(environment,'cr_native_'+action)
            snapshot=capture(environment,session,owned,['PRU']);state=snapshot['countries']['PRU']
            record['steps'].append(dict(action=action,snapshot=snapshot))
            old.require(state['religion']==expected,f'Религия {state} вместо {expected}')
            if gui:record['steps'][-1]['gui']=capture_politics(environment,session,owned,output.with_name(output.stem+f'-{index}.png'),open_window=False)
            old.require(state['primary']==('polar_bears' if action=='nopop' else 'north_german'),'Неверная primary')
        record['effects_ok']=True
        if gui:
            record['gui_religions']=[]
            for religion in sorted(old.identifiers(environment/'game/common/religion.txt')):
                execute(environment,'cr_native_relig_'+religion)
                actual=capture(environment,session,owned,['PRU'])
                old.require(actual['countries']['PRU']['religion']==religion,'Не назначена тестовая религия')
                image=capture_politics(environment,session,owned,output.with_name(output.stem+'-'+religion+'.png'),open_window=False)
                record['gui_religions'].append(dict(religion=religion,snapshot=actual,image=image))
            for government in ('hms','prussian'):
                execute(environment,'cr_native_gov_'+government)
                record['gui_'+government]=capture_politics(environment,session,owned,output.with_name(output.stem+'-'+government+'.png'),open_window=False)
            execute(environment,'cr_native_apply')
        name='cr_native_'+uuid.uuid4().hex[:8]
        result=cli.request(environment,'save-native '+name,180);old.require(result.get('ok'),str(result))
        record['save']=name+'.v2'
        old.require(cli.return_to_menu(environment,180).get('ok'),'Не выполнен возврат в меню')
        old.require(cli.load_campaign(environment,name+'.v2',1200).get('ok'),'Не выполнена повторная загрузка')
        record['reloaded']=capture(environment,session,owned,['PRU'])
        old.require(record['reloaded']['countries']['PRU']['religion']==winner,'Религия потеряна после загрузки')
        record['functional_ok']=True
        record['ok']='gui_error' not in record
    except BaseException as error:record.update(error=str(error),traceback=traceback.format_exc())
    finally:
        if owned:owned.stop();record['stopped']=True
        config.write_bytes(original_config)
        record['gpc_restored']=config.read_bytes()==original_config
        output.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    return record

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--environment',type=Path,required=True);parser.add_argument('--save',default='chr_fresh.v2');parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--winner',default='protestant');parser.add_argument('--gui',action='store_true')
    args=parser.parse_args();result=run(args.environment,args.save,args.output,args.winner,args.gui)
    print(json.dumps(result,ensure_ascii=False));raise SystemExit(0 if result['ok'] else 1)
