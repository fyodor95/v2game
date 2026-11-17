"""Проверка карты кандидатов и полного результата CoreAddingEvents без запуска игры."""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.vic2test.scripts import Node, parse


def read(path: Path) -> str:
    return path.read_bytes().decode("cp1251")


def reconstruct_map(root: Path) -> dict:
    """Независимо воспроизводит штатный выбор MainCountry, включая последовательность D-тэгов."""
    groups = parse(read(root / "common/cultures.txt"))
    order, unions = [], {}
    for group in groups:
        union = next((node.value for node in group.children if node.key == "union"), None)
        for node in group.children:
            if node.children:
                order.append(node.key)
                unions[node.key] = union
    countries = {}
    for path in sorted((root / "history/countries").glob("*.txt")):
        tag = path.name[:3]
        if tag == "REB":
            continue
        if tag in countries:
            raise ValueError(f"Повторная история страны {tag}")
        # Это контракт существующего data.cpp: первая primary, все culture, последняя civilized.
        text = re.sub(r"#[^\n]*", "", read(path))
        primary = re.findall(r"\bprimary_culture\s*=\s*(\w+)", text)
        accepted = re.findall(r"(?<!\w)culture\s*=\s*(\w+)", text)
        civilized = re.findall(r"\bcivilized\s*=\s*(\w+)", text)
        if not primary or primary[0] not in unions:
            raise ValueError(f"Неизвестная основная культура: {path}")
        countries[tag] = dict(primary=primary[0], accepted=accepted,
                              civilized=bool(civilized and civilized[-1] == "yes"))
    country_text = read(root / "common/countries.txt")
    dynamic_part = country_text.split("dynamic_tags", 1)[1]
    dynamic = re.findall(r"^([A-Z][A-Z0-9]{2})\s*=", dynamic_part, re.M)
    if not dynamic:
        raise ValueError("Не найдены динамические тэги")
    default = countries[dynamic[0]]["primary"]
    if any(countries[tag]["primary"] != default for tag in dynamic):
        raise ValueError("Разные исходные культуры динамических тэгов")
    dynamic = sorted(dynamic)
    config = re.sub(r"#[^\n]*", "", read(root / "autogen.cfg"))
    excluded_block = re.search(r"\bcountries_without_culture\s*=\s*\{([^{}]*)\}", config)
    excluded = set(re.findall(r"\b[A-Z][A-Z0-9]{2}\b", excluded_block[1])) if excluded_block else set()

    def unique_choice(tags, culture):
        for condition in (lambda tag: countries[tag]["civilized"],
                          lambda tag: tag == culture[:3].upper()):
            matches = [tag for tag in tags if condition(tag)]
            if len(matches) == 1:
                return matches[0]
        return None

    rows, dynamic_index = [], 0
    for culture in order:
        primary = [tag for tag, data in countries.items()
                   if data["primary"] == culture and tag not in dynamic and tag not in excluded]
        accepted = [tag for tag, data in countries.items() for value in data["accepted"]
                    if value == culture and tag not in excluded]
        union = unions[culture]
        additional = []

        def extend(tags, union_tag, omit=None):
            additional.extend(tag for tag in tags if tag != omit)
            if union_tag and union_tag not in tags:
                additional.append(union_tag)

        if len(primary) == 1:
            main = primary[0]
            extend(accepted, None if main == union else union)
        elif len(primary) == 2 and union in primary:
            main = next(tag for tag in primary if tag != union)
            additional = accepted + [union]
        elif len(primary) > 1:
            main = unique_choice(primary, culture)
            if main is None:
                main = dynamic[dynamic_index % len(dynamic)]
                dynamic_index += 1
            extend(primary, None, main)
            extend(accepted, None if union in primary else union)
        elif len(accepted) == 1:
            main = accepted[0]
            extend(accepted, union, main)
        else:
            main = unique_choice(accepted, culture)
            if main is None:
                main = dynamic[dynamic_index % len(dynamic)]
                dynamic_index += 1
            extend(accepted, union, main)
        spread = list(dict.fromkeys([main] + additional))
        first = [tag for tag in spread if countries[tag]["primary"] == culture or tag == union
                 or (tag == main and tag in dynamic)]
        rows.append(dict(culture=culture, main=main, additional=additional, union=union,
                         dynamic_fallback=main if main in dynamic else None, first=first, spread=spread))
    return dict(countries=countries, dynamic=dynamic, rows=rows)


def signature(nodes):
    return tuple((node.key, node.value, signature(node.children)) for node in nodes)


def one(nodes, key):
    found = [node for node in nodes if node.key == key]
    if len(found) != 1:
        raise ValueError(f"Поле {key}: ожидалось одно, найдено {len(found)}")
    return found[0]


def scalar(key, value):
    return Node(key, 0, value=value)


def block(key, *children):
    return Node(key, 0, children=list(children))


