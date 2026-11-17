"""Отрицательные регрессии валидатора готовых событий появления корок."""
import copy
from pathlib import Path
import tempfile
import unittest

from tools.chronology_import.validate_core_events import one, parse, reconstruct_map, validate_events


FIRST = """
province_event = {
 id = 24001
 title = EVTNAME24001
 desc = EVTDESC24001
 trigger = {
  controlled_by_rebels = yes
  culture = north_german
  NOT = { is_core = SAX }
  owner = { NOT = { tag = SAX }
   SAX = { exists = no primary_culture = north_german NOT = { any_core = { always = yes } } }
  }
 }
 mean_time_to_happen = { months = 4 }
 option = { name = EVTOPTA24001
  if = { limit = {
   controlled_by_rebels = yes
   culture = north_german
   NOT = { is_core = SAX }
   owner = { NOT = { tag = SAX }
    SAX = { exists = no primary_culture = north_german NOT = { any_core = { always = yes } } }
   }
  } add_core = SAX }
 }
}
"""
SPREAD = """
province_event = {
 id = 24002
 title = EVTNAME24002
 desc = EVTDESC24002
 trigger = {
  controlled_by_rebels = yes
  NOT = { is_core = SAX }
  any_neighbor_province = { is_core = SAX }
  any_pop = { has_pop_culture = north_german }
  owner = { NOT = { tag = SAX }
   SAX = { OR = { accepted_culture = north_german primary_culture = north_german } }
  }
 }
 mean_time_to_happen = { months = 4 }
 option = { name = EVTOPTA24002 add_core = SAX }
}
"""
LOC = "\n".join(f"{prefix}{id};Русский текст;;;;;;;;;;;;;x"
                for id in (24001, 24002) for prefix in ("EVTNAME", "EVTDESC", "EVTOPTA"))
MODEL = {"rows": [dict(culture="north_german", main="SAX", union="GER", dynamic_fallback=None,
                       first=["SAX"], spread=["SAX"])]}


