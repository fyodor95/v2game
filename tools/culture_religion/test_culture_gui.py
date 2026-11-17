"""Проверка отказов GUI-сценария без Win32, ввода и запуска игры."""
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from tools.culture_religion import culture_gui as runner, gui


class Desktop:
    def __init__(self):
        self.foreground = 1
        self.hit = 1
        self.roots = {1: 1, 2: 2, 3: 1}
        self.pids = {1: 77, 2: 99, 3: 77}
        self.cursor = (0, 0)
        self.events = []
        self.hit_calls = 0
        self.overlay_after = None

    def GetForegroundWindow(self): return self.foreground
    def GetWindowThreadProcessId(self, window, pointer): pointer._obj.value = self.pids.get(window, 0)
    def GetClassNameW(self, window, buffer, size): buffer.value = 'Victoria 2'
    def GetClientRect(self, window, pointer):
        pointer._obj.right, pointer._obj.bottom = 1366, 768
        return True
    def ClientToScreen(self, window, pointer):
        pointer._obj.x += 100
        pointer._obj.y += 100
        return True
    def WindowFromPoint(self, point):
        self.hit_calls += 1
        return 2 if self.overlay_after and self.hit_calls >= self.overlay_after else self.hit
    def GetAncestor(self, window, flags): return self.roots.get(window, 0)
    def SetCursorPos(self, x, y): self.cursor = x, y; return True
    def GetCursorPos(self, pointer):
        pointer._obj.x, pointer._obj.y = self.cursor
        return True
    def GetSystemMetrics(self, metric): return (3200, 2000)[metric]
    def mouse_event(self, *args): self.events.append(('mouse', *args))
    def keybd_event(self, *args): self.events.append(('key', *args))


class InputGuardsTests(unittest.TestCase):
    def setUp(self):
        self.desktop = Desktop()
        self.owned = SimpleNamespace(pid=77)
        for target, options in ((gui._input_api, {'return_value': self.desktop}),
                                (gui._owned_session, {}), (gui.time.sleep, {})):
            owner = gui.time if target == gui.time.sleep else gui
            active = patch.object(owner, target.__name__, **options)
            active.start()
            self.addCleanup(active.stop)

    def click(self): return gui.click_client(Path('environment'), {}, self.owned, 1180, 716)

    def test_own_child_window_accepts_one_click(self):
        self.desktop.hit = 3
        self.click()
        self.assertEqual([e[1] for e in self.desktop.events], [0x8001, 2, 4])

    def test_foreign_overlay_rejected_before_any_input(self):
        self.desktop.hit = 2
        with self.assertRaisesRegex(ValueError, 'перекрыта'): self.click()
        self.assertEqual(self.desktop.events, [])

    def test_other_window_of_same_process_is_not_game_client(self):
        self.desktop.hit = 2
        self.desktop.pids[2] = 77
        with self.assertRaisesRegex(ValueError, 'перекрыта'): self.click()
        self.assertEqual(self.desktop.events, [])

    def test_overlay_after_cursor_motion_prevents_mouse_down(self):
        self.desktop.overlay_after = 2
        with self.assertRaisesRegex(ValueError, 'перекрыта'): self.click()
        self.assertEqual([e[1] for e in self.desktop.events], [0x8001])

    def test_scroll_rechecks_each_notch(self):
        self.desktop.overlay_after = 4
        with self.assertRaisesRegex(ValueError, 'перекрыта'):
            gui.scroll_client(Path('environment'), {}, self.owned, 683, 430, -4)
        self.assertEqual([e[1] for e in self.desktop.events], [0x8001, 0x800])
        self.assertEqual(self.desktop.events[-1][4], (-120) & 0xffffffff)

    def test_keyboard_foreign_focus_receives_nothing(self):
        self.desktop.foreground = 2
        with self.assertRaisesRegex(ValueError, 'фокус'):
            gui.press_key(Path('environment'), {}, self.owned, 0x44)
        self.assertEqual(self.desktop.events, [])

    def test_client_bounds_checked_before_input(self):
        with self.assertRaisesRegex(ValueError, 'вне игрового'):
            gui.click_client(Path('environment'), {}, self.owned, 1366, 716)
        self.assertEqual(self.desktop.events, [])

    def test_cursor_intervention_prevents_mouse_down(self):
        with patch.object(self.desktop, 'GetCursorPos', return_value=False):
            with self.assertRaisesRegex(ValueError, 'Указатель'): self.click()
        self.assertEqual([e[1] for e in self.desktop.events], [0x8001])


