"""Сравнение реальных временных деревьев MP без запуска игры."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import cli
import mp_preflight


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.host, self.client = [self.root / ".work" / name for name in ("host", "client")]
        self.patch_root = patch.object(cli, "ROOT", self.root)
        self.patch_root.start()
        self.addCleanup(self.patch_root.stop)
        for environment in (self.host, self.client):
            (environment / "game").mkdir(parents=True)
            (environment / "game" / "map").mkdir()
            (environment / "game" / "gfx").mkdir()
            (environment / "userdata").mkdir()
            (environment / "environment.json").write_text(json.dumps({
                "game": str(environment / "game"), "userdata": str(environment / "userdata"),
                "sources": [{"source": "исходный пользовательский сейв"}],
            }), encoding="utf-8")
            self.put(environment, "userdir.txt", (str(environment / "userdata") + "/\n").encode("ascii"))
            self.put(environment, "v2game.exe", b"fixture executable")
            self.put(environment, "dinput8.dll", b"loader")
            self.put(environment, "z_CreatineCore/creatinecore.dll", b"core")
            self.put(environment, "z_CreatineCore/dll_Submods/gpc.dll", b"gpc")
            self.put(environment, "z_CreatineCore/creatinecore_settings.ini", b"BUILDING_GOODS_CONSUMPTION=0\r\n")
            self.put(environment, "events/event.txt", b"country_event = { id = 123 }\r\n")
            self.put(environment, "localisation/event.csv", "key;Русский текст;;;;;;;;;;;;;x\r\n".encode("cp1251"))

    def put(self, environment, relative, content):
        destination = environment / "game" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
        return destination

    def test_equal_inputs_have_repeatable_fingerprint_and_stream_hash(self):
        block = b"0123456789" * (mp_preflight.CHUNK_BYTES // 10 + 9)
        for environment in (self.host, self.client):
            self.put(environment, "map/large.bin", block * 3)
        first = mp_preflight.compare(self.host, self.client)
        second = mp_preflight.compare(self.client, self.host)
        self.assertTrue(first["ok"])
        self.assertEqual(first["differences"], [])
        self.assertEqual(first["fingerprint"], second["fingerprint"])
        self.assertEqual(first["host"]["fingerprint"], first["client"]["fingerprint"])
        manifest = mp_preflight.fingerprint_environment(self.host)
        self.assertEqual(manifest["entries"]["map/large.bin"]["sha256"], hashlib.sha256(block * 3).hexdigest())

    def test_each_binary_config_and_game_script_difference_is_reported(self):
        names = ["v2game.exe", "dinput8.dll", "z_CreatineCore/creatinecore.dll",
                 "z_CreatineCore/dll_Submods/gpc.dll", "z_CreatineCore/creatinecore_settings.ini",
                 "events/event.txt", "localisation/event.csv"]
        for relative in names:
            with self.subTest(relative=relative):
                target = self.client / "game" / relative
                original = target.read_bytes()
                target.write_bytes(original + b"changed")
                result = mp_preflight.compare(self.host, self.client)
                self.assertFalse(result["ok"])
                self.assertIsNone(result["fingerprint"])
                self.assertEqual([item["path"] for item in result["differences"]], [relative])
                self.assertEqual(result["differences"][0]["difference"], "content")
                target.write_bytes(original)

    def test_unknown_directories_root_scripts_and_unusual_extensions_are_included(self):
        for relative in ("future_feature/rules.unknown", "script/ai.lua", "news/world.txt",
                         "mod/future.mod", "root_rules.csv", "new.dll", "common/logs/rule.txt",
                         "events/cache/script.txt"):
            self.put(self.client, relative, b"active new data")
        result = mp_preflight.compare(self.host, self.client)
        changed = {item["path"] for item in result["differences"]}
        self.assertFalse(result["ok"])
        self.assertTrue({"future_feature/rules.unknown", "script/ai.lua", "news/world.txt",
                         "mod/future.mod", "root_rules.csv", "new.dll", "common/logs/rule.txt",
                         "events/cache/script.txt"} <= changed)
        self.assertTrue(all(item["difference"] == "only_client" for item in result["differences"]))

    def test_unicode_casefold_does_not_merge_distinct_names_across_environments(self):
        self.put(self.host, "events/Straße.txt", b"event data")
        self.put(self.client, "events/strasse.txt", b"event data")
        result = mp_preflight.compare(self.host, self.client)
        self.assertFalse(result["ok"])
        self.assertEqual({item["path"] for item in result["differences"]},
                         {"events/Straße.txt", "events/strasse.txt"})

    def test_userdata_logs_cache_and_session_differences_are_allowed(self):
        baseline = mp_preflight.compare(self.host, self.client)["fingerprint"]
        for number, environment in enumerate((self.host, self.client)):
            content = f"local {number}".encode("ascii")
            for relative in ("settings.txt", "messagetypes_custom.txt", "map/cache/time",
                             "gfx/cache/local.bin", "logs/error.log", "z_CreatineCore/Logs/v2dll_oos.log"):
                self.put(environment, relative, content)
            (environment / "session.json").write_bytes(content)
            (environment / "userdata" / "save games").mkdir()
            (environment / "userdata" / "save games" / f"{number}.v2").write_bytes(content)
            (environment / "userdata" / "settings.txt").write_bytes(content)
        result = mp_preflight.compare(self.host, self.client)
        self.assertTrue(result["ok"])
        self.assertEqual(result["fingerprint"], baseline)
        self.assertEqual(result["host"]["fingerprint"], result["client"]["fingerprint"])
        self.assertTrue(result["host"]["excluded"])

    def test_missing_and_type_changed_files_do_not_compare_equal(self):
        (self.client / "game" / "dinput8.dll").unlink()
        (self.client / "game" / "dinput8.dll").mkdir()
        (self.host / "game" / "events" / "event.txt").unlink()
        result = mp_preflight.compare(self.host, self.client)
        kinds = {item["path"]: item["difference"] for item in result["differences"]}
        self.assertEqual(kinds["dinput8.dll"], "type")
        self.assertEqual(kinds["events/event.txt"], "only_client")

    def test_redirect_to_other_environment_is_rejected(self):
        self.put(self.host, "userdir.txt", str(self.client / "userdata").encode("ascii"))
        with self.assertRaisesRegex(mp_preflight.PreflightError, "userdir.txt"):
            mp_preflight.compare(self.host, self.client)

    def test_external_or_shared_metadata_path_is_rejected(self):
        metadata = self.host / "environment.json"
        original = json.loads(metadata.read_text(encoding="utf-8"))
        for target in (self.client / "game", self.root / "outside", Path("game")):
            metadata.write_text(json.dumps({**original, "game": str(target)}), encoding="utf-8")
            with self.subTest(target=target), self.assertRaises(mp_preflight.PreflightError):
                mp_preflight.compare(self.host, self.client)

    def test_same_and_external_environments_are_rejected(self):
        with self.assertRaises(mp_preflight.PreflightError):
            mp_preflight.compare(self.host, self.host)
        with self.assertRaises(mp_preflight.PreflightError):
            mp_preflight.compare(self.root, self.client)

    def test_symlink_file_and_directory_are_rejected(self):
        outside = self.root / "external.txt"
        outside.write_bytes(b"external source")
        link = self.host / "game" / "linked.txt"
        try:
            link.symlink_to(outside)
        except OSError as error:
            self.skipTest(f"Создание тестового симлинка недоступно: {error}")
        with self.assertRaisesRegex(mp_preflight.PreflightError, "Симлинк"):
            mp_preflight.compare(self.host, self.client)
        link.unlink()
        link.symlink_to(self.client / "game" / "events", target_is_directory=True)
        with self.assertRaisesRegex(mp_preflight.PreflightError, "Симлинк"):
            mp_preflight.compare(self.host, self.client)

    def test_file_changed_while_hashing_is_rejected(self):
        target = self.host / "game" / "dinput8.dll"
        original = mp_preflight._sha256

        def change_after_read(path, before):
            result = original(path, before)
            if path == target:
                path.write_bytes(b"changed after hash")
            return result

        with patch.object(mp_preflight, "_sha256", side_effect=change_after_read):
            with self.assertRaisesRegex(mp_preflight.PreflightError, "изменилось") as raised:
                mp_preflight.compare(self.host, self.client)
        details = raised.exception.details
        self.assertEqual(details["phase"], "tree_recheck")
        self.assertIn("st_size", details["changed_fields"])
        self.assertEqual(details["before"]["st_size"], len(b"loader"))
        self.assertEqual(details["after"]["st_size"], len(b"changed after hash"))
        self.assertIn('"before":', str(raised.exception))
        self.assertIn('"after":', str(raised.exception))

    def changed_directory_stat(self, field, value):
        original = mp_preflight._regular_stat
        target = self.host / "game" / "events"
        calls = 0

        def observe(path):
            nonlocal calls
            info = original(path)
            if path == target:
                calls += 1
                fields = {name: getattr(info, name) for name in dir(info) if name.startswith("st_")}
                fields[field] = value(info, calls)
                return SimpleNamespace(**fields)
            return info

        return observe

    def test_directory_size_change_without_content_change_is_allowed(self):
        expected = mp_preflight.compare(self.host, self.client)["fingerprint"]
        observe = self.changed_directory_stat("st_size", lambda info, calls: 0 if calls <= 2 else 28672)
        with patch.object(mp_preflight, "_regular_stat", side_effect=observe):
            result = mp_preflight.compare(self.host, self.client)
        self.assertTrue(result["ok"])
        self.assertEqual(result["fingerprint"], expected)

    def test_directory_inode_change_is_rejected_with_diagnostics(self):
        observe = self.changed_directory_stat("st_ino", lambda info, calls: info.st_ino + (calls > 2))
        with patch.object(mp_preflight, "_regular_stat", side_effect=observe):
            with self.assertRaises(mp_preflight.PreflightError) as raised:
                mp_preflight.compare(self.host, self.client)
        self.assertEqual(set(raised.exception.details["changed_fields"]) &
                         set(raised.exception.details["compared_fields"]), {"st_ino"})
        self.assertIn("file_type", raised.exception.details["compared_fields"])
        self.assertNotIn("st_size", raised.exception.details["compared_fields"])

    def test_new_child_is_rejected_even_if_directory_timestamp_is_restored(self):
        target = self.host / "game" / "events"
        original = mp_preflight._sha256

        def add_after_read(path, before):
            result = original(path, before)
            if path == target / "event.txt":
                stamp = target.stat()
                (target / "unseen_event.txt").write_bytes(b"new event")
                os.utime(target, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
            return result

        with patch.object(mp_preflight, "_sha256", side_effect=add_after_read):
            with self.assertRaises(mp_preflight.PreflightError) as raised:
                mp_preflight.compare(self.host, self.client)
        self.assertEqual(raised.exception.details["phase"], "children_recheck")
        self.assertEqual(raised.exception.details["added"], ["unseen_event.txt"])
        self.assertEqual(raised.exception.details["removed"], [])

    @unittest.skipUnless(os.name == "nt", "Проверка Windows junction")
    def test_windows_junction_directory_is_rejected_without_following(self):
        target = self.client / "game" / "events"
        junction = self.host / "game" / "linked_directory"
        subprocess.run(["cmd.exe", "/d", "/c", "mklink", "/J", str(junction), str(target)],
                       check=True, capture_output=True)
        try:
            with self.assertRaisesRegex(mp_preflight.PreflightError, "reparse point"):
                mp_preflight.compare(self.host, self.client)
        finally:
            # Удаляется только сама ссылка, без рекурсивного обхода её цели.
            os.rmdir(junction)
        self.assertTrue((target / "event.txt").is_file())


if __name__ == "__main__":
    unittest.main()