def expected_trigger(row, tag, first):
    culture = row["culture"]
    owner = block("owner", block("NOT", scalar("tag", tag)))
    common = [scalar("controlled_by_rebels", "yes")]
    if first:
        recipient = [scalar("exists", "no")]
        if tag not in (row["union"], row["dynamic_fallback"]):
            recipient.append(scalar("primary_culture", culture))
        recipient.append(block("NOT", block("any_core", scalar("always", "yes"))))
        owner.children.append(block(tag, *recipient))
        return common + [scalar("culture", culture), block("NOT", scalar("is_core", tag)), owner]
    owner.children.append(block(tag, block("OR", scalar("accepted_culture", culture),
                                          scalar("primary_culture", culture))))
    return common + [block("NOT", scalar("is_core", tag)),
                     block("any_neighbor_province", scalar("is_core", tag)),
                     block("any_pop", scalar("has_pop_culture", culture)), owner]


def validate_events(events, localisation: str, model: dict) -> dict:
    expected = {(row["culture"], tag, kind): row for row in model["rows"]
                for kind in ("first", "spread") for tag in row[kind]}
    actual, ids, issues, checks = Counter(), set(), [], 0
    localised = Counter(line.split(";", 1)[0] for line in localisation.splitlines() if ";" in line)
    values = {line.split(";", 2)[0]: line.split(";", 2)[1]
              for line in localisation.splitlines() if ";" in line}
    for event in events:
        event_id = "?"
        try:
            if event.key != "province_event":
                raise ValueError("Ожидалось province_event")
            body = event.children
            event_id = one(body, "id").value
            if event_id in ids:
                raise ValueError("Повторный ID")
            ids.add(event_id)
            trigger = one(body, "trigger").children
            option = one(body, "option").children
            guarded = [node for node in option if node.key == "if"]
            kind = "first" if guarded else "spread"
            effects = one(option, "if").children if guarded else option
            tag = one(effects, "add_core").value
            culture = (one(trigger, "culture").value if guarded
                       else one(one(trigger, "any_pop").children, "has_pop_culture").value)
            pair = (culture, tag, kind)
            actual[pair] += 1
            if pair not in expected:
                raise ValueError(f"Лишняя пара {pair}")
            row = expected[pair]
            if signature(trigger) != signature(expected_trigger(row, tag, bool(guarded))):
                raise ValueError("Неверный trigger или порядок его условий")
            months = 4 * len(row["first"]) if guarded else 4
            mtth = one(body, "mean_time_to_happen").children
            if len(mtth) != 1 or float(one(mtth, "months").value) != months:
                raise ValueError(f"Неверный MTTH: ожидалось {months} месяцев без модификаторов")
            expected_effects = [scalar("add_core", tag)]
            if guarded:
                if signature(one(effects, "limit").children) != signature(trigger):
                    raise ValueError("Защита опции отличается от trigger")
                if tag == row["dynamic_fallback"]:
                    expected_effects.append(block("owner", block(tag, scalar("add_accepted_culture", culture))))
                if signature(effects) != signature([block("limit", *trigger)] + expected_effects):
                    raise ValueError("Лишние или отсутствующие эффекты первой корки")
                if len(option) != 2 or option[0].key != "name" or option[1].key != "if":
                    raise ValueError("Эффекты первой корки вне защищённого блока")
            elif signature([node for node in option if node.key != "name"]) != signature(expected_effects):
                raise ValueError("Изменены эффекты соседского распространения")
            for field, key in (("title", f"EVTNAME{event_id}"), ("desc", f"EVTDESC{event_id}")):
                if one(body, field).value != key:
                    raise ValueError(f"Неверная ссылка {field}")
            if one(option, "name").value != f"EVTOPTA{event_id}":
                raise ValueError("Неверная ссылка опции")
            for key in (f"EVTNAME{event_id}", f"EVTDESC{event_id}", f"EVTOPTA{event_id}"):
                if localised[key] != 1 or not re.search("[А-Яа-яЁё]", values.get(key, "")):
                    raise ValueError(f"Нет однозначной русской локализации {key}")
            checks += 1
        except (ValueError, TypeError, KeyError) as error:
            issues.append(f"Событие {event_id}: {error}")
    missing = sorted(set(expected) - set(actual))
    duplicates = sorted(pair for pair, count in actual.items() if count != 1)
    issues += [f"Отсутствует пара {pair}" for pair in missing]
    issues += [f"Повторная пара {pair}" for pair in duplicates]
    counts = {kind: sum(count for (_, _, k), count in actual.items() if k == kind) for kind in ("first", "spread")}
    return dict(ok=not issues, checked_events=checks, counts={**counts, "total": sum(counts.values())},
                expected={kind: sum(len(row[kind]) for row in model["rows"]) for kind in ("first", "spread")},
                culture_count=len(model["rows"]), main_dynamic=sum(bool(row["dynamic_fallback"]) for row in model["rows"]),
                first_candidates={row["culture"]: len(row["first"]) for row in model["rows"]},
                issues=issues, scope="Статическая генерация и локализация; не исполнение движком")


def validate(root: Path) -> dict:
    model = reconstruct_map(root)
    event_file, localisation_file = root / "events/adding_cores.txt", root / "localisation/adding_cores.csv"
    result = validate_events(parse(read(event_file)), read(localisation_file), model)
    result["files"] = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in (event_file, localisation_file)}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = validate(args.root.resolve())
    output = json.dumps(result, ensure_ascii=False, indent=2)
    if args.report:
        args.report.write_text(output + "\n", encoding="utf-8")
    print(output)
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
