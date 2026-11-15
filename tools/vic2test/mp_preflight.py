"""Read-only сравнение входных файлов двух MP-сред до bootstrap.

Проверяются все файлы game/, включая неизвестные каталоги. Узкие исключения
ниже относятся к локальным настройкам и производным данным. Отпечаток описывает
файлы на диске, а не уже загруженный мир, выбранные аргументами моды или OOS.
Вызывающий код обязан заморозить игровые входы на время проверки и запуска.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat

import cli


SCHEMA = 1
CHUNK_BYTES = 1024 * 1024
EXCLUDED_FILES = {
    "userdir.txt": "Абсолютное перенаправление проверяется отдельно для каждой среды",
    "settings.txt": "Локальные настройки отображения, звука и автосохранений",
    "messagetypes_custom.txt": "Локальные правила уведомлений и пауз",
}
EXCLUDED_DIRECTORIES = {
    "logs": "Журналы конкретного процесса",
    "z_creatinecore/logs": "Журналы и OOS-отчёты конкретного процесса",
    "map/cache": "Производный кэш карты; исходные map/ входят в отпечаток",
    "gfx/cache": "Производный графический кэш; исходные gfx/ входят в отпечаток",
}


class PreflightError(ValueError):
    """Среда не позволяет достоверно сравнить контролируемые файлы."""

    def __init__(self, message: str, *, details: dict | None = None):
        self.details = details
        if details is not None:
            message += "; " + json.dumps(details, ensure_ascii=False, sort_keys=True)
        super().__init__(message)


def _absolute(path: Path) -> Path:
    # resolve() здесь скрыл бы симлинк до обязательной проверки компонентов.
    return Path(os.path.abspath(path))


def _regular_stat(path: Path) -> os.stat_result:
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or (
        getattr(info, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT
    ):
        raise PreflightError(f"Симлинк или reparse point запрещён: {path}")
    if not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)):
        raise PreflightError(f"Неподдерживаемый тип файла: {path}")
    return info


def _checked_path(path: Path, root: Path) -> Path:
    path = _absolute(path)
    if not path.is_relative_to(root):
        raise PreflightError(f"Путь выходит за разрешённый каталог: {path}")
    _regular_stat(root)
    current = root
    for component in path.relative_to(root).parts:
        current = current / component
        _regular_stat(current)
    return path


def _identity(info: os.stat_result) -> tuple:
    if stat.S_ISDIR(info.st_mode):
        # В этой Windows-среде само iterdir() изменило st_size каталога
        # gfx/FX/textures с 0 на 28672 при прежних dev/ino/mtime и содержимом.
        # Тип, inode и mtime обязательны; состав детей проверяется отдельно.
        return "directory", info.st_dev, info.st_ino, info.st_mtime_ns
    # На Windows lstat и fstat могут давать разную семантику st_ctime
    # (время изменения metadata / время создания). inode, размер и mtime общие.
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns


def _stat_details(info: os.stat_result) -> dict:
    fields = ("st_dev", "st_ino", "st_size", "st_mtime_ns", "st_ctime_ns",
              "st_atime_ns", "st_birthtime_ns", "st_file_attributes", "st_reparse_tag")
    return {"file_type": stat.S_IFMT(info.st_mode),
            **{name: getattr(info, name, None) for name in fields}}


def _check_unchanged(path: Path, before: os.stat_result, after: os.stat_result,
                     phase: str, message: str) -> None:
    if _identity(before) == _identity(after):
        return
    a, b = _stat_details(before), _stat_details(after)
    compared = ["file_type", "st_dev", "st_ino", "st_mtime_ns"] if stat.S_ISDIR(before.st_mode) else [
        "st_dev", "st_ino", "st_size", "st_mtime_ns"]
    raise PreflightError(f"{message}: {path}", details={
        "path": str(path), "phase": phase, "compared_fields": compared,
        "changed_fields": [name for name in a if a[name] != b[name]],
        "before": a, "after": b,
    })


def _sha256(path: Path, before: os.stat_result) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        _check_unchanged(path, before, os.fstat(stream.fileno()), "before_read", "Файл изменился перед чтением")
        while block := stream.read(CHUNK_BYTES):
            result.update(block)
        after = os.fstat(stream.fileno())
    _check_unchanged(path, before, after, "after_read_handle", "Файл изменился во время проверки")
    _check_unchanged(path, before, _regular_stat(path), "after_read_path", "Файл изменился во время проверки")
    return result.hexdigest()


def _environment_paths(environment: Path) -> tuple[Path, Path, Path]:
    root = cli.ROOT.resolve(strict=True)
    work = _checked_path(root / ".work", root)
    environment = _checked_path(environment, work)
    if environment == work:
        raise PreflightError("Требуется отдельная тестовая среда внутри .work")
    metadata_path = _checked_path(environment / "environment.json", environment)
    if metadata_path.stat().st_size > CHUNK_BYTES:
        raise PreflightError("Слишком большой environment.json")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    if not isinstance(metadata, dict) or any(not isinstance(metadata.get(key), str) for key in ("game", "userdata")):
        raise PreflightError("environment.json должен содержать строки game и userdata")
    paths = []
    for key in ("game", "userdata"):
        declared = Path(metadata[key])
        if not declared.is_absolute() or _absolute(declared) != environment / key:
            raise PreflightError(f"metadata {key} должен указывать на {environment / key}")
        value = _checked_path(declared, environment)
        if not value.is_dir():
            raise PreflightError(f"Требуется каталог: {value}")
        paths.append(value)
    game, userdata = paths
    redirect = _checked_path(game / "userdir.txt", game)
    if redirect.stat().st_size > 32768:
        raise PreflightError("Слишком большой userdir.txt")
    target = Path(redirect.read_text(encoding="ascii").strip())
    if not target.is_absolute() or _absolute(target) != userdata:
        raise PreflightError(f"userdir.txt не направлен в изолированный userdata: {redirect}")
    return environment, game, userdata


def fingerprint_environment(environment: Path) -> dict:
    """Возвращает manifest и SHA-256; не обращается к процессам и не пишет файлы."""
    environment, game, userdata = _environment_paths(environment)
    entries: dict[str, dict] = {}
    excluded = []
    observed: list[tuple[Path, os.stat_result, tuple[str, ...] | None]] = []
    names: set[str] = set()

    def visit(directory: Path) -> None:
        before = _regular_stat(directory)
        children = sorted(directory.iterdir(), key=lambda item: item.name)
        observed.append((directory, before, tuple(path.name for path in children)))
        for path in children:
            info = _regular_stat(path)
            relative = path.relative_to(game).as_posix()
            key = relative.casefold()
            if key in names:
                raise PreflightError(f"Неоднозначные имена файлов для Windows: {relative}")
            names.add(key)
            is_directory = stat.S_ISDIR(info.st_mode)
            reason = (EXCLUDED_DIRECTORIES if is_directory else EXCLUDED_FILES).get(key)
            if reason:
                excluded.append({"path": relative, "reason": reason})
                continue
            if is_directory:
                entries[relative] = {"kind": "directory"}
                visit(path)
            else:
                # Имена хешируются точно: Unicode casefold способен объединить
                # разные имена Windows, например Straße и strasse.
                entries[relative] = {"kind": "file", "size": info.st_size, "sha256": _sha256(path, info)}
                observed.append((path, info, None))

    visit(game)
    if entries.get("v2game.exe", {}).get("kind") != "file":
        raise PreflightError("В тестовой среде отсутствует v2game.exe")
    # Повторная проверка обнаруживает изменение уже прочитанного файла и состава
    # каталога. Внешняя блокировка входов всё равно остаётся обязанностью стенда.
    for path, before, children in observed:
        _check_unchanged(path, before, _regular_stat(path), "tree_recheck", "Дерево изменилось во время проверки")
        if children is not None:
            after_children = tuple(sorted(child.name for child in path.iterdir()))
            if children != after_children:
                raise PreflightError(f"Состав каталога изменился во время проверки: {path}", details={
                    "path": str(path), "phase": "children_recheck",
                    "before": list(children), "after": list(after_children),
                    "added": sorted(set(after_children) - set(children)),
                    "removed": sorted(set(children) - set(after_children)),
                })
    payload = json.dumps({"schema": SCHEMA, "entries": entries}, sort_keys=True,
                         ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return {
        "environment": str(environment), "game": str(game), "userdata": str(userdata),
        "fingerprint": hashlib.sha256(payload).hexdigest(), "entries": entries,
        "files": sum(item["kind"] == "file" for item in entries.values()),
        "bytes": sum(item.get("size", 0) for item in entries.values()),
        "excluded": excluded,
    }


def compare(host: Path, client: Path) -> dict:
    """Сравнивает содержимое; при некорректной изоляции выбрасывает PreflightError.

    userdata (сейвы, кэш, логи, настройки) и служебные файлы рядом с game/
    намеренно не сравниваются: их различия не являются различием установки.
    Сейв для MP передаёт хост. Выбор модов и регенерация кэша — отдельные
    обязанности запуска; совпавший manifest не удостоверяет эти условия.
    """
    host_path, client_path = _absolute(host), _absolute(client)
    if host_path == client_path or host_path.is_relative_to(client_path) or client_path.is_relative_to(host_path):
        raise PreflightError("Хосту и клиенту нужны отдельные непересекающиеся среды")
    left, right = fingerprint_environment(host), fingerprint_environment(client)
    differences = []
    for path in sorted(left["entries"].keys() | right["entries"].keys()):
        a, b = left["entries"].get(path), right["entries"].get(path)
        if a != b:
            kind = "only_client" if a is None else "only_host" if b is None else "type" if a["kind"] != b["kind"] else "content"
            differences.append({"path": path, "difference": kind, "host": a, "client": b})
    return {
        "ok": not differences, "schema": SCHEMA, "algorithm": "sha256",
        "fingerprint": left["fingerprint"] if not differences else None,
        "scope": "game_input_files", "differences": differences,
        "host": {key: value for key, value in left.items() if key != "entries"},
        "client": {key: value for key, value in right.items() if key != "entries"},
        "limitations": [
            "Сравниваются файлы на диске; запущенные процессы могли загрузить другую версию",
            "Выбор модов, состояние мира и согласованность MP проверяются отдельно",
            "Кэш исключён как производный; его регенерацию обеспечивает подготовка стенда",
            "userdata и session metadata различаются между участниками и не входят в fingerprint",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--client", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = compare(args.host, args.client)
    except (OSError, ValueError, UnicodeError) as error:
        result = {"ok": False, "error": "E_MP_PREFLIGHT", "message": str(error)}
        if isinstance(error, PreflightError) and error.details is not None:
            result["details"] = error.details
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
