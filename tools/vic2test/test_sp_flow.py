"""SP-переходы через нативные ACK: моделируем только IPC и время, без игры."""
from copy import deepcopy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import cli


TRANSITIONS = ('sp-return-resign', 'sp-return-confirm', 'sp-return-exit')
FIELDS = ('confirmation', 'summary', 'main_menu')


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


class ScriptedIPC:
    """Один ожидаемый запрос на шаг; неожиданный повтор сразу ломает тест."""
    def __init__(self, environment, clock, steps):
        self.environment, self.clock = environment, clock
        self.steps = list(steps)
        self.calls = []

    def __call__(self, environment, command, timeout):
        if environment != self.environment or not self.steps:
            raise AssertionError('Лишний запрос: ' + command)
        expected, response, duration = self.steps.pop(0)
        if command != expected:
            raise AssertionError(f'Ожидался {expected}, получен {command}')
        if timeout <= 0:
            raise AssertionError('Неположительный срок IPC')
        started = self.clock.now
        self.clock.now += min(duration, timeout)
        actual = response if duration <= timeout else {'ok': False, 'error': 'E_IPC_TIMEOUT'}
        self.calls.append({'command': command, 'started': started, 'timeout': timeout,
                           'response': deepcopy(actual)})
        return deepcopy(actual)


class ReturnModel:
    def __init__(self, environment, clock, immediate=False):
        self.environment, self.clock, self.immediate = environment, clock, immediate
        self.stage = -1
        self.acknowledged = -1
        self.due = None
        self.terminal_ack = False
        self.calls = []

    def __call__(self, environment, command, timeout):
        if environment != self.environment or timeout <= 0 or self.terminal_ack:
            raise AssertionError('Лишний запрос после конечного ACK: ' + command)
        started = self.clock.now
        self.clock.now += min(.01, timeout)
        if self.due is not None and self.clock.now >= self.due:
            self.acknowledged = self.stage
        response = {'ok': True}
        if command == 'pause':
            response['paused'] = True
        elif command in TRANSITIONS:
            target = TRANSITIONS.index(command)
            if target != self.stage + 1 or (target and self.acknowledged != target - 1):
                raise AssertionError('Переход отправлен до ACK предыдущего шага')
            self.stage = target
            self.due = self.clock.now if self.immediate else self.clock.now + .35
            if self.immediate:
                self.acknowledged = target
                response['already'] = True
        elif command != 'sp-return-state':
            raise AssertionError('Ожидание использует неверную команду: ' + command)
        for index, field in enumerate(FIELDS):
            response[field] = self.acknowledged == index
        if response['main_menu']:
            self.terminal_ack = True
        self.calls.append({'command': command, 'started': started, 'timeout': timeout,
                           'response': deepcopy(response)})
        return response


class LoadModel:
    def __init__(self, environment, clock, basename):
        self.environment, self.clock, self.basename = environment, clock, basename
        self.opened = False
        self.load_due = None
        self.play_due = None
        self.paused = False
        self.calls = []

    def __call__(self, environment, command, timeout):
        if environment != self.environment or timeout <= 0 or self.paused:
            raise AssertionError('Лишний запрос после подтверждения паузы: ' + command)
        started = self.clock.now
        self.clock.now += min(.01, timeout)
        loaded = self.load_due is not None and self.clock.now >= self.load_due
        campaign = self.play_due is not None and self.clock.now >= self.play_due
        response = {'ok': True}
        if command == 'status':
            response.update(campaign_loaded=campaign, paused=False)
        elif command == 'open-single-player':
            if self.opened:
                raise AssertionError('Повторное открытие SP')
            self.opened = True
        elif command == 'load-save ' + self.basename:
            if not self.opened or self.load_due is not None:
                raise AssertionError('Неверная или повторная загрузка')
            self.load_due = self.clock.now + .45
            response.update(load_pending=True, save_loaded=False)
        elif command == 'frontend-status':
            if self.load_due is None:
                raise AssertionError('Статус загрузки запрошен до загрузки')
            response.update(load_pending=not loaded, save_loaded=loaded)
        elif command == 'play':
            if not loaded or self.play_due is not None:
                raise AssertionError('Лишний или преждевременный Play')
            self.play_due = self.clock.now + .4
            response['queued'] = True
        elif command == 'pause':
            if not campaign:
                raise AssertionError('Пауза до ACK загрузки кампании')
            self.paused = True
            response['paused'] = True
        else:
            raise AssertionError('Неожиданная команда загрузки: ' + command)
        self.calls.append({'command': command, 'started': started, 'timeout': timeout,
                           'response': deepcopy(response)})
        return response


class SpFlowTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='vic2-sp-flow-test-')
        self.addCleanup(temporary.cleanup)
        self.environment = Path(temporary.name)
        self.userdata = self.environment / 'userdata'
        (self.userdata / 'save games').mkdir(parents=True)
        self.basename = 'sample.v2'
        (self.userdata / 'save games' / self.basename).write_bytes(b'fake save, never loaded by a game')
        (self.environment / 'environment.json').write_text(
            json.dumps({'userdata': str(self.userdata)}), encoding='utf-8')

    def execute(self, function, ipc, *args, phase=None, **kwargs):
        with patch.object(cli, 'request', side_effect=ipc), \
                patch.object(cli.time, 'monotonic', ipc.clock.monotonic), \
                patch.object(cli.time, 'sleep', ipc.clock.sleep), \
                patch.object(cli, 'wait_gpc_phase', return_value=(phase if phase is not None else {'ok': True})) as barrier:
            result = function(self.environment, *args, **kwargs)
        return result, barrier

    def test_return_sends_three_transitions_once_and_waits_only_with_readonly_state(self):
        ipc = ReturnModel(self.environment, Clock())
        result, barrier = self.execute(cli.return_to_menu, ipc, timeout=3)
        self.assertTrue(result['ok'])
        self.assertTrue(result['campaign_discarded'])
        self.assertTrue(result['main_menu'])
        commands = [call['command'] for call in ipc.calls]
        self.assertEqual(commands[0], 'pause')
        self.assertEqual([command for command in commands if command in TRANSITIONS], list(TRANSITIONS))
        self.assertTrue(all(command in ('pause', 'sp-return-state', *TRANSITIONS) for command in commands))
        self.assertGreater(commands.count('sp-return-state'), 3)
        self.assertEqual(commands[-1], 'sp-return-state')
        self.assertTrue(ipc.terminal_ack)
        barrier.assert_called_once()

    def test_immediate_acks_do_not_trigger_extra_state_polls(self):
        ipc = ReturnModel(self.environment, Clock(), immediate=True)
        result, _ = self.execute(cli.return_to_menu, ipc, timeout=1)
        self.assertTrue(result['main_menu'])
        self.assertTrue(result['already'])
        self.assertEqual([call['command'] for call in ipc.calls], ['pause', *TRANSITIONS])
        self.assertEqual(ipc.clock.sleeps, [])

    def test_each_transition_error_is_returned_without_repeating_the_action(self):
        for failed_index in range(3):
            for error in ('E_IPC_TIMEOUT', 'E_NATIVE_EXCEPTION'):
                with self.subTest(command=TRANSITIONS[failed_index], error=error):
                    steps = [('pause', {'ok': True}, 0)]
                    steps.extend((command, {'ok': True, FIELDS[index]: True}, 0)
                                 for index, command in enumerate(TRANSITIONS[:failed_index]))
                    failure = {'ok': False, 'error': error}
                    steps.append((TRANSITIONS[failed_index], failure, 0))
                    ipc = ScriptedIPC(self.environment, Clock(), steps)
                    result, _ = self.execute(cli.return_to_menu, ipc, timeout=1)
                    self.assertEqual(result, failure)
                    self.assertEqual(ipc.steps, [])

    def test_error_while_polling_never_resends_resign(self):
        error = {'ok': False, 'error': 'E_IPC_TIMEOUT'}
        ipc = ScriptedIPC(self.environment, Clock(), [
            ('pause', {'ok': True}, 0), ('sp-return-resign', {'ok': True, 'queued': True}, 0),
            ('sp-return-state', error, 0)])
        result, _ = self.execute(cli.return_to_menu, ipc, timeout=1)
        self.assertEqual(result, error)
        self.assertEqual([call['command'] for call in ipc.calls].count('sp-return-resign'), 1)

    def test_return_timeout_does_not_resend_or_move_to_next_transition(self):
        ipc = ScriptedIPC(self.environment, Clock(), [
            ('pause', {'ok': True}, 0), ('sp-return-resign', {'ok': True, 'queued': True}, 0)])
        result, _ = self.execute(cli.return_to_menu, ipc, timeout=.05)
        self.assertEqual(result['error'], 'E_SP_TRANSITION_TIMEOUT')
        self.assertEqual([call['command'] for call in ipc.calls], ['pause', 'sp-return-resign'])
        self.assertLessEqual(ipc.clock.now, .05 + 1e-9)

    def test_failed_gpc_prevents_resign(self):
        ipc = ScriptedIPC(self.environment, Clock(), [('pause', {'ok': True}, 0)])
        failure = {'ok': False, 'error': 'E_GPC_FAILED'}
        result, barrier = self.execute(cli.return_to_menu, ipc, timeout=1, phase=failure)
        self.assertEqual(result, failure)
        self.assertEqual([call['command'] for call in ipc.calls], ['pause'])
        barrier.assert_called_once()

    def test_failed_pause_does_not_call_phase_or_resign(self):
        failure = {'ok': False, 'error': 'E_IPC_TIMEOUT'}
        ipc = ScriptedIPC(self.environment, Clock(), [('pause', failure, 0)])
        result, barrier = self.execute(cli.return_to_menu, ipc, timeout=1)
        self.assertEqual(result, failure)
        barrier.assert_not_called()

    def test_load_waits_for_both_asynchronous_acks_then_pauses_once(self):
        ipc = LoadModel(self.environment, Clock(), self.basename)
        result, barrier = self.execute(cli.load_campaign, ipc, self.basename, timeout=3)
        self.assertTrue(result['ok'])
        self.assertTrue(result['campaign_loaded'])
        self.assertTrue(result['paused'])
        commands = [call['command'] for call in ipc.calls]
        self.assertEqual(commands.count('open-single-player'), 1)
        self.assertEqual(commands.count('load-save ' + self.basename), 1)
        self.assertEqual(commands.count('play'), 1)
        self.assertEqual(commands.count('pause'), 1)
        self.assertGreater(commands.count('frontend-status'), 1)
        play_index = commands.index('play')
        self.assertTrue(ipc.calls[play_index - 1]['response']['save_loaded'])
        self.assertTrue(ipc.calls[-2]['response']['campaign_loaded'])
        self.assertEqual(commands[-1], 'pause')
        barrier.assert_not_called()

    def test_load_refuses_an_existing_campaign_without_open_or_load(self):
        ipc = ScriptedIPC(self.environment, Clock(), [('status', {'ok': True, 'campaign_loaded': True}, 0)])
        with self.assertRaisesRegex(ValueError, 'Сначала вернитесь'):
            self.execute(cli.load_campaign, ipc, self.basename, timeout=1)
        self.assertEqual([call['command'] for call in ipc.calls], ['status'])

    def test_load_rejects_bad_basename_before_ipc(self):
        directory = self.userdata / 'save games' / 'directory.v2'
        directory.mkdir()
        for name in ('', '../sample.v2', 'folder/sample.v2', 'folder\\sample.v2',
                     str(self.userdata / 'save games' / self.basename), 'missing.v2', 'directory.v2'):
            with self.subTest(name=name):
                ipc = ScriptedIPC(self.environment, Clock(), [])
                with self.assertRaises(ValueError):
                    self.execute(cli.load_campaign, ipc, name, timeout=1)
                self.assertEqual(ipc.calls, [])

    def load_steps(self):
        return [('status', {'ok': True, 'campaign_loaded': False}, 0),
                ('open-single-player', {'ok': True}, 0),
                ('load-save ' + self.basename, {'ok': True}, 0),
                ('frontend-status', {'ok': True, 'save_loaded': True}, 0),
                ('play', {'ok': True}, 0),
                ('status', {'ok': True, 'campaign_loaded': True, 'paused': False}, 0),
                ('pause', {'ok': True, 'paused': True}, 0)]

    def test_error_at_any_load_stage_prevents_every_following_mutation(self):
        for failed_index in range(7):
            with self.subTest(stage=failed_index):
                steps = self.load_steps()[:failed_index + 1]
                command = steps[-1][0]
                failure = {'ok': False, 'error': 'E_IPC_TIMEOUT'}
                steps[-1] = (command, failure, 0)
                ipc = ScriptedIPC(self.environment, Clock(), steps)
                result, _ = self.execute(cli.load_campaign, ipc, self.basename, timeout=1)
                self.assertFalse(result['ok'])
                self.assertEqual(result['error'], failure['error'])
                self.assertEqual(ipc.steps, [])
                self.assertEqual([call['command'] for call in ipc.calls].count('play'), 1 if failed_index >= 4 else 0)
                if failed_index == 0:
                    self.assertEqual([call['command'] for call in ipc.calls], ['status'])
                if failed_index == 6:
                    self.assertTrue(result['campaign_loaded'])
                    self.assertIsNot(result.get('paused'), True)

    def test_load_deadline_expiring_in_initial_status_does_not_open_sp(self):
        ipc = ScriptedIPC(self.environment, Clock(), [('status', {'ok': True, 'campaign_loaded': False}, .5)])
        result, _ = self.execute(cli.load_campaign, ipc, self.basename, timeout=.5)
        self.assertEqual(result['error'], 'E_LOAD_TIMEOUT')
        self.assertEqual([call['command'] for call in ipc.calls], ['status'])

    def test_save_loaded_ack_at_deadline_does_not_send_play(self):
        steps = self.load_steps()[:4]
        steps[-1] = ('frontend-status', {'ok': True, 'save_loaded': True}, .5)
        ipc = ScriptedIPC(self.environment, Clock(), steps)
        result, _ = self.execute(cli.load_campaign, ipc, self.basename, timeout=.5)
        self.assertEqual(result['error'], 'E_LOAD_TIMEOUT')
        self.assertNotIn('play', [call['command'] for call in ipc.calls])

    def test_deadline_after_play_never_repeats_play_or_sends_pause(self):
        steps = self.load_steps()[:5]
        steps[-1] = ('play', {'ok': True, 'queued': True}, .5)
        ipc = ScriptedIPC(self.environment, Clock(), steps)
        result, _ = self.execute(cli.load_campaign, ipc, self.basename, timeout=.5)
        self.assertEqual(result['error'], 'E_START_TIMEOUT')
        self.assertEqual([call['command'] for call in ipc.calls].count('play'), 1)
        self.assertNotIn('pause', [call['command'] for call in ipc.calls])

    def test_campaign_loaded_reply_at_deadline_does_not_send_pause(self):
        steps = self.load_steps()[:6]
        steps[-1] = ('status', {'ok': True, 'campaign_loaded': True}, .5)
        ipc = ScriptedIPC(self.environment, Clock(), steps)
        result, _ = self.execute(cli.load_campaign, ipc, self.basename, timeout=.5)
        self.assertFalse(result['ok'])
        self.assertEqual(result['error'], 'E_START_TIMEOUT')
        self.assertNotIn('pause', [call['command'] for call in ipc.calls])

    def test_nonpositive_timeouts_do_not_send_actions(self):
        for function, args in ((cli.return_to_menu, ()), (cli.load_campaign, (self.basename,))):
            for timeout in (0, -1):
                with self.subTest(function=function.__name__, timeout=timeout):
                    ipc = ScriptedIPC(self.environment, Clock(), [])
                    with self.assertRaises(ValueError):
                        self.execute(function, ipc, *args, timeout=timeout)
                    self.assertEqual(ipc.calls, [])


if __name__ == '__main__':
    unittest.main()