class RunnerLifetimeTests(unittest.TestCase):
    def exercise(self, initialisation_error=None, screenshot_error=None):
        with tempfile.TemporaryDirectory() as directory:
            environment = Path(directory)/'environment'
            environment.mkdir()
            output = Path(directory)/'output'
            source = environment/'source.v2'
            source.write_bytes(b'unchanged')
            config = dict(environment=str(environment), output=str(output), save='cr36_start.v2',
                          save_path=str(source), save_sha256=runner.old.cli.digest(source), inputs={},
                          expected_client=[1366, 768], africa_client=[1180, 716], zoom_client=[683, 430],
                          nationality_client=[1083, 642],
                          startup_timeout=900, load_timeout=1200, image_review_required=True)
            process = Mock()

            def start(*args, **kwargs):
                (environment/'session.json').write_text(json.dumps(dict(token='owned', pid=77)))
                return dict(ok=True)

            status = dict(ok=True, campaign_loaded=True, paused=True, event_windows=0,
                          advance_target=0, date_raw=123)
            with patch.object(runner, 'preflight', return_value=config), \
                    patch.object(runner.old.cli, 'start', side_effect=start) as started, \
                    patch.object(runner.old, 'OwnedProcess', return_value=process), \
                    patch.object(runner, 'wait_initialisation', side_effect=initialisation_error), \
                    patch.object(runner.old.cli, 'load_campaign', return_value=dict(ok=True)), \
                    patch.object(runner.old.cli, 'request', return_value=status), \
                    patch.object(runner.gui, 'capture_politics', return_value=dict(client_size=[1366, 768]),
                                 side_effect=screenshot_error) as captures, \
                    patch.object(runner.gui, 'press_key', return_value={}) as key, \
                    patch.object(runner.gui, 'click_client', return_value={}) as click, \
                    patch.object(runner.gui, 'scroll_client', return_value={}) as scroll:
                result = runner.run(environment, 'cr36_start.v2', output)
                started.assert_called_once_with(environment, hidden=False)
                process.stop.assert_called_once()
                self.assertTrue(result['save_unchanged'])
                self.assertEqual(json.loads((output/'report.json').read_text(encoding='utf-8'))['ok'], result['ok'])
                return result, captures.call_args_list, key.call_count, click.call_count, scroll.call_count

    def test_initialisation_failure_stops_owned_process_without_gui(self):
        result, captures, keys, clicks, scrolls = self.exercise(initialisation_error=ValueError('cold timeout'))
        self.assertFalse(result['ok'])
        self.assertEqual((len(captures), keys, clicks, scrolls), (0, 0, 0, 0))

    def test_occluded_capture_failure_stops_without_later_actions(self):
        result, captures, keys, clicks, scrolls = self.exercise(screenshot_error=ValueError('lost focus'))
        self.assertFalse(result['ok'])
        self.assertEqual((len(captures), keys, clicks, scrolls), (1, 0, 0, 0))

    def test_four_frames_only_initial_frame_may_activate_window(self):
        result, captures, keys, clicks, scrolls = self.exercise()
        self.assertTrue(result['ok'])
        self.assertTrue(result['image_review_required'])
        self.assertEqual([call.kwargs['activate'] for call in captures], [True, False, False, False])
        self.assertEqual((keys, clicks, scrolls), (0, 2, 1))


if __name__ == '__main__':
    unittest.main()
