"""Импорт стран из закреплённого Chronology; без --apply только расчёт и проверки."""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.vic2test.scripts import parse

SOURCE = "01004f77b55bddc9ff969ef25fcf7f4f38f8eaff"
BASE = "88c23775e49fec12a8ed21da85de0ad19dc9bc19"
PREFIX = "mod/chronology/"
HERE = Path(__file__).resolve().parent
FLAGS = ("", "_communist", "_fascist", "_monarchy", "_republic")
LOC_FIXES = {
    "OBO": "Обер-Ост", "OBO_ADJ": "Обер-Остск.",
    "TKG": "Сёгунат Токугава", "TKS_ADJ": "Туркестанск.",
    "DUB": "Дубай", "DUB_ADJ": "Дубайск.",
    "RGR_conservative_2": "Национальная партия", "RGR_liberal_2": "Либеральная партия",
    "RGR_liberal_3": "Прогрессивная партия", "RGR_conservative_3": "Конституционная партия",
    "RGR_liberal_4": "Демократическая партия", "KMT_liberal_2": "Гоминьдан",
    "KMT_liberal_3": "Демократическая лига", "KMT_conservative_4": "Консервативная фракция",
    "KMT_reactionary_2": "Националистическая фракция", "TGA_liberal_2": "Либеральная партия",
}

# Исторические уточнения текущей сборки не должны теряться при повторном импорте.
# Реестр и источники: docs/research/culture-history.md.
HISTORY_OVERRIDES = {
    "GBS": {"primary_culture": "papel_balanta"},
}


def history_overrides(tag, history, cultures):
    result = dict(history)
    fixes = []
    for key, value in HISTORY_OVERRIDES.get(tag, {}).items():
        assert key == "primary_culture" and value in cultures, (tag, key, value)
        assert tag != "GBS" or history.get(key) == "african_minor", (tag, history)
        result[key] = value
        fixes.append(f"Историческое уточнение {key}: {history.get(key)} → {value}; реестр culture-history")
    return result, fixes


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def blob(path, rev=SOURCE):
    return git("show", f"{rev}:{path}")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encoded(text):
    return (text.replace("\r\n", "\n").rstrip() + "\n").replace("\n", "\r\n").encode("cp1251")


def vals(nodes):
    return {n.key: n.value for n in nodes if n.value is not None}


def nodes(path):
    return parse((ROOT / path).read_bytes().decode("cp1251"))


def localisation(paths, reader):
    result = {}
    for path in paths:
        label = path.relative_to(ROOT).as_posix() if isinstance(path, Path) else path
        for line in reader(path).decode("cp1251").splitlines():
            if line.startswith("#") or ";" not in line:
                continue
            parts = line.split(";")
            result.setdefault(parts[0], []).append((label, parts[1]))
    return result


def date(value):
    return tuple(map(int, value.split(".")))


def unit_name_tokens(text):
    """Имена внутри списков unit_names: строки, обычные слова и ошибочные «умные» кавычки."""
    start = re.search(r'\bunit_names\s*=\s*\{', text)
    if not start:
        return
    depth = 1
    pattern = r'"[^"\n]*"|“[^”\n]*”|#[^\n]*|\s+|[{}=]|[^\s{}="#]+'
    for match in re.finditer(pattern, text[start.end():]):
        raw = match[0]
        if raw.isspace() or raw.startswith("#"):
            continue
        if raw == "{":
            depth += 1
        elif raw == "}":
            depth -= 1
            if depth == 0:
                return
        elif depth == 2:
            assert raw != "=", raw
            yield start.end() + match.start(), start.end() + match.end(), raw.strip('"“”')
    assert depth == 0, "Незакрытый блок unit_names"


def foreign_letters(name):
    return bool(re.search("[A-Za-z]", re.sub(r'\b[IVXLCDM]+\b', '', name)))


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        assert key not in result, f"Повторный ключ: {key}"
        result[key] = value
    return result


def translate_unit_names(text, translations):
    tokens = list(unit_name_tokens(text))
    translated = 0
    for start, end, name in reversed(tokens):
        if name in translations:
            text = text[:start] + '"' + translations[name] + '"' + text[end:]
            translated += 1
        else:
            assert not foreign_letters(name), name
            if text[start] == '“':
                text = text[:start] + '"' + name + '"' + text[end:]
    return text, translated


