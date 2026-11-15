"""Дневной барьер GPC: ранняя остановка календаря и поздний callback, без игры."""
from copy import deepcopy
from pathlib import Path
import unittest
from unittest.mock import patch

import cli


RAW_DATE = 60365280
ENVIRONMENT = Path('fake-isolated-environment')


class Clock:
    def __init__(self):
        self.now = 0.0
        self.sleeps = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        if seconds < 0:
            raise AssertionError('Отрицательное ожидание')
        self.sleeps.append((self.now, seconds))
        self.now += seconds


def module_state(**changes):
    state = {'ok': True, 'installed': True, 'enabled': 1, 'failed': 0,
             'busy': 0, 'has_schedule': 1, 'completed_raw_date': RAW_DATE,
             'callback_raw_date': RAW_DATE}
    state.update(changes)
    return state


def game_state(**changes):
    state = {'ok': True, 'campaign_loaded': True, 'paused': True, 'date_raw': RAW_DATE}
    state.update(changes)
    return state


class ReadOnlyIPC:
    """RPC занимает время, а состояние мира меняется независимо от числа опросов."""
    def __init__(self, clock, module=None, game=None, duration=0.0):
        self.clock = clock
        self.module = module if module is not None else module_state()
        self.game = game if game is not None else game_state()
        self.duration = duration
        self.calls = []

    def __call__(self, environment, command, timeout):
        if environment != ENVIRONMENT:
            raise AssertionError('Опрос другой тестовой среды')
        if command not in ('culture-cores-status', 'status'):
            raise AssertionError('Барьер отправил изменяющую команду: ' + command)
        if timeout <= 0:
            raise AssertionError('Неположительный срок RPC')
        started = self.clock.now
        duration = self.duration(command, len(self.calls)) if callable(self.duration) else self.duration
        self.clock.now += min(duration, timeout)
        if duration > timeout:
            response = {'ok': False, 'error': 'E_IPC_TIMEOUT'}
        else:
            source = self.module if command == 'culture-cores-status' else self.game
            response = source(self.clock.now) if callable(source) else source
        response = deepcopy(response)
        self.calls.append({'command': command, 'timeout': timeout, 'started': started,
                           'finished': self.clock.now, 'response': response})
        return response


