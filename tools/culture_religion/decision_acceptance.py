"""Настоящее решение Д-страны: религия культуры всей страны, перезагрузка и GUI."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys
import time
import traceback
import uuid
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.culture_religion import native_acceptance as native
from tools.culture_religion.snapshot import capture
from tools.culture_religion.gui import capture_politics
old, cli = native.old, native.cli


def run(environment, output, gui=False):
    environment, output = old.work_path(environment), old.work_path(output)
    old.require(not output.exists(), 'Отчёт уже существует')
    config = environment / 'game/z_CreatineCore/great_power_culture_cores.ini'
    original = config.read_bytes()
    disabled, count = native.re.subn(rb'(?m)^(\s*ENABLE_GREAT_POWER_CULTURE_CORES\s*=\s*)[01]', rb'\g<1>0', original)
    old.require(count == 1, 'Нет единственной настройки GPC')
    record, owned = dict(ok=False), None
    try:
        config.write_bytes(disabled)
        started = time.time()
        record['start'] = cli.start(environment, hidden=True)
        session = json.loads((environment / 'session.json').read_text())
        owned = old.OwnedProcess(session['pid'], environment / 'game/v2game.exe')
        old.require(record['start'].get('ok'), str(record['start']))
        native.wait_initialisation(environment, started)
        record['load'] = cli.load_campaign(environment, 'cr_d_primary.v2', 1200)
        old.require(record['load'].get('ok'), str(record['load']))
        record['before'] = capture(environment, session, owned, ['D50'])
        old.require(record['before']['countries']['D50']['primary'] == 'british', 'Не дефолтная основная культура')
        if gui:
            record['gui_before'] = capture_politics(environment, session, owned, output.with_suffix('.before.png'))
        record['check'] = cli.request(environment, 'decision-check D50 D_primary_north_german', 60)
        old.require(record['check'].get('allow') is True and record['check'].get('potential') is True, str(record['check']))
        record['execute'] = cli.request(environment, 'decision-execute D50 D_primary_north_german', 60)
        old.require(record['execute'].get('ok'), str(record['execute']))
        deadline = time.monotonic() + 30
        while True:
            record['after'] = capture(environment, session, owned, ['D50'])
            state = record['after']['countries']['D50']
            if state['primary'] == 'north_german' and state['religion'] == 'catholic':
                break
            old.require(time.monotonic() < deadline, 'Не подтверждена культура/религия после решения')
            time.sleep(.1)
        if gui:
            record['gui_after'] = capture_politics(environment, session, owned, output.with_suffix('.after.png'), False)
        name = 'crd_' + uuid.uuid4().hex[:8]
        record['save'] = cli.request(environment, 'save-native ' + name, 180)
        old.require(record['save'].get('ok'), str(record['save']))
        old.require(cli.return_to_menu(environment, 180).get('ok'), 'Не выполнен возврат в меню')
        old.require(cli.load_campaign(environment, name + '.v2', 1200).get('ok'), 'Не выполнена повторная загрузка')
        record['reloaded'] = capture(environment, session, owned, ['D50'])
        state = record['reloaded']['countries']['D50']
        old.require(state['primary'] == 'north_german' and state['religion'] == 'catholic', 'Результат потерян после загрузки')
        if gui:
            record['gui_reloaded'] = capture_politics(environment, session, owned, output.with_suffix('.reloaded.png'))
        old.require(cli.return_to_menu(environment, 180).get('ok'), 'Не выполнен возврат в меню')
        old.require(cli.load_campaign(environment, 'chr_fresh.v2', 1200).get('ok'), 'Не загружена другая страна')
        record['other_country'] = capture(environment, session, owned, ['PRU'])
        old.require(record['other_country']['countries']['PRU']['religion'] == 'protestant', 'Неверная религия другой страны')
        if gui:
            record['gui_other_country'] = capture_politics(environment, session, owned, output.with_suffix('.other.png'))
        record['ok'] = True
    except BaseException as error:
        record.update(error=str(error), traceback=traceback.format_exc())
    finally:
        if owned:
            owned.stop()
        config.write_bytes(original)
        record['gpc_restored'] = config.read_bytes() == original
        output.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gui', action='store_true')
    args = parser.parse_args()
    result = run(args.environment, args.output, args.gui)
    print(json.dumps(dict(ok=result['ok'], error=result.get('error')), ensure_ascii=False))
    raise SystemExit(0 if result['ok'] else 1)
