"""Быстрые структурные проверки Clausewitz без исполнения игровых условий."""
from dataclasses import dataclass, field
from pathlib import Path
import re


@dataclass
class Node:
    key: str
    line: int
    value: str | None = None
    children: list = field(default_factory=list)


class ParseError(ValueError):
    pass


def parse(text: str) -> list[Node]:
    # Строка идёт раньше комментария: знак # внутри строки не начинает комментарий.
    pattern = r'"(?:\\.|[^"\\])*"|#[^\n]*|\s+|[{}=]|[^\s{}="#]+|"'
    tokens = []
    line = 1
    for match in re.finditer(pattern, text):
        value = match.group()
        start = line
        line += value.count("\n")
        if value.isspace() or value.startswith("#"):
            continue
        if value == '"':
            raise ParseError(f"Строка {start}: незакрытая кавычка")
        tokens.append((value, start))
    position = 0

    def block(nested=False):
        nonlocal position
        result = []
        while position < len(tokens):
            key, key_line = tokens[position]
            position += 1
            if key == "}":
                if not nested:
                    raise ParseError(f"Строка {key_line}: лишняя закрывающая скобка")
                return result
            if key == "{":
                raise ParseError(f"Строка {key_line}: блок без имени")
            if position >= len(tokens) or tokens[position][0] != "=":
                continue  # Списки цветов и значений допускают элементы без присваивания.
            position += 1
            if position >= len(tokens):
                raise ParseError(f"Строка {key_line}: отсутствует значение")
            value, value_line = tokens[position]
            position += 1
            if value == "{":
                result.append(Node(key, key_line, children=block(True)))
            elif value in {"}", "="}:
                raise ParseError(f"Строка {value_line}: неверное значение")
            else:
                result.append(Node(key, key_line, value=value.strip('"')))
        if nested:
            raise ParseError("Отсутствует закрывающая скобка")
        return result

    return block()


def check(root: Path) -> dict:
    issues, events, decisions = [], {}, {}
    files = sorted((root / "events").glob("*.txt")) + sorted((root / "decisions").glob("*.txt"))
    for path in files:
        try:
            nodes = parse(path.read_bytes().decode("cp1251"))
        except ParseError as error:
            issues.append({"file": str(path), "message": str(error)})
            continue
        for node in nodes:
            if node.key in {"country_event", "province_event"}:
                ids = [child for child in node.children if child.key == "id" and child.value]
                for child in ids:
                    previous = events.get(child.value)
                    if previous:
                        issues.append({"file": str(path), "line": child.line, "message": f"Повторный ID события {child.value}", "previous": previous})
                    events[child.value] = {"file": str(path), "line": child.line}
            if node.key == "political_decisions":
                for decision in node.children:
                    previous = decisions.get(decision.key)
                    if previous:
                        issues.append({"file": str(path), "line": decision.line, "message": f"Повторное решение {decision.key}", "previous": previous})
                    decisions[decision.key] = {"file": str(path), "line": decision.line}
    return {"ok": not issues, "files": len(files), "events": len(events), "decisions": len(decisions), "issues": issues,
            "coverage": "Структура блоков и строк, повторы ID событий и имён решений. Условия не исполняются."}