def make_import():
    paths = git("ls-tree", "-r", "--name-only", SOURCE, PREFIX).decode().splitlines()
    # В Git файл нормализован в LF, рабочая игровая копия использует CRLF.
    baseline = blob("common/countries.txt", BASE).replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
    current = vals(parse(baseline.decode("cp1251")))
    source = vals(parse(blob(PREFIX + "common/countries.txt").decode("cp1251")))
    cultures = {c.key for g in nodes("common/cultures.txt") for c in g.children if c.children}
    # Состав импорта закреплён на исходном наборе культур. Добавление новых
    # культур в игру не должно незаметно импортировать ранее исключённые страны.
    qualification_cultures = {c.key for g in parse(blob("common/cultures.txt", BASE).decode("cp1251"))
                              for c in g.children if c.children}
    governments = {g.key: {n.key for n in g.children if n.value == "yes"} for g in nodes("common/governments.txt")}
    source_loc = localisation([p for p in paths if p.startswith(PREFIX + "localisation/") and p.endswith(".csv")], blob)
    current_loc = localisation(sorted(p for p in (ROOT / "localisation").glob("*.csv") if p.name != "chronology_countries.csv"), Path.read_bytes)
    translations = json.loads((HERE / "unit_name_translations.json").read_text(encoding="utf-8"), object_pairs_hook=unique_pairs)
    for value in translations.values():
        assert value and re.search("[А-Яа-яЁё]", value) and not foreign_letters(value), value
        assert '"' not in value and '\n' not in value and '\r' not in value, value
        value.encode("cp1251")
    histories = {Path(p).name[:3]: p for p in paths if p.startswith(PREFIX + "history/countries/")}
    outputs, rows, countries, excluded, reused_loc = {}, {}, [], [], {}

    def add_loc(key):
        if key in current_loc:
            assert all(re.search("[А-Яа-яЁё]", v) for _, v in current_loc[key]), (key, current_loc[key])
            reused_loc[key] = current_loc[key]
            return
        options = source_loc.get(key, [])
        value = LOC_FIXES.get(key)
        if value is None:
            assert options and len({v for _, v in options}) == 1, (key, options)
            value = options[0][1]
        assert re.search("[А-Яа-яЁё]", value) and not re.search("[A-Za-z]", value), (key, value)
        assert key not in rows or rows[key] == value
        rows[key] = value

    for tag, path in source.items():
        if len(tag) != 3 or tag in current:
            continue
        history_path = histories.get(tag)
        history = vals(parse(blob(history_path).decode("cp1251"))) if history_path else {}
        culture = history.get("primary_culture")
        if culture not in qualification_cultures:
            excluded.append({"tag": tag, "primary_culture": culture, "reason": "Основная культура отсутствует в текущей сборке"})
            continue
        raw = blob(PREFIX + "common/" + path)
        text = raw.decode("cp1251").replace("\r\n", "\n")
        history, fixes = history_overrides(tag, history, cultures)
        culture = history["primary_culture"]
        assert culture in cultures, (tag, culture)
        if tag == "ERM":
            text, count = re.subn(r'(color\s*=\s*\{\s*)259\b', r'\g<1>255', text)
            assert count == 1
            fixes.append("Красный канал RGB 259 ограничен допустимым максимумом 255")
        if "pro_militarity" in text:
            text = text.replace("pro_militarity", "pro_military")
            fixes.append("Исправлена опечатка pro_militarity → pro_military")
        if tag == "RSR":
            text, count = re.subn(r'(name\s*=\s*"RUS_communist"\s+start_date\s*=\s*)1848\.1\.1', r'\g<1>1830.1.1', text)
            assert count == 1
            fixes.append("Первая коммунистическая партия доступна с 1830 года для инициализации пролетарской диктатуры")
        if tag in ("BHR", "QAT"):
            fixes.append("Восстановлены Ghalib/Khalid/Khalis, повреждённые заменой HAL → BHR внутри имён")
        text, translated = translate_unit_names(text, translations)
        parties = [vals(n.children) for n in parse(text) if n.key == "party"]
        government = history["government"]
        if government == "military_clique":
            government = "presidential_dictatorship"
            fixes.append("Отсутствующая military_clique заменена на presidential_dictatorship")
        nationalvalue = history.get("nationalvalue", "nv_order")
        if nationalvalue == "nv_tradition":
            nationalvalue = "nv_order"
            fixes.append("Отсутствующая nv_tradition заменена на nv_order")
        eligible = [p for p in parties if p["ideology"] in governments[government] and date(p["start_date"]) <= (1836, 1, 1) < date(p["end_date"])]
        assert eligible, tag
        ruling = history.get("ruling_party")
        if ruling not in {p["name"] for p in eligible}:
            preferred = ruling.replace("SAU_", "HSM_") if tag == "HSM" else ruling.replace("GDL_", "MAR_") if tag == "MAR" else None
            replacement = next((p["name"] for p in eligible if p["name"] == preferred), eligible[0]["name"])
            fixes.append(f"Недопустимая начальная партия {ruling} заменена на {replacement}")
            ruling = replacement
        minimum = dict(primary_culture=culture, religion=history["religion"], government=government,
                       nationalvalue=nationalvalue, civilized=history.get("civilized", "no"), ruling_party=ruling)
        new_history = "# Минимальное определение отсутствующей страны; сценарные данные не перенесены.\n"
        new_history += "\n".join(f"{k} = {v}" for k, v in minimum.items())
        common_path = "common/" + path
        target_history = "history/countries/" + Path(history_path).name
        outputs[common_path] = encoded(text)
        outputs[target_history] = encoded(new_history)
        for suffix in FLAGS:
            flag = "gfx/flags/" + tag + suffix + ".tga"
            outputs[flag] = blob(PREFIX + flag)
        for key in [tag, tag + "_ADJ"] + [p["name"] for p in parties]:
            add_loc(key)
        # Не переносим локализации сценарных событий и решений Chronology.
        for gov in governments:
            for key in [tag + "_" + gov, tag + "_" + gov + "_ADJ"]:
                if key in source_loc:
                    add_loc(key)
        countries.append(dict(tag=tag, primary_culture=culture, common=common_path, history=target_history,
                              source_common_sha256=sha(raw), source_history_sha256=sha(blob(history_path)),
                              translated_unit_names=translated, fixes=fixes))
    # autogen читает CSV разделителями ';'; комментарий без ';' поглощает первый ключ.
    rows_text = "#CODE;Русский;;;;;;;;;;;;;x\n"
    rows_text += "\n".join(f"{k};{v};;;;;;;;;;;;;x" for k, v in sorted(rows.items()))
    outputs["localisation/chronology_countries.csv"] = encoded(rows_text)
    additions = "# Страны Chronology Mod с культурами текущей сборки\r\n" + "".join(f'{c["tag"]}\t= "{c["common"][7:]}"\r\n' for c in sorted(countries, key=lambda x: x["tag"])) + "\r\n"
    marker = b"##HoD Dominions"
    assert baseline.count(marker) == 1
    outputs["common/countries.txt"] = baseline.replace(marker, additions.encode("cp1251") + marker)
    manifest = dict(source_revision=SOURCE, source_directory=PREFIX, baseline_revision=git("rev-parse", BASE).decode().strip(),
                    imported_count=len(countries), excluded=excluded, countries=countries,
                    localisation_rows=len(rows), reused_localisations=reused_loc,
                    files={p: dict(sha256=sha(b), bytes=len(b)) for p, b in sorted(outputs.items())})
    return outputs, manifest, baseline


