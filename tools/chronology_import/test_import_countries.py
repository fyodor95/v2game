"""Отрицательные регрессии валидатора импортированных игровых данных."""
import json
import struct
import unittest

from tools.chronology_import import import_countries as ci


class ImportedDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((ci.HERE / "manifest.json").read_text(encoding="utf-8"))
        cls.files = {p: (ci.ROOT / p).read_bytes() for p in cls.manifest["files"]}
        cls.country = next(c for c in cls.manifest["countries"] if c["tag"] == "ERM")

    def test_installed_data(self):
        self.assertTrue(ci.check_data(self.files, self.manifest)["ok"])

    def bad_history(self, old, new):
        files = dict(self.files)
        path = self.country["history"]
        self.assertIn(old, files[path])
        files[path] = files[path].replace(old, new, 1)
        with self.assertRaises(AssertionError):
            ci.check_data(files, self.manifest)

    def test_missing_culture(self):
        self.bad_history(b"bulgarian", b"unknown_culture")

    def test_missing_religion(self):
        self.bad_history(b"orthodox", b"unknown_religion")

    def test_missing_government(self):
        self.bad_history(b"hms_government", b"military_clique")

    def test_missing_party(self):
        self.bad_history(b"ERM_conservative", b"FOREIGN_party")

    def test_party_disallowed_by_government(self):
        self.bad_history(b"hms_government", b"proletarian_dictatorship")

    def test_scenario_field(self):
        self.bad_history(b"civilized = yes", b"civilized = yes\r\ncapital = 1")

    def test_duplicate_history_field(self):
        self.bad_history(b"primary_culture = bulgarian", b"primary_culture = bulgarian\r\nprimary_culture = bulgarian")

    def test_rgb_overflow(self):
        files = dict(self.files)
        path = self.country["common"]
        self.assertIn(b"255", files[path])
        files[path] = files[path].replace(b"255", b"259", 1)
        with self.assertRaises(AssertionError):
            ci.check_data(files, self.manifest)

    def test_bare_foreign_unit_name(self):
        files = dict(self.files)
        path = self.country["common"]
        files[path] = files[path].replace('"София"'.encode('cp1251'), b'Sofia', 1)
        with self.assertRaises(AssertionError):
            ci.check_data(files, self.manifest)

    def test_csv_comment_without_separator(self):
        files = dict(self.files)
        path = "localisation/chronology_countries.csv"
        files[path] = b"# Comment without separator\r\n" + files[path].split(b"\r\n", 1)[1]
        with self.assertRaises(AssertionError):
            ci.check_data(files, self.manifest)

    def test_unknown_policy(self):
        files = dict(self.files)
        path = self.country["common"]
        self.assertIn(b"pro_military", files[path])
        files[path] = files[path].replace(b"pro_military", b"pro_militarity", 1)
        with self.assertRaises(AssertionError):
            ci.check_data(files, self.manifest)

    def test_tags_must_precede_dynamic_tags(self):
        files = dict(self.files)
        path = "common/countries.txt"
        files[path] = b"dynamic_tags = yes\r\n" + files[path].replace(b"dynamic_tags  = yes", b"# dynamic_tags = yes")
        with self.assertRaises(AssertionError):
            ci.check_data(files, self.manifest)


class FlagTests(unittest.TestCase):
    def tga(self, kind=2):
        return struct.pack("<BBBHHBHHHHBB", 0, 0, kind, 0, 0, 0, 0, 0, 93, 64, 24, 0)

    def test_uncompressed_truncation(self):
        ci.check_tga(self.tga() + bytes(93 * 64 * 3), "valid.tga")
        with self.assertRaises(AssertionError):
            ci.check_tga(self.tga() + bytes(93 * 64 * 3 - 1), "truncated.tga")

    def test_rle_pixel_overflow(self):
        # 47 полных пакетов по 128 пикселей превышают размер изображения 5952.
        with self.assertRaises(AssertionError):
            ci.check_tga(self.tga(10) + bytes([255, 0, 0, 0]) * 47, "overflow.tga")

    def test_rle_truncated_pixel(self):
        with self.assertRaises(AssertionError):
            ci.check_tga(self.tga(10) + bytes([255, 0, 0]), "truncated.tga")


class TranslationTests(unittest.TestCase):
    def test_bare_quoted_and_smart_quotes(self):
        source = 'unit_names = { cruiser = { Sofia "Varna" “Stara Zagora” "Пётр I" } }'
        result, count = ci.translate_unit_names(source, {'Sofia': 'София', 'Varna': 'Варна', 'Stara Zagora': 'Стара-Загора'})
        self.assertEqual(count, 3)
        self.assertEqual([n for _, _, n in ci.unit_name_tokens(result)], ['София', 'Варна', 'Стара-Загора', 'Пётр I'])
        self.assertNotIn('“', result)

    def test_unknown_bare_name_rejected(self):
        with self.assertRaises(AssertionError):
            ci.translate_unit_names('unit_names = { cruiser = { Unknown } }', {})

    def test_duplicate_translation_key_rejected(self):
        with self.assertRaises(AssertionError):
            json.loads('{"name": "one", "name": "two"}', object_pairs_hook=ci.unique_pairs)

    def test_only_unit_names_are_changed(self):
        source = 'party = { name = "Sofia" } unit_names = { cruiser = { Sofia } }'
        result, count = ci.translate_unit_names(source, {'Sofia': 'София'})
        self.assertIn('party = { name = "Sofia" }', result)
        self.assertEqual(count, 1)


if __name__ == "__main__":
    unittest.main()
