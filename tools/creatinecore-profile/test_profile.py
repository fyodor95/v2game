"""Проверки совместимости профиля, переводов и отсутствия лишних изменений."""
import importlib.util
from pathlib import Path
import re
import unittest

SPEC = importlib.util.spec_from_file_location("prepare", Path(__file__).with_name("prepare.py"))
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)


class ProfileTests(unittest.TestCase):
    def test_empty_values_do_not_consume_next_setting(self):
        self.assertEqual(PREPARE.assignments("A=\nB=\nC=0\n"), {"A": "", "B": "", "C": "0"})

    def test_all_upstream_settings_are_explicit(self):
        original = (PREPARE.UPSTREAM / "z_CreatineCore/creatinecore_settings.ini").read_text(encoding="utf-8-sig")
        self.assertLessEqual(PREPARE.assignments(original).keys(), PREPARE.assignments(PREPARE.profile()).keys())
        values = PREPARE.assignments(PREPARE.profile())
        for key in PREPARE.ENABLED:
            self.assertEqual(values[key], "1", key)
        for key in ("PATCH_PARALLEL_EVENTS", "PATCH_PARALLEL_DECISIONS", "PATCH_FAST_AI_QUEUE",
                    "TRADE_PRICE_DELTA_PERCENTUAL", "TRADE_MAX_PRICE_20X_MULTIPLIER",
                    "POP_BUILD_INFRASTRUCTURE_RULE", "ENABLE_MINTING", "PATCH_STATE_SAVINGS_CAP"):
            self.assertEqual(values[key], "0", key)

    def test_translations_preserve_upstream_parameters(self):
        PREPARE.validate_translation()

    def test_hidden_puppet_and_scan_patches_are_explicitly_disabled(self):
        source = (PREPARE.ROOT / ".work/creatinecore/upstream/Source/V2/V2/V2TechButton.cpp").read_text(encoding="utf-8-sig")
        values = PREPARE.assignments(PREPARE.profile())
        for key in ("PATCH_WAR_ON_PUPPET_ALLOWED", "PATCH_WAR_ON_PUPPET_TOOLTIP_1", "PATCH_WAR_ON_PUPPET_TOOLTIP_2"):
            self.assertEqual(values[key], "0", key)
            self.assertIn(f'{{ "{key.removeprefix("PATCH_").lower()}",', source)
        self.assertIn('_strnicmp(key, "PATCH_", 6)', source)
        self.assertEqual(values["PATCH_FACTORY_DUMP_SCAN"], "0")
        self.assertIn('_stricmp(key, "PATCH_FACTORY_DUMP_SCAN")', source)

    def test_no_new_duplicate_localization_keys(self):
        occurrences = {key: [] for key in PREPARE.TEXT | PREPARE.EXISTING_TEXT}
        for path in (PREPARE.ROOT / "localisation").glob("*.csv"):
            for line in path.read_bytes().decode("cp1251").splitlines():
                key = line.split(";", 1)[0]
                if key in occurrences:
                    occurrences[key].append(path.name)
        for key, files in occurrences.items():
            self.assertEqual(len(files), 1, f"{key}: {files}")

    def test_written_outputs_are_current(self):
        for path, expected in PREPARE.output_files().items():
            self.assertEqual(path.read_bytes(), expected, str(path))
        defines = (PREPARE.ROOT / "common/defines_ccore.txt").read_bytes().decode("cp1251")
        self.assertRegex(defines, r"(?m)^pop_salary_additional_multiplier = 0\r?$")

    def test_budget_field_is_unique(self):
        gui = (PREPARE.ROOT / "interface/country_budget.gui").read_bytes().decode("cp1251")
        self.assertEqual(len(re.findall(r'name\s*=\s*"naval_base_expense"', gui)), 1)

    def test_budget_maintenance_row_is_below_salaries_and_left_of_total(self):
        gui = (PREPARE.ROOT / "interface/country_budget.gui").read_bytes().decode("cp1251")
        rectangles = []
        for name in ("building_maintenance_desc", "naval_base_expense"):
            field = re.search(r'name = "' + name + r'"(.*?)\n\t\t}', gui, re.S)[1]
            x, y = map(int, re.search(r'position = \{ x = (\d+) y = (\d+) }', field).groups())
            width = int(re.search(r'maxWidth = (\d+)', field)[1])
            height = int(re.search(r'maxHeight = (\d+)', field)[1])
            self.assertGreaterEqual(y, 425)
            self.assertLessEqual(y + height, 450)
            self.assertLessEqual(x + width, 830)
            rectangles.append((x, y, width, height))
        self.assertLessEqual(rectangles[0][0] + rectangles[0][2], rectangles[1][0])
        self.assertEqual(rectangles[0][1], rectangles[1][1])


if __name__ == "__main__":
    unittest.main()