class CoreValidationTests(unittest.TestCase):
    def test_valid_first_and_spread(self):
        result = validate_events(parse(FIRST + SPREAD), LOC, MODEL)
        self.assertTrue(result["ok"], result["issues"])
        self.assertEqual(result["counts"], dict(first=1, spread=1, total=2))

    def test_guard_missing_one_condition(self):
        events = parse(FIRST + SPREAD)
        option = one(events[0].children, "option")
        limit = one(one(option.children, "if").children, "limit")
        limit.children.pop(0)
        result = validate_events(events, LOC, MODEL)
        self.assertFalse(result["ok"])
        self.assertTrue(any("Защита опции" in issue for issue in result["issues"]))

    def test_owner_only_core_check_is_rejected(self):
        result = validate_events(parse((FIRST + SPREAD).replace("any_core", "any_owned_province")), LOC, MODEL)
        self.assertFalse(result["ok"])

    def test_pop_culture_uses_actual_generator_command(self):
        # HasCulture в conditions.cpp и реальный generated output используют has_pop_culture.
        self.assertTrue(validate_events(parse(FIRST + SPREAD), LOC, MODEL)["ok"])
        damaged = SPREAD.replace("has_pop_culture = north_german", "culture = north_german")
        result = validate_events(parse(FIRST + damaged), LOC, MODEL)
        self.assertFalse(result["ok"])
        self.assertTrue(any("has_pop_culture" in issue for issue in result["issues"]))

    def test_missing_and_duplicate_pair(self):
        self.assertFalse(validate_events(parse(FIRST), LOC, MODEL)["ok"])
        self.assertFalse(validate_events(parse(FIRST + SPREAD + FIRST), LOC, MODEL)["ok"])
        duplicate = FIRST.replace("24001", "24003")
        extra_loc = LOC + "\n" + LOC.replace("24001", "24003").split("EVTNAME24002")[0]
        self.assertFalse(validate_events(parse(FIRST + SPREAD + duplicate), extra_loc, MODEL)["ok"])

    def test_no_first_for_accepted_only(self):
        model = copy.deepcopy(MODEL)
        model["rows"][0]["first"] = []
        self.assertFalse(validate_events(parse(FIRST + SPREAD), LOC, model)["ok"])
        self.assertTrue(validate_events(parse(SPREAD), LOC, model)["ok"])

    def test_wrong_order_and_unguarded_effect(self):
        events = parse(FIRST + SPREAD)
        trigger = one(events[0].children, "trigger")
        trigger.children[0], trigger.children[1] = trigger.children[1], trigger.children[0]
        self.assertFalse(validate_events(events, LOC, MODEL)["ok"])
        events = parse(FIRST + SPREAD)
        option = one(events[0].children, "option")
        option.children.append(copy.deepcopy(one(one(option.children, "if").children, "add_core")))
        self.assertFalse(validate_events(events, LOC, MODEL)["ok"])

    def test_missing_duplicate_or_nonrussian_localisation(self):
        for localisation in (LOC.replace("EVTDESC24001", "REMOVED"), LOC + "\nEVTDESC24001;Повтор;",
                             LOC.replace("Русский текст", "English text")):
            self.assertFalse(validate_events(parse(FIRST + SPREAD), localisation, MODEL)["ok"])

    def test_scaled_first_mtth_and_unchanged_spread(self):
        model = copy.deepcopy(MODEL)
        row = model["rows"][0]
        row["first"] += ["PRU"]
        row["spread"] += ["PRU", "FRA"]  # Признанная пара не увеличивает n.
        text = (FIRST.replace("months = 4", "months = 8") + SPREAD +
                FIRST.replace("SAX", "PRU").replace("24001", "24003").replace("months = 4", "months = 8") +
                SPREAD.replace("SAX", "PRU").replace("24002", "24004") +
                SPREAD.replace("SAX", "FRA").replace("24002", "24005"))
        loc = "\n".join(f"{prefix}{id};Русский текст;;;;;;;;;;;;;x"
                        for id in range(24001, 24006) for prefix in ("EVTNAME", "EVTDESC", "EVTOPTA"))
        good = validate_events(parse(text), loc, model)
        self.assertTrue(good["ok"], good["issues"])
        self.assertEqual(good["first_candidates"], {"north_german": 2})
        for broken in (text.replace("months = 8", "months = 4"),
                       text.replace("months = 4", "months = 8"),
                       text.replace("months = 8", "months = 8 modifier = { factor = 0.5 }")):
            result = validate_events(parse(broken), loc, model)
            self.assertFalse(result["ok"])
            self.assertTrue(any("MTTH" in issue for issue in result["issues"]))

    def test_candidate_reconstruction_deduplicates_union_and_excludes_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "common").mkdir()
            (root / "history/countries").mkdir(parents=True)
            (root / "common/cultures.txt").write_text(
                'german = { union = GER north_german = { color = { 1 1 1 } } '
                'south_german = { color = { 1 1 1 } } } '
                'french_group = { french = { color = { 1 1 1 } } }', encoding="ascii")
            (root / "common/countries.txt").write_text('dynamic_tags = yes\nD01 = "d.txt"\nD02 = "d2.txt"', encoding="ascii")
            (root / "autogen.cfg").write_text('countries_without_culture = {}', encoding="ascii")
            for tag, primary, extra in (("PRU", "north_german", ""), ("SAX", "north_german", ""),
                                        ("GER", "north_german", "culture = north_german"),
                                        ("FRA", "french", "culture = north_german"),
                                        ("D01", "french", ""), ("D02", "french", "")):
                (root / "history/countries" / (tag + '.txt')).write_text(
                    f'primary_culture = {primary}\n{extra}\ncivilized = yes', encoding="ascii")
            rows = {row["culture"]: row for row in reconstruct_map(root)["rows"]}
            self.assertEqual(set(rows["north_german"]["first"]), {"D01", "PRU", "SAX", "GER"})
            self.assertEqual(len(rows["north_german"]["first"]), 4)
            self.assertIn("FRA", rows["north_german"]["spread"])
            self.assertEqual(rows["south_german"]["first"], ["D02", "GER"])


if __name__ == "__main__":
    unittest.main()