class GpcPhaseTests(unittest.TestCase):
    def wait(self, ipc, timeout=1.0):
        with patch.object(cli, 'request', side_effect=ipc), \
                patch.object(cli.time, 'monotonic', ipc.clock.monotonic), \
                patch.object(cli.time, 'sleep', ipc.clock.sleep):
            return cli.wait_gpc_phase(ENVIRONMENT, timeout)

    def assert_only_reads(self, ipc):
        self.assertTrue(all(call['command'] in ('culture-cores-status', 'status') for call in ipc.calls))

    def test_missing_or_disabled_module_does_not_read_game_status(self):
        for module in ({'ok': True, 'installed': False},
                       {'ok': True, 'installed': True, 'enabled': 0, 'failed': 0}):
            with self.subTest(module=module):
                clock = Clock()
                ipc = ReadOnlyIPC(clock, module=module,
                                  game=lambda _: self.fail('Для выключенной механики status не нужен'))
                self.assertEqual(self.wait(ipc), {'ok': True, 'gpc_phase_required': False})
                self.assertEqual([call['command'] for call in ipc.calls], ['culture-cores-status'])
                self.assertEqual(clock.sleeps, [])

    def test_calendar_is_already_paused_but_callback_completes_later(self):
        clock = Clock()

        def late_phase(now):
            # Реальный проблемный порядок: дата уже остановлена; OnDay ещё
            # даже не начался, поэтому busy=0 недостаточно для готовности.
            if now < .25:
                return module_state(completed_raw_date=0, callback_raw_date=0)
            if now < .65:
                return module_state(busy=1, completed_raw_date=RAW_DATE - 24)
            return module_state()

        ipc = ReadOnlyIPC(clock, module=late_phase, duration=.03)
        result = self.wait(ipc, timeout=1.5)
        self.assertTrue(result['ok'])
        self.assertTrue(result['gpc_phase_required'])
        self.assertEqual(result['gpc']['completed_raw_date'], RAW_DATE)
        phases = [call for call in ipc.calls if call['command'] == 'culture-cores-status']
        self.assertGreaterEqual(len(phases), 3)
        self.assertEqual(phases[0]['response']['busy'], 0)
        self.assertEqual(phases[0]['response']['has_schedule'], 1)
        self.assertEqual(phases[0]['response']['completed_raw_date'], 0)
        self.assertTrue(any(call['response']['busy'] == 1 for call in phases))
        self.assertGreaterEqual(clock.now, .65)
        self.assertTrue(all(call['response']['date_raw'] == RAW_DATE and call['response']['paused'] is True
                            for call in ipc.calls if call['command'] == 'status'))
        self.assert_only_reads(ipc)

    def test_runtime_failure_is_a_hard_error_without_game_poll(self):
        for enabled in (0, 1):
            with self.subTest(enabled=enabled):
                # Ошибка Init после enabled=0 не является успешно выключенной
                # механикой: failed обязателен и проверяется раньше disabled.
                ipc = ReadOnlyIPC(Clock(), module=module_state(enabled=enabled, failed=1))
                result = self.wait(ipc)
                self.assertEqual(result['error'], 'E_GPC_FAILED')
                self.assertFalse(result['ok'])
                self.assertEqual([call['command'] for call in ipc.calls], ['culture-cores-status'])

    def test_not_in_campaign_is_a_hard_error(self):
        ipc = ReadOnlyIPC(Clock(), game=game_state(campaign_loaded=False))
        self.assertEqual(self.wait(ipc), {'ok': False, 'error': 'E_NOT_READY'})
        self.assertEqual(len(ipc.calls), 2)
        self.assert_only_reads(ipc)

    def test_incomplete_or_busy_state_never_satisfies_the_barrier(self):
        cases = [
            (module_state(busy=1), game_state()),
            (module_state(has_schedule=0), game_state()),
            (module_state(completed_raw_date=RAW_DATE - 24), game_state()),
            (module_state(callback_raw_date=RAW_DATE - 24), game_state()),
            (module_state(), game_state(paused=False)),
            (module_state(completed_raw_date=0, callback_raw_date=0), game_state()),
        ]
        for module, game in cases:
            with self.subTest(module=module, game=game):
                ipc = ReadOnlyIPC(Clock(), module, game)
                result = self.wait(ipc, timeout=.51)
                self.assertFalse(result['ok'])
                self.assertEqual(result['error'], 'E_GPC_PHASE_TIMEOUT')
                self.assertGreater(len(ipc.calls), 2)
                self.assert_only_reads(ipc)

    def test_missing_fields_cannot_be_mistaken_for_explicit_disabled_or_equal_dates(self):
        cases = []
        for field in ('installed', 'enabled', 'failed', 'busy', 'has_schedule',
                      'completed_raw_date', 'callback_raw_date'):
            module = module_state()
            del module[field]
            cases.append((field, module, game_state()))
        for field in ('campaign_loaded', 'paused', 'date_raw'):
            game = game_state()
            del game[field]
            cases.append((field, module_state(), game))
        module, game = module_state(), game_state()
        del module['completed_raw_date'], module['callback_raw_date'], game['date_raw']
        cases.append(('all_dates_absent', module, game))
        cases.append(('disabled_missing_failed', {'ok': True, 'installed': True, 'enabled': 0}, game_state()))
        for field, module, game in cases:
            with self.subTest(field=field):
                ipc = ReadOnlyIPC(Clock(), module, game)
                self.assertFalse(self.wait(ipc, timeout=.41).get('ok', False))
                self.assert_only_reads(ipc)

    def test_rpc_errors_propagate_without_retrying(self):
        errors = [{'ok': False, 'error': 'E_IPC_TIMEOUT'},
                  {'ok': False, 'error': 'E_NATIVE_EXCEPTION'}]
        for error in errors:
            for command in ('culture-cores-status', 'status'):
                with self.subTest(error=error, command=command):
                    kwargs = {'module' if command == 'culture-cores-status' else 'game': error}
                    ipc = ReadOnlyIPC(Clock(), **kwargs)
                    self.assertEqual(self.wait(ipc), error)
                    self.assertEqual(len(ipc.calls), 1 if command == 'culture-cores-status' else 2)
                    self.assertEqual(ipc.clock.sleeps, [])
                    self.assert_only_reads(ipc)

    def test_each_rpc_uses_remaining_total_deadline(self):
        clock = Clock()
        ipc = ReadOnlyIPC(clock, module=module_state(busy=1), duration=.03)
        timeout = .7
        self.assertFalse(self.wait(ipc, timeout)['ok'])
        self.assertGreater(len(ipc.calls), 2)
        for call in ipc.calls:
            self.assertGreater(timeout - call['started'], 0)
            self.assertLessEqual(call['timeout'], timeout - call['started'] + 1e-9)
        self.assertLess(ipc.calls[1]['timeout'], ipc.calls[0]['timeout'])
        self.assertLess(ipc.calls[-1]['timeout'], ipc.calls[1]['timeout'])
        self.assert_only_reads(ipc)

    def test_deadline_exhausted_by_one_rpc_does_not_start_another_rpc(self):
        clock = Clock()
        ipc = ReadOnlyIPC(clock, duration=lambda command, _: .5 if command == 'culture-cores-status' else 0)
        result = self.wait(ipc, timeout=.5)
        self.assertFalse(result['ok'])
        self.assertEqual(result['error'], 'E_GPC_PHASE_TIMEOUT')
        self.assertEqual([call['command'] for call in ipc.calls], ['culture-cores-status'])
        self.assert_only_reads(ipc)

    def test_poll_sleep_is_bounded_by_remaining_deadline(self):
        clock = Clock()
        ipc = ReadOnlyIPC(clock, module=module_state(busy=1))
        timeout = .05
        result = self.wait(ipc, timeout)
        self.assertEqual(result['error'], 'E_GPC_PHASE_TIMEOUT')
        self.assertLessEqual(clock.now, timeout + 1e-9)
        self.assertTrue(all(delay <= timeout - started + 1e-9 for started, delay in clock.sleeps))
        self.assert_only_reads(ipc)

    def test_nonpositive_timeout_does_not_send_any_request(self):
        for timeout in (0, -1):
            with self.subTest(timeout=timeout):
                ipc = ReadOnlyIPC(Clock())
                self.assertEqual(self.wait(ipc, timeout)['error'], 'E_GPC_PHASE_TIMEOUT')
                self.assertEqual(ipc.calls, [])
                self.assertEqual(ipc.clock.sleeps, [])


if __name__ == '__main__':
    unittest.main()
