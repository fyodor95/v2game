"""Read-only диагностика: ровно один запрос и отказ до IPC при неверных числах."""
import contextlib
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch

import cli


class EventCheckCliTests(unittest.TestCase):
    def invoke(self, province="546", event="24754", result=None):
        output = io.StringIO()
        arguments = ["cli.py", "event-check", "--environment", ".work/fixture",
                     "--province", province, "--id", event]
        if result is None:
            result = {"ok": True, "trigger": False, "cached": {"contains": False}}
        with patch("sys.argv", arguments), patch.object(cli, "request", return_value=result) as request:
            with contextlib.redirect_stdout(output):
                code = cli.main()
        return code, json.loads(output.getvalue()), request

    def test_diagnosis_sends_only_read_only_command(self):
        code, result, request = self.invoke(event="0000024754")
        self.assertEqual(code, 0)
        self.assertFalse(result["trigger"])
        self.assertFalse(result["cached"]["contains"])
        request.assert_called_once_with(Path(".work/fixture"), "event-check-province 546 24754", 15)

    def test_native_refusal_is_not_success_or_retried(self):
        for error in ["E_NATIVE_LAYOUT", "E_PAUSE_REQUIRED", "E_DAY_BUSY", "E_EVENT_SCOPE", "E_PLAYER_ONLY"]:
            with self.subTest(error=error):
                expected = {"ok": False, "error": error}
                code, result, request = self.invoke(result=expected)
                self.assertNotEqual(code, 0)
                self.assertEqual(result, expected)
                request.assert_called_once()

    def test_bad_numbers_never_reach_transport(self):
        for province, event in [("0", "24754"), ("100000", "24754"), ("-1", "24754"),
                                ("546", "0"), ("546", "2147483648"), ("546", "1;pause"),
                                ("546", "1 2"), ("546", "1.5")]:
            with self.subTest(province=province, event=event):
                code, result, request = self.invoke(province, event)
                self.assertNotEqual(code, 0)
                self.assertEqual(result["error"], "E_CLIENT")
                request.assert_not_called()


if __name__ == "__main__":
    unittest.main()