def check_tga(data, path):
    """Проверить размеры и полноту пикселей несжатого/RLE TrueColor TGA."""
    assert len(data) >= 18, path
    header = struct.unpack("<BBBHHBHHHHBB", data[:18])
    id_size, color_map, kind = header[:3]
    width, height, depth = header[8:11]
    assert color_map == 0 and kind in (2, 10), (path, header)
    # SNR в источнике имеет ширину 96; размер сам по себе не портит TGA.
    # Основная сборка также содержит флаги нескольких размеров.
    assert (width, height) in ((93, 64), (96, 64)) and depth in (24, 32), (path, header)
    position, pixels, stride = 18 + id_size, 0, depth // 8
    if kind == 2:
        assert len(data) >= position + width * height * stride, path
        return
    while pixels < width * height:
        assert position < len(data), path
        packet = data[position]
        position += 1
        count = (packet & 127) + 1
        position += stride if packet & 128 else count * stride
        pixels += count
        assert position <= len(data) and pixels <= width * height, path


def check_data(outputs, manifest):
    """Структура и ссылки не заменяют загрузку данных игровым движком."""
    loc_text = outputs["localisation/chronology_countries.csv"].decode("cp1251")
    loc_rows = []
    for line in loc_text.splitlines():
        fields = line.split(";")
        assert len(fields) == 15, "CSV должен сохранять 15 полей, включая строку заголовка"
        if fields[0].startswith("#"):
            continue
        assert re.search("[А-Яа-яЁё]", fields[1]) and not foreign_letters(fields[1]), fields[0]
        loc_rows.append(fields[0])
    assert len(loc_rows) == len(set(loc_rows)) == manifest["localisation_rows"]
    previous_loc = localisation(sorted(p for p in (ROOT / "localisation").glob("*.csv") if p.name != "chronology_countries.csv"), Path.read_bytes)
    assert not (set(loc_rows) & previous_loc.keys()), "Новая локализация перекрывает существующую"
    available_loc = set(loc_rows) | previous_loc.keys()
    religions = {c.key for g in nodes("common/religion.txt") for c in g.children}
    cultures = {c.key for g in nodes("common/cultures.txt") for c in g.children if c.children}
    governments = {g.key: {n.key for n in g.children if n.value == "yes"} for g in nodes("common/governments.txt")}
    nationalvalues = {n.key for n in nodes("common/nationalvalues.txt")}
    ideologies = {c.key for g in nodes("common/ideologies.txt") for c in g.children}
    policies = {n.key: {c.key for c in n.children} for g in nodes("common/issues.txt") for n in g.children}
    graphics = set((ROOT / "common/graphicalculturetype.txt").read_text().split())
    units = {p.stem for p in (ROOT / "units").glob("*.txt")}
    all_tags = [n.key for n in parse(outputs["common/countries.txt"].decode("cp1251"))]
    assert not [k for k, count in Counter(all_tags).items() if count > 1]
    assert all_tags.index("dynamic_tags") > max(all_tags.index(c["tag"]) for c in manifest["countries"])
    for country in manifest["countries"]:
        assert country["tag"] in available_loc and country["tag"] + "_ADJ" in available_loc
        text = outputs[country["common"]].decode("cp1251")
        parsed = parse(text)
        assert all(count == 1 for key, count in Counter(n.key for n in parsed if n.key != "party").items())
        assert not any(foreign_letters(name) for _, _, name in unit_name_tokens(text)), country["tag"]
        assert '“' not in text and '”' not in text, country["tag"]
        color = re.search(r'\bcolor\s*=\s*\{([^}]+)\}', text)
        assert color, country["tag"]
        rgb = color[1].split()
        assert len(rgb) == 3 and all(v.isdigit() and 0 <= int(v) <= 255 for v in rgb), (country["tag"], rgb)
        assert vals(parsed)["graphical_culture"] in graphics, country["tag"]
        names = []
        for node in parsed:
            if node.key == "party":
                party = vals(node.children)
                assert len(party) == len(node.children), (country["tag"], "Повторное поле партии")
                assert party["ideology"] in ideologies
                assert date(party["start_date"]) < date(party["end_date"]), (country["tag"], party)
                names.append(party["name"])
                assert party["name"] in available_loc, party["name"]
                for key in ("economic_policy", "trade_policy", "religious_policy", "citizenship_policy", "war_policy"):
                    assert party[key] in policies[key], (country["tag"], key, party[key])
            elif node.key == "unit_names":
                assert all(n.key in units for n in node.children), (country["tag"], [n.key for n in node.children if n.key not in units])
        assert len(names) == len(set(names)), country["tag"]
        history_nodes = parse(outputs[country["history"]].decode("cp1251"))
        assert all(n.value is not None for n in history_nodes)
        history = vals(history_nodes)
        assert len(history) == len(history_nodes), country["tag"]
        assert set(history) == {"primary_culture", "religion", "government", "nationalvalue", "civilized", "ruling_party"}
        assert history["religion"] in religions and history["nationalvalue"] in nationalvalues
        assert history["primary_culture"] in cultures and history["government"] in governments
        assert history["ruling_party"] in names
        ruling = next(vals(n.children) for n in parsed if n.key == "party" and vals(n.children)["name"] == history["ruling_party"])
        assert ruling["ideology"] in governments[history["government"]]
        assert date(ruling["start_date"]) <= (1836, 1, 1) < date(ruling["end_date"])
    for path, data in outputs.items():
        if path.endswith(".tga"):
            check_tga(data, path)
        else:
            assert b"\n" not in data.replace(b"\r\n", b""), path
            data.decode("cp1251")
    return {"ok": True, "countries": len(manifest["countries"]), "files": len(outputs), "flags": sum(p.endswith(".tga") for p in outputs), "localisation_rows": manifest["localisation_rows"]}


