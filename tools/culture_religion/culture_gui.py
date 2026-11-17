"""Ограниченный сценарий снимка культурной карты; запускается явно главным агентом."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
import time
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.chronology_import import engine_acceptance as old
from tools.culture_religion import gui
from tools.culture_religion.native_acceptance import wait_initialisation


def preflight(environment, save, output, zoom_out):
    environment, output = old.work_path(environment), old.work_path(output)
    old.require(not output.exists(), 'Для снимков нужен новый каталог .work')
    old.require(type(zoom_out) is int and 1 <= zoom_out <= 8, 'Допустимо от 1 до 8 шагов колеса')
    old.require(Path(save).name == save and Path(save).suffix == '.v2', 'Требуется имя копии .v2')
    metadata = json.loads((environment/'environment.json').read_text(encoding='utf-8'))
    game, userdata = environment/'game', environment/'userdata'
    old.require(Path(metadata['game']).resolve() == game and
                Path(metadata['userdata']).resolve() == userdata, 'Неверная изоляция среды')
    redirect = (game/'userdir.txt').read_text(encoding='ascii').strip()
    old.require(Path(redirect).resolve() == userdata, 'Настройки не перенаправлены в тестовую среду')
    for directory in (game, userdata):
        text = (directory/'settings.txt').read_text(encoding='cp1251')
        old.require(re.search(r'(?im)^\s*autosave\s*=\s*"never"\s*$', text),
                    'Автосохранение должно быть отключено в '+str(directory))
        old.require(re.search(r'(?im)^\s*fullScreen\s*=\s*no\s*$', text),
                    'Требуется оконная тестовая игра')
    inputs = {}
    for relative in ('events/adding_cores.txt', 'common/cultures.txt',
                     'interface/menubar.gui', 'interface/menubar.gfx'):
        target = (game/relative).resolve(strict=True)
        old.require(target.is_relative_to(game), 'Игровые данные выходят из среды')
        actual, production = old.cli.digest(target), old.cli.digest(old.ROOT/relative)
        old.require(actual == production, 'Тестовая копия отличается от production: '+relative)
        inputs[relative] = actual
    source = (userdata/'save games'/save).resolve(strict=True)
    old.require(source.is_relative_to(userdata), 'Сохранение находится вне тестового профиля')
    return dict(environment=str(environment), output=str(output), save=save,
                save_path=str(source), save_sha256=old.cli.digest(source), inputs=inputs,
                expected_client=[1366, 768], africa_client=[1180, 716],
                nationality_client=[1083, 642],
                zoom_client=[683, 430], zoom_out=zoom_out, image_review_required=True,
                startup_timeout=900, load_timeout=1200)


def run(environment, save, output, zoom_out=4):
    record = preflight(environment, save, output, zoom_out)
    environment, output = Path(record['environment']), Path(record['output'])
    output.mkdir(parents=True, exist_ok=False)
    record.update(ok=False, frames=[], actions=[])
    owned = None
    session_path = environment/'session.json'
    previous = json.loads(session_path.read_text(encoding='utf-8')) if session_path.exists() else {}

    def checkpoint():
        (output/'report.json').write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

    def checked(reply, operation):
        record[operation] = reply
        checkpoint()
        old.require(reply.get('ok'), operation+': '+str(reply))
        return reply

    def frame(name, activate=False):
        result = gui.capture_politics(environment, session, owned, output/(name+'.png'),
                                      open_window=False, activate=activate)
        record['frames'].append(dict(stage=name, **result))
        checkpoint()
        old.require(result['client_size'] == record['expected_client'],
                    'Координаты квалифицированы только для клиента 1366×768')
        return result

    try:
        checkpoint()
        started_at = time.time()
        try:
            started = old.cli.start(environment, hidden=False)
        finally:
            if session_path.exists():
                session = json.loads(session_path.read_text(encoding='utf-8'))
                if session.get('token') != previous.get('token'):
                    owned = old.OwnedProcess(session['pid'], environment/'game/v2game.exe')
        checked(started, 'started')
        old.require(owned is not None, 'Нет handle созданного процесса')
        wait_initialisation(environment, started_at, timeout=record['startup_timeout'])
        record['initialisation_complete'] = True
        checkpoint()
        checked(old.cli.load_campaign(environment, save, record['load_timeout']), 'loaded')
        state = checked(old.cli.request(environment, 'status', 60), 'initial_status')
        old.require(state.get('campaign_loaded') is True and state.get('paused') is True and
                    state.get('event_windows') == 0 and state.get('advance_target') == 0,
                    'Нужна загруженная кампания на паузе без событий и продвижения времени')
        frame('00-loaded', activate=True)
        # Буквенная горячая клавиша зависит от раскладки; используем штатную кнопку.
        record['actions'].append(gui.click_client(environment, session, owned, *record['nationality_client']))
        frame('01-nationality')
        record['actions'].append(gui.click_client(environment, session, owned, *record['africa_client']))
        frame('02-africa')
        record['actions'].append(gui.scroll_client(environment, session, owned,
                                                   *record['zoom_client'], -zoom_out))
        frame('03-wide')
        final = checked(old.cli.request(environment, 'status', 60), 'final_status')
        old.require(final.get('paused') is True and final.get('date_raw') == state.get('date_raw'),
                    'Календарь изменился при проверке карты')
        record['ok'] = True
    except BaseException as error:
        record.update(error=str(error), traceback=traceback.format_exc())
    finally:
        try:
            if owned is not None:
                owned.stop()
                record['stopped'] = True
        except BaseException as error:
            record.update(ok=False, cleanup_error=str(error))
        try:
            record['save_unchanged'] = old.cli.digest(Path(record['save_path'])) == record['save_sha256']
            record['inputs_unchanged'] = all(old.cli.digest(environment/'game'/name) == digest
                                              for name, digest in record['inputs'].items())
            old.require(record['save_unchanged'] and record['inputs_unchanged'],
                        'Изменились исходные файлы проверки')
        except BaseException as error:
            record.update(ok=False, integrity_error=str(error))
        checkpoint()
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', required=True, type=Path)
    parser.add_argument('--save', default='cr36_start.v2')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--zoom-out', type=int, default=4)
    parser.add_argument('--dry-run', action='store_true', help='Проверить входы без запуска игры и GUI')
    args = parser.parse_args()
    if args.dry_run:
        result = dict(ok=True, dry_run=True, **preflight(args.environment, args.save, args.output, args.zoom_out))
    else:
        result = run(args.environment, args.save, args.output, args.zoom_out)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
