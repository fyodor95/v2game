"""Управление настоящей сетевой партией через нативные команды, без мыши и клавиатуры."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import time

import cli


class Driver:
    def __init__(self, host: Path, client: Path, output: Path, timeout=600):
        self.host, self.client = host.resolve(), client.resolve()
        self.output, self.timeout = output, timeout
        if self.host == self.client:
            raise ValueError('Хосту и клиенту нужны разные тестовые среды')
        for environment in (self.host, self.client):
            metadata = json.loads((environment / 'environment.json').read_text(encoding='utf-8'))
            if Path(metadata['game']).resolve() != environment / 'game' or not environment.is_relative_to(cli.ROOT / '.work'):
                raise ValueError('Требуются изолированные среды внутри .work')
        if output.exists():
            raise FileExistsError('Отчёт уже существует')
        self.record = {'ok': False, 'complete': False, 'physical_input': False, 'steps': []}

    def save(self):
        self.output.parent.mkdir(parents=True, exist_ok=True)
        self.output.write_text(json.dumps(self.record, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    def call(self, environment, wire, mutation=False, timeout=None):
        result = cli.request(environment, wire, self.timeout if timeout is None else timeout)
        self.record['steps'].append({'role': 'host' if environment == self.host else 'client',
                                     'command': wire, 'mutation': mutation, 'result': result})
        self.save()
        return result

    def send(self, environment, wire):
        deadline = time.monotonic() + self.timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f'Истёк срок команды: {wire}')
            result = self.call(environment, wire, mutation=True, timeout=remaining)
            if result.get('ok'):
                return result
            # Эти отказы находятся ДО enqueue. Отправленная или неоднозначная
            # команда никогда не повторяется. Start отдельно ждёт список peers.
            before_send = not result.get('queued') and not result.get('outcome_unknown') and (
                result.get('error') == 'E_MP_BUSY' or (
                    wire == 'mp-ready' and result.get('error') == 'E_MP_NOT_READY'))
            if before_send and time.monotonic() < deadline:
                time.sleep(.2)
                continue
            raise RuntimeError(f'Команда не выполнена, повтор не отправлен: {wire}: {result}')

    def wait(self, environment, wire, predicate):
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            result = self.call(environment, wire, timeout=max(.001, deadline - time.monotonic()))
            if result.get('ok') and predicate(result):
                return result
            if result.get('error') in {'E_SESSION_POISONED', 'E_IPC_TIMEOUT', 'E_NATIVE_EXCEPTION',
                                       'E_MP_NATIVE_EXCEPTION', 'E_TIMEOUT', 'E_IPC'}:
                raise RuntimeError(f'Повреждённая сессия: {result}')
            time.sleep(.2)
        raise TimeoutError(f'Не подтверждено состояние: {wire}; изменяющая команда не повторяется')

    def bootstrap(self, save, host_tag, client_tag):
        self.record.update(ok=False, complete=False, stage='bootstrap')
        self.save()
        for environment in (self.host, self.client):
            self.wait(environment, 'mp-lobby', lambda state: state.get('ready'))
            self.send(environment, 'mp-open')
            self.wait(environment, 'mp-lobby', lambda state: state.get('active'))
        self.send(self.host, 'mp-host GpcHost GpcSession')
        self.wait(self.host, 'mp-front-state', lambda state: state.get('host'))
        self.send(self.client, 'mp-join GpcClient 127.0.0.1')
        self.wait(self.client, 'mp-front-state', lambda state: state.get('host') is False)
        self.send(self.host, 'mp-load ' + save)
        for environment in (self.host, self.client):
            self.wait(environment, 'mp-front-state', lambda state: state.get('loaded') and not state.get('loading'))
        for environment, tag in ((self.host, host_tag), (self.client, client_tag)):
            self.send(environment, 'mp-country ' + tag)
            self.wait(environment, 'mp-front-state', lambda state: state.get('country') == tag)
        self.send(self.host, 'mp-play')
        for environment in (self.host, self.client):
            self.wait(environment, 'status', lambda state: state.get('campaign_loaded'))
        # Не повторять Start после отправки: его результат подтверждается сетью.
        self.wait(self.host, 'mp-state', lambda state: state.get('multiplayer') and state.get('host') and state.get('can_start'))
        self.wait(self.client, 'mp-state', lambda state: state.get('multiplayer') and state.get('host') is False)
        self.send(self.host, 'mp-ready')
        for environment in (self.host, self.client):
            self.wait(environment, 'mp-state', lambda state: state.get('network_ready'))
        self.record.update(ok=True, complete=True)
        self.save()

    def speed(self, target):
        if not 0 <= target <= 4:
            raise ValueError('Скорость: 0..4')
        for _ in range(5):
            before = self.call(self.host, 'mp-state')
            if not before.get('ok'):
                raise RuntimeError(str(before))
            if before['speed'] == target:
                for environment in (self.host, self.client):
                    self.wait(environment, 'mp-state', lambda state: state.get('speed') == target and state.get('pending') is False)
                return
            result = self.send(self.host, 'mp-speed ' + str(target))
            expected = result['expected_speed']
            for environment in (self.host, self.client):
                self.wait(environment, 'mp-state', lambda state: state.get('speed') == expected and state.get('pending') is False)
        raise RuntimeError('Скорость не достигла цели')

    def pause(self, paused):
        self.send(self.host, 'mp-pause ' + ('1' if paused else '0'))
        for environment in (self.host, self.client):
            self.wait(environment, 'mp-state', lambda state: state.get('paused') is paused and state.get('pending') is False)

    def advance(self, days, speed=4):
        if not 1 <= days <= 10000:
            raise ValueError('Число дней: 1..10000')
        self.record.update(ok=False, complete=False, stage='advance')
        self.save()

        self.pause(True)
        before = self.call(self.host, 'mp-state')
        self.wait(self.client, 'mp-state', lambda state: state['date_raw'] == before['date_raw'] and state['paused'])
        self.speed(speed)
        target = before['date_raw'] + days * 24
        self.pause(False)
        self.wait(self.host, 'mp-state', lambda state: state['date_raw'] >= target)
        self.pause(True)
        final = self.wait(self.host, 'mp-state', lambda state: state.get('paused') is True and state.get('pending') is False and state.get('date_raw', 0) >= target)
        client = self.wait(self.client, 'mp-state', lambda state: state.get('paused') is True and state.get('pending') is False and state.get('date_raw') == final['date_raw'])
        self.record.update(ok=True, complete=True, requested_target=target, host_final=final, client_final=client)
        self.save()

    def save_campaign(self, name):
        if not re.fullmatch(r'[A-Za-z0-9_-]{1,15}', name):
            raise ValueError('Имя сохранения: 1..15 ASCII-букв, цифр, дефисов или подчёркиваний')
        output = self.host / 'userdata/save games' / (name + '.v2')
        if output.exists():
            raise FileExistsError('Сохранение уже существует')
        self.record.update(ok=False, complete=False, stage='save')
        self.save()
        self.pause(True)
        self.gpc_barrier()
        before = self.wait(self.host, 'mp-state', lambda state: state.get('paused') is True and state.get('pending') is False)
        self.wait(self.client, 'mp-state', lambda state: state.get('paused') is True and state.get('pending') is False and state.get('date_raw') == before['date_raw'])
        self.send(self.host, 'mp-save-menu')
        result = self.send(self.host, 'mp-save ' + name)
        if not result.get('completed') or not output.is_file() or not output.stat().st_size:
            raise RuntimeError('Не подтверждено завершённое сохранение')
        for environment in (self.host, self.client):
            self.wait(environment, 'mp-state', lambda state: state.get('paused') is True and state.get('pending') is False and state.get('date_raw') == before['date_raw'])
        self.record.update(ok=True, complete=True, saved={'path': str(output), 'sha256': cli.digest(output), 'size': output.stat().st_size})
        self.save()

    def gpc_barrier(self):
        states = {}
        for role, environment in (('host', self.host), ('client', self.client)):
            state = self.call(environment, 'culture-cores-status')
            if not state.get('ok'):
                raise RuntimeError(str(state))
            if state.get('installed') and state.get('enabled') and (
                    state.get('has_schedule') or 'requested_target' in self.record):
                date = self.call(environment, 'mp-state')['date_raw']
                state = self.wait(environment, 'culture-cores-status', lambda item: item.get('failed') or (
                    item.get('busy') == 0 and item.get('has_schedule') == 1 and item.get('completed_raw_date') == date))
                if state.get('failed'):
                    raise RuntimeError('Нативный GPC-модуль остановлен после ошибки')
            states[role] = state
        compared = ('installed', 'enabled', 'failed', 'busy', 'has_schedule', 'completed_raw_date',
                    'pass', 'rng', 'core_digest', 'state_digest')
        if any(states['host'].get(key) != states['client'].get(key) for key in compared):
            raise RuntimeError('Состояния GPC участников не совпали на завершённой фазе')
        self.record['gpc_barrier'] = states
        self.save()
        return states

    def collect_logs(self):
        """Сохраняет диагностику до следующего запуска, который ротирует журналы."""
        artifacts = self.output.parent / (self.output.stem + '-logs')
        artifacts.mkdir(exist_ok=False)
        records = {}
        for role, environment in (('host', self.host), ('client', self.client)):
            for filename in ('v2dll.log', 'v2dll_oos.log'):
                source = environment / 'game/z_CreatineCore/Logs' / filename
                if not source.is_file():
                    continue
                target = artifacts / (role + '-' + filename)
                if filename == 'v2dll.log':
                    # Ядро дописывает общий лог, сохраняя прежние PID и смешанные
                    # CP1251/UTF-8 сообщения. Проверяем ASCII-протокол текущей
                    # сессии; оригинальные байты остаются отдельным артефактом.
                    shutil.copy2(source, artifacts / (role + '-v2dll.full.log'))
                    session = json.loads((environment / 'session.json').read_text(encoding='utf-8'))
                    prefix = ('[' + str(session['pid']) + ']').encode('ascii')
                    selected = b''.join(line for line in source.read_bytes().splitlines(keepends=True) if line.startswith(prefix))
                    target.write_text(selected.decode('utf-8', errors='replace'), encoding='utf-8')
                else:
                    shutil.copy2(source, target)
                if filename == 'v2dll_oos.log':
                    text = target.read_text(encoding='utf-8', errors='replace')
                    sync = re.findall(r'SYNC n=(\d+) date=(.*?) sum=(\d+)/(\d+)', text)
                    records[role] = {'records': len(sync), 'mismatches': sum(a != b for _, _, a, b in sync),
                                     'first_real_oos': bool(re.search(r'first_real=(?!-)', text)),
                                     'last': sync[-1] if sync else None}
        self.record['native_sync'] = records
        self.record['logs'] = str(artifacts)
        from mp_logcheck import compare_logs
        proof = compare_logs(artifacts / 'host-v2dll.log', artifacts / 'client-v2dll.log',
                             artifacts / 'host-v2dll_oos.log', artifacts / 'client-v2dll_oos.log',
                             require_native_sync='requested_target' in self.record)
        self.record['log_proof'] = proof
        if not proof['ok']:
            self.record.update(ok=False, complete=False)
            self.save()
            raise RuntimeError('Не подтверждено совпадение сетевых журналов: ' + str(proof))
        self.save()



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', type=Path, required=True)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--save', required=True)
    parser.add_argument('--host-tag', default='AUS')
    parser.add_argument('--client-tag', default='NGF')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--start', action='store_true', help='Запустить обе подготовленные среды скрытыми; завершить после сценария')
    parser.add_argument('--leave-running', action='store_true', help='Оставить созданные процессы для дальнейшей диагностики')
    parser.add_argument('--days', type=int, default=0, help='После старта лобби пройти указанное число дней')
    parser.add_argument('--speed', type=int, choices=range(5), default=4)
    parser.add_argument('--save-result', help='Сохранить кампанию хостом после прогона')
    args = parser.parse_args()
    driver = Driver(args.host, args.client, args.output, args.timeout)
    owned = []
    try:
        from mp_preflight import compare
        preflight = compare(driver.host, driver.client)
        driver.record['preflight'] = preflight
        driver.save()
        if not preflight.get('ok'):
            raise ValueError('Игровые данные или конфигурации участников различаются')
        if args.start:
            from mp_session import OwnedProcess
            for environment in (driver.host, driver.client):
                started = cli.start(environment, hidden=True)
                if not started.get('ok'):
                    raise RuntimeError(str(started))
                owned.append(OwnedProcess(started['pid'], environment / 'game/v2game.exe'))
                driver.record.setdefault('started', []).append(started)
                driver.save()
        driver.bootstrap(args.save, args.host_tag, args.client_tag)
        if args.days:
            driver.advance(args.days, args.speed)
        if args.save_result:
            driver.save_campaign(args.save_result)
        driver.gpc_barrier()
        driver.collect_logs()
        for process in owned:
            cli.hide_windows(process.pid)
    except Exception as error:
        driver.record.update(ok=False, complete=False, error=str(error))
        driver.save()
        raise
    finally:
        cleanup_errors = []
        for process in reversed(owned):
            try:
                process.release() if args.leave_running else process.stop()
            except Exception as error:
                cleanup_errors.append(str(error))
        if cleanup_errors:
            driver.record.update(ok=False, complete=False, cleanup_errors=cleanup_errors)
            driver.save()
            raise RuntimeError('; '.join(cleanup_errors))
    print(json.dumps({'ok': True, 'report': str(args.output)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