def main():
    if not __debug__:
        raise RuntimeError("Проверки импорта требуют Python без -O")
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--apply", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    outputs, manifest, baseline = make_import()
    result = check_data(outputs, manifest)
    if args.check:
        for path, data in outputs.items():
            assert (ROOT / path).read_bytes() == data, f"Файл отличается от проверенного импорта: {path}"
        saved = json.loads((HERE / "manifest.json").read_text(encoding="utf-8"))
        # Сериализация преобразует пары локализации в списки.
        assert json.loads(json.dumps(manifest)) == saved, "Манифест изменился"
    elif args.apply:
        previous_path = HERE / "manifest.json"
        previous = json.loads(previous_path.read_text(encoding="utf-8")) if previous_path.exists() else {}
        if previous:
            assert previous["source_revision"] == SOURCE and previous["baseline_revision"] == manifest["baseline_revision"]
        # Все конфликты проверяются до первой записи.
        for path, data in outputs.items():
            dest = ROOT / path
            if dest.exists():
                allowed = [data, baseline] if path == "common/countries.txt" else [data]
                actual = dest.read_bytes()
                recorded = previous.get("files", {}).get(path, {})
                managed = recorded.get("sha256") == sha(actual) and recorded.get("bytes") == len(actual)
                assert actual in allowed or managed, f"Конфликт существующего файла: {path}"
        for path, data in outputs.items():
            dest = ROOT / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
        (HERE / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
