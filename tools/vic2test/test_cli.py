"""Проверки сохранности оригиналов при подготовке тестовой копии."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import threading
import time
from unittest.mock import patch

import cli
import scripts


class PreparationTests(unittest.TestCase):
    def test_copy_is_independent_and_redirects_user_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "game"
            root.mkdir()
            (root / "v2game.exe").write_bytes(b"fixture executable")
            generator = root / "autogen" / ".vs"
            generator.mkdir(parents=True)
            (generator / "locked.vsidx").write_bytes(b"source-only IDE cache")
            (root / "settings.txt").write_text('fullScreen=yes\nautosave="MONTHLY"\n', encoding="cp1251")
            (root / "messagetypes_custom.txt").write_text('notice={ popup=yes pausepopup = yes log=yes }', encoding="cp1251")
            source = Path(directory) / "original.v2"
            source.write_bytes(b'date="1836.1.1"')
            expected = hashlib.sha256((root / "v2game.exe").read_bytes()).hexdigest()
            with patch.object(cli, "ROOT", root), patch.object(cli, "EXE_HASH", expected):
                result = cli.prepare(root / ".work" / "test", [source])
            copy = Path(result["sources"][0]["copy"])
            self.assertEqual(copy.read_bytes(), source.read_bytes())
            self.assertFalse((Path(result["game"]) / "autogen").exists())
            copy.write_bytes(b"changed copy")
            self.assertEqual(source.read_bytes(), b'date="1836.1.1"')
            self.assertEqual((root / "settings.txt").read_text(encoding="cp1251"), 'fullScreen=yes\nautosave="MONTHLY"\n')
            self.assertIn('autosave="NEVER"', (Path(result["userdata"]) / "settings.txt").read_text(encoding="cp1251"))
            self.assertEqual((root / "messagetypes_custom.txt").read_text(encoding="cp1251"), 'notice={ popup=yes pausepopup = yes log=yes }')
            self.assertEqual((Path(result["userdata"]) / "messagetypes_custom.txt").read_text(encoding="cp1251"), 'notice={ popup=no pausepopup=no log=yes }')
            self.assertEqual((Path(result["game"]) / "userdir.txt").read_text(), Path(result["userdata"]).as_posix() + "/")

    def test_prepare_refuses_existing_or_external_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(cli, "ROOT", root):
                with self.assertRaises(ValueError):
                    cli.prepare(root / "outside", [])
                destination = root / ".work" / "exists"
                destination.mkdir(parents=True)
                with self.assertRaises(ValueError):
                    cli.prepare(destination, [])


class ScriptTests(unittest.TestCase):
    def test_strings_comments_and_lists(self):
        nodes = scripts.parse('country_event = { id = 5 desc = "{#still text}" color = { 1 2 3 } # }\n }')
        self.assertEqual(nodes[0].children[1].value, "{#still text}")

    def test_rejects_unclosed_text_and_blocks(self):
        for source in ['a = { b = 1', 'a = "unfinished', 'a = 1 }']:
            with self.subTest(source=source), self.assertRaises(scripts.ParseError):
                scripts.parse(source)


class DeadlineTests(unittest.TestCase):
    def test_initial_loading_uses_requested_deadline_without_short_retry(self):
        with patch.object(cli, "request", return_value={"ok": True}) as exchange:
            self.assertTrue(cli.wait_request(Path("unused"), "open-single-player", 600)["ok"])
            self.assertGreater(exchange.call_args.args[2], 590)

    def test_stalled_exchange_is_bounded_and_poison_blocks_later_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            environment = Path(directory)
            release = threading.Event()

            def stalled(environment, command, timeout, connected, cancelled):
                connected.set()
                release.wait(2)
                return {"ok": True}

            try:
                with patch.object(cli, "_blocking_request", side_effect=stalled) as exchange:
                    started = time.monotonic()
                    result = cli.request(environment, "status", 0.05)
                    self.assertLess(time.monotonic() - started, 1)
                    self.assertEqual(result["error"], "E_IPC_TIMEOUT")
                    self.assertTrue((environment / "poisoned.json").exists())
                    self.assertEqual(cli.request(environment, "decision-execute HAW x", 0.05)["error"], "E_SESSION_POISONED")
                    self.assertEqual(exchange.call_count, 1)
            finally:
                release.set()

    def test_exchange_failure_returns_structured_error_without_deadlock(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(cli, "_blocking_request", side_effect=ValueError("bad JSON")):
                result = cli.request(Path(directory), "status", 1)
            self.assertEqual(result["error"], "E_IPC")

    def test_timeout_before_delivery_does_not_poison_or_send_later(self):
        with tempfile.TemporaryDirectory() as directory:
            release = threading.Event()
            observed = []

            def waiting(environment, command, timeout, connected, cancelled):
                release.wait(1)
                observed.append(cancelled.is_set())
                return {"ok": False, "error": "E_PIPE_BUSY"}

            try:
                with patch.object(cli, "_blocking_request", side_effect=waiting):
                    result = cli.request(Path(directory), "status", 0.05)
                self.assertEqual(result["error"], "E_PIPE_BUSY")
                self.assertFalse((Path(directory) / "poisoned.json").exists())
            finally:
                release.set()


if __name__ == "__main__":
    unittest.main()
