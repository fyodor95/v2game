"""Проверки сетевого контроллера на моделируемом IPC, без запуска игры."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import cli
import mp_driver


class Clock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Network:
    """Модель двух участников; queued и подтверждённое состояние разделены."""
    def __init__(self, host, client):
        self.host, self.client = host, client
        self.active = {host: False, client: False}
        self.hosted = False
        self.joined = False
        self.loaded = False
        self.started = False
        self.network_ready = False
        self.countries = {host: None, client: None}
        self.date = 1000
        self.paused = True
        self.speed = 4
        self.client_ready_lag = 1
        self.freeze = False
        self.missing_client_role = False
        self.exchanges = []

    def __call__(self, environment, wire, timeout):
        result = self.respond(environment, wire)
        self.exchanges.append((environment, wire, timeout, dict(result)))
        return result

    def respond(self, environment, wire):
        if wire == 'mp-lobby':
            return {'ok': True, 'ready': True, 'active': self.active[environment]}
        if wire == 'mp-open':
            self.active[environment] = True
            return {'ok': True, 'queued': True}
        if wire.startswith('mp-host '):
            self.hosted = True
            return {'ok': True, 'queued': True}
        if wire.startswith('mp-join '):
            self.joined = True
            return {'ok': True, 'queued': True}
        if wire == 'mp-front-state':
            if environment == self.client and self.missing_client_role:
                self.missing_client_role = False
                return {'ok': True}
            return {'ok': True, 'host': environment == self.host, 'loaded': self.loaded,
                    'loading': False, 'country': self.countries[environment]}
        if wire.startswith('mp-load '):
            self.loaded = True
            return {'ok': True, 'queued': True}
        if wire.startswith('mp-country '):
            self.countries[environment] = wire.split()[1]
            return {'ok': True, 'queued': True}
        if wire == 'mp-play':
            self.started = True
            return {'ok': True, 'queued': True}
        if wire == 'status':
            return {'ok': True, 'campaign_loaded': self.started}
        if wire == 'mp-ready':
            self.network_ready = True
            return {'ok': True, 'queued': True}
        if wire.startswith('mp-speed '):
            target = int(wire.split()[1])
            self.speed += 1 if self.speed < target else -1 if self.speed > target else 0
            return {'ok': True, 'queued': True, 'expected_speed': self.speed}
        if wire.startswith('mp-pause '):
            self.paused = wire.endswith('1')
            return {'ok': True, 'queued': True, 'expected_paused': self.paused}
        if wire == 'mp-state':
            if not self.paused and not self.freeze and environment == self.host:
                self.date += 24
            ready = self.network_ready
            if ready and environment == self.client and self.client_ready_lag:
                self.client_ready_lag -= 1
                ready = False
            return {'ok': True, 'multiplayer': self.started, 'host': environment == self.host,
                    'can_start': self.started and self.hosted and self.joined,
                    'network_ready': ready, 'pending': False, 'paused': self.paused,
                    'speed': self.speed, 'date_raw': self.date}
        raise AssertionError(f'Неожиданная команда: {wire}')


class DriverTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.host = self.root / '.work/host'
        self.client = self.root / '.work/client'
        for environment in (self.host, self.client):
            (environment / 'game').mkdir(parents=True)
            (environment / 'environment.json').write_text(
                json.dumps({'game': str(environment / 'game')}), encoding='utf-8')
        self.addCleanup(patch.stopall)
        patch.object(cli, 'ROOT', self.root).start()
        self.clock = Clock()
        patch.object(mp_driver.time, 'monotonic', self.clock.monotonic).start()
        patch.object(mp_driver.time, 'sleep', self.clock.sleep).start()
        self.driver = mp_driver.Driver(self.host, self.client, self.root / 'report.json', timeout=1)

    def exchange(self, side_effect):
        return patch.object(cli, 'request', side_effect=side_effect)

    def test_queued_command_is_sent_once_and_not_declared_complete(self):
        queued = {'ok': True, 'queued': True, 'completed': False}
        with self.exchange([queued]) as ipc:
            self.assertEqual(self.driver.send(self.host, 'mp-play'), queued)
        self.assertEqual(ipc.call_count, 1)
        self.assertFalse(self.driver.record['complete'])

    def test_unknown_result_is_never_retried(self):
        for error in ('E_IPC_TIMEOUT', 'E_NATIVE_EXCEPTION', 'E_MP_NATIVE_EXCEPTION', 'E_TIMEOUT', 'E_IPC'):
            with self.subTest(error=error), self.exchange([{'ok': False, 'error': error}]) as ipc:
                with self.assertRaises(RuntimeError):
                    self.driver.send(self.host, 'mp-pause 1')
                self.assertEqual(ipc.call_count, 1)

    def test_explicit_queued_error_is_not_retried_even_if_busy(self):
        # Статус queued важнее retryable-кода: сервер мог принять операцию.
        with self.exchange([{'ok': False, 'error': 'E_MP_BUSY', 'queued': True}, {'ok': True}]) as ipc:
            with self.assertRaises(RuntimeError):
                self.driver.send(self.host, 'mp-play')
            self.assertEqual(ipc.call_count, 1)

    def test_proven_prequeue_busy_can_retry(self):
        with self.exchange([{'ok': False, 'error': 'E_MP_BUSY'}, {'ok': True, 'queued': True}]) as ipc:
            self.assertTrue(self.driver.send(self.host, 'mp-play')['queued'])
        self.assertEqual(ipc.call_count, 2)

    def test_send_uses_remaining_deadline_for_second_exchange(self):
        seen = []

        def request(environment, wire, timeout):
            seen.append(timeout)
            self.clock.now += .4
            return {'ok': False, 'error': 'E_MP_BUSY'} if len(seen) == 1 else {'ok': True}

        with self.exchange(request):
            self.driver.send(self.host, 'mp-play')
        self.assertLessEqual(seen[1], .401)

    def test_wait_uses_remaining_deadline(self):
        seen = []

        def request(environment, wire, timeout):
            seen.append(timeout)
            self.clock.now += .4
            return {'ok': True, 'ready': len(seen) > 1}

        with self.exchange(request):
            self.driver.wait(self.host, 'mp-state', lambda state: state['ready'])
        self.assertLessEqual(seen[1], .401)

    def test_wait_stops_immediately_on_all_poison_errors(self):
        for error in ('E_SESSION_POISONED', 'E_IPC_TIMEOUT', 'E_NATIVE_EXCEPTION',
                      'E_MP_NATIVE_EXCEPTION', 'E_TIMEOUT', 'E_IPC'):
            with self.subTest(error=error), self.exchange([{'ok': False, 'error': error}]) as ipc:
                with self.assertRaises(RuntimeError):
                    self.driver.wait(self.host, 'mp-state', lambda state: False)
                self.assertEqual(ipc.call_count, 1)

    def test_wait_timeout_does_not_resend_mutation(self):
        def request(environment, wire, timeout):
            self.assertEqual(wire, 'mp-state')
            return {'ok': True, 'network_ready': False}

        with self.exchange(request) as ipc:
            with self.assertRaises(TimeoutError):
                self.driver.wait(self.client, 'mp-state', lambda state: state.get('network_ready'))
        self.assertGreater(ipc.call_count, 1)
        self.assertFalse(self.driver.record['complete'])

    def test_speed_already_at_target_still_requires_client_ack(self):
        def request(environment, wire, timeout):
            self.assertEqual(wire, 'mp-state')
            if environment == self.client:
                raise RuntimeError('Клиент не подтвердил скорость')
            return {'ok': True, 'speed': 4, 'pending': False}

        with self.exchange(request):
            with self.assertRaisesRegex(RuntimeError, 'Клиент'):
                self.driver.speed(4)

    def test_bootstrap_waits_both_network_acks_and_sends_start_once(self):
        network = Network(self.host, self.client)
        with self.exchange(network):
            self.driver.bootstrap('fixture.v2', 'AUS', 'NGF')
        self.assertTrue(self.driver.record['complete'])
        commands = [(environment, wire) for environment, wire, _, _ in network.exchanges]
        self.assertEqual(commands.count((self.host, 'mp-play')), 1)
        self.assertEqual(commands.count((self.host, 'mp-ready')), 1)
        for environment in (self.host, self.client):
            self.assertTrue(any(env == environment and wire == 'mp-state' and reply.get('network_ready')
                                for env, wire, _, reply in network.exchanges))
        ready_index = next(i for i, (_, wire, _, _) in enumerate(network.exchanges) if wire == 'mp-ready')
        client_acks = [reply for env, wire, _, reply in network.exchanges[ready_index+1:]
                       if env == self.client and wire == 'mp-state']
        self.assertEqual([reply['network_ready'] for reply in client_acks], [False, True])

    def test_missing_client_role_does_not_confirm_join(self):
        network = Network(self.host, self.client)
        network.missing_client_role = True
        with self.exchange(network):
            self.driver.bootstrap('fixture.v2', 'AUS', 'NGF')
        load_index = next(i for i, (_, wire, _, _) in enumerate(network.exchanges) if wire.startswith('mp-load '))
        self.assertTrue(any(env == self.client and wire == 'mp-front-state' and reply.get('host') is False
                            for env, wire, _, reply in network.exchanges[:load_index]))

    def test_advance_complete_requires_paused_matching_peers_and_target_date(self):
        network = Network(self.host, self.client)
        network.started = network.network_ready = True
        network.client_ready_lag = 0
        with self.exchange(network):
            self.driver.advance(2)
        record = self.driver.record
        self.assertTrue(record['ok'] and record['complete'])
        self.assertEqual(record['requested_target'], 1048)
        self.assertEqual(record['host_final']['date_raw'], record['client_final']['date_raw'])
        self.assertGreaterEqual(record['host_final']['date_raw'], record['requested_target'])
        self.assertTrue(record['host_final']['paused'] and record['client_final']['paused'])
        self.assertFalse(record['host_final']['pending'] or record['client_final']['pending'])

    def test_failed_advance_invalidates_previous_bootstrap_completion(self):
        network = Network(self.host, self.client)
        with self.exchange(network):
            self.driver.bootstrap('fixture.v2', 'AUS', 'NGF')
        self.assertTrue(self.driver.record['complete'])
        network.freeze = True
        with self.exchange(network), self.assertRaises(TimeoutError):
            self.driver.advance(1)
        self.assertFalse(self.driver.record['complete'])
        self.assertFalse(self.driver.record['ok'])

    def test_final_host_state_cannot_be_unpaused_or_pending(self):
        network = Network(self.host, self.client)
        network.started = network.network_ready = True
        pause_commands = 0
        final_host_reads = 0

        def request(environment, wire, timeout):
            nonlocal pause_commands, final_host_reads
            if wire.startswith('mp-pause '):
                pause_commands += 1
            result = network(environment, wire, timeout)
            if pause_commands >= 3 and environment == self.host and wire == 'mp-state':
                final_host_reads += 1
                if final_host_reads >= 2:
                    result.update(paused=False, pending=True)
            return result

        with self.exchange(request), self.assertRaises((RuntimeError, TimeoutError)):
            self.driver.advance(1)
        self.assertFalse(self.driver.record['complete'])

    def test_pause_ack_requires_explicit_pending_false(self):
        def request(environment, wire, timeout):
            if wire == 'mp-pause 1':
                return {'ok': True, 'queued': True}
            self.assertEqual(wire, 'mp-state')
            return {'ok': True, 'paused': True}

        with self.exchange(request), self.assertRaises(TimeoutError):
            self.driver.pause(True)


if __name__ == '__main__':
    unittest.main()
