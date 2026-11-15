"""Изолированный запуск Victoria II и нативные команды тестового плагина."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid
import queue
import threading

EXE_HASH = "62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648"
ROOT = Path(__file__).resolve().parents[2]
EXCLUDED = {".git", ".work", "AGENTS", "docs", "tools", "temp", "zRezerv", "Собрания", "Screenshots"}


def hide_windows(pid: int) -> dict:
    """Скрывает только окна созданного тестового процесса и проверяет результат."""
    user = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    hidden, visible = [], []

    @callback_type
    def visit(window, _):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == pid:
            if user.IsWindowVisible(window):
                user.ShowWindow(window, 0)
                hidden.append(int(window))
            if user.IsWindowVisible(window):
                visible.append(int(window))
        return True

    if not user.EnumWindows(visit, 0):
        raise OSError(ctypes.get_last_error(), "Не удалось проверить окна тестового процесса")
    if visible:
        raise ValueError("Окна тестового процесса остались видимыми")
    return {"hidden_windows": len(hidden), "visible_windows": len(visible)}


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def emit(result: dict) -> int:
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


def archive_logs(environment: Path, game: Path, token: str) -> dict:
    """Сохранить журналы завершённого экземпляра перед созданием нового."""
    environment, game = environment.resolve(), game.resolve()
    if game != environment / 'game' or not environment.is_relative_to((ROOT / '.work').resolve()):
        raise ValueError('Архивировать можно только журналы изолированной тестовой среды')
    history = environment / 'log-history' / token
    if history.resolve().is_relative_to(environment) is False:
        raise ValueError('Каталог истории журналов выходит из тестовой среды')
    saved = {}
    for name in ('v2dll.log', 'v2dll_oos.log'):
        source = game / 'z_CreatineCore/Logs' / name
        if not source.resolve().is_relative_to(game):
            raise ValueError('Журнал находится вне тестовой копии')
        if not source.exists():
            continue
        if not source.is_file():
            raise ValueError('Журнал находится вне тестовой копии')
        history.mkdir(parents=True, exist_ok=True)
        target = history / name
        if target.exists():
            raise FileExistsError('Архив журнала уже существует')
        source.rename(target)
        saved[name] = str(target)
    return saved


def prepare(destination: Path, saves: list[Path]) -> dict:
    """Копирует данные, никогда не связывая их с оригиналами жёсткими ссылками."""
    destination = destination.resolve()
    work = (ROOT / ".work").resolve()
    if not destination.is_relative_to(work):
        raise ValueError("Тестовый каталог должен находиться внутри .work")
    if destination.exists():
        raise ValueError("Тестовый каталог уже существует; выберите новое имя")
    if digest(ROOT / "v2game.exe") != EXE_HASH:
        raise ValueError("Исполняемый файл отличается от поддерживаемого 3.04")
    destination.mkdir(parents=True)
    game, user = destination / "game", destination / "userdata"
    game.mkdir()
    (user / "save games").mkdir(parents=True)
    for item in ROOT.iterdir():
        if item.name in EXCLUDED or item.name.startswith("autogen"):
            continue
        target = game / item.name
        if item.is_dir():
            shutil.copytree(item, target)
        else:
            shutil.copy2(item, target)
    original_saves = []
    for save in saves:
        save = save.resolve(strict=True)
        target = user / "save games" / save.name
        if target.exists():
            raise ValueError("Повторяющееся имя сохранения")
        shutil.copy2(save, target)
        original_saves.append({"source": str(save), "sha256": digest(save), "copy": str(target)})
    (game / "userdir.txt").write_text(user.as_posix() + "/", encoding="ascii")
    settings = (game / "settings.txt").read_bytes().decode("cp1251")
    settings = re.sub(r"fullScreen\s*=\s*yes", "fullScreen=no", settings)
    settings = re.sub(r"borderless\s*=\s*yes", "borderless=no", settings)
    settings = re.sub(r"autosave\s*=\s*\"[^\"]*\"", 'autosave="NEVER"', settings)
    (user / "settings.txt").write_bytes(settings.encode("cp1251"))
    messages = ROOT / "messagetypes_custom.txt"
    if messages.is_file():
        quiet = messages.read_bytes().decode("cp1251")
        quiet = re.sub(r"\b(popup|pausepopup)\s*=\s*yes\b", r"\1=no", quiet)
        (user / "messagetypes_custom.txt").write_bytes(quiet.encode("cp1251"))
    metadata = {"game": str(game), "userdata": str(user), "sources": original_saves}
    (destination / "environment.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    return {"ok": True, **metadata, "campaign_loaded": False}


def start(environment: Path, hidden: bool, save: str | None = None, mods: list[str] | None = None) -> dict:
    metadata = json.loads((environment / "environment.json").read_text(encoding="utf-8"))
    game = Path(metadata["game"]).resolve()
    if not game.is_relative_to((ROOT / ".work").resolve()):
        raise ValueError("Запуск разрешён только для тестовой копии")
    if digest(game / "v2game.exe") != EXE_HASH:
        raise ValueError("Неподдерживаемый EXE")
    if save and (Path(save).name != save or not (Path(metadata["userdata"]) / "save games" / save).is_file()):
        raise ValueError("Копия сохранения не найдена в тестовом каталоге")
    previous_session = environment / "session.json"
    if previous_session.exists():
        previous = json.loads(previous_session.read_text(encoding="utf-8"))
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.OpenProcess(0x100000, False, previous["pid"])
        if handle:
            try:
                if kernel.WaitForSingleObject(handle, 0) == 258:
                    raise ValueError("Предыдущая тестовая сессия ещё работает; сначала завершите её")
            finally:
                kernel.CloseHandle(handle)
    poison = environment / "poisoned.json"
    if poison.exists():
        poison.rename(environment / ("poisoned-" + uuid.uuid4().hex + ".json"))
    arguments = [str(game / "v2game.exe")]
    for mod in mods or []:
        descriptor = (game / "mod" / mod).resolve()
        if Path(mod).name != mod or not descriptor.is_file() or descriptor.suffix != ".mod":
            raise ValueError("Описание тестового мода не найдено")
        arguments.append("-mod=mod/" + mod)
    token = uuid.uuid4().hex
    env = os.environ.copy()
    env["VIC2_TEST_SESSION"] = token
    startup = subprocess.STARTUPINFO()
    if hidden:
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    history = archive_logs(environment, game, token)
    log = game / 'z_CreatineCore/Logs/v2dll.log'
    log_offsets = {'v2dll.log': log.stat().st_size if log.exists() else 0}
    process = subprocess.Popen(arguments, cwd=game, env=env, startupinfo=startup)
    state = {"pid": process.pid, "token": token, "game": str(game), "campaign_loaded": False,
             "log_offsets": log_offsets, "log_history": history}
    (environment / "session.json").write_text(json.dumps(state, indent=2), encoding="utf-8")
    if save:
        result = wait_request(environment, "open-single-player", 600)
        if not result.get("ok"):
            return result
        result = wait_request(environment, "load-save " + save, 60)
        if not result.get("ok"):
            return result
        deadline = time.monotonic() + 600
        while time.monotonic() < deadline:
            result = request(environment, "frontend-status", 180)
            if result.get("save_loaded"):
                break
            if not result.get("ok"):
                return result
            time.sleep(0.5)
        else:
            return {"ok": False, "error": "E_LOAD_TIMEOUT", "message": "Загрузка сохранения не завершилась"}
        result = wait_request(environment, "play", 60)
        if not result.get("ok"):
            return result
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            result = request(environment, "status", 15)
            if result.get("campaign_loaded"):
                visibility = hide_windows(process.pid) if hidden else {}
                return {**request(environment, "pause", 15), **state, "campaign_loaded": True, **visibility}
            time.sleep(0.5)
        return {"ok": False, "error": "E_START_TIMEOUT", "message": "Не удалось войти в кампанию"}
    visibility = hide_windows(process.pid) if hidden else {}
    return {"ok": True, **state, **visibility, "message": "Процесс запущен без автоматической загрузки кампании."}


def wait_request(environment: Path, command: str, timeout: float) -> dict:
    deadline = time.monotonic() + timeout
    while True:
        # Первый запрос может ожидать завершения загрузки игровых данных.
        # После доставки запрос нельзя повторять: истёкший срок отравляет сессию.
        result = request(environment, command, max(0.1, deadline - time.monotonic()))
        if result.get("ok") or result.get("error") not in {"E_NO_PIPE", "E_NOT_READY"}:
            return result
        if time.monotonic() >= deadline:
            return result
        time.sleep(0.2)


def run_scenario(environment: Path, scenario: Path) -> dict:
    """Исполняет реальные CLI-команды и сравнивает их результаты, без подмены движка."""
    definition = json.loads(scenario.read_text(encoding="utf-8"))
    steps = definition.get("steps")
    if not isinstance(steps, list) or not steps:
        raise ValueError("Сценарий должен содержать непустой список steps")
    results = []
    allowed = {"status", "snapshot", "decision", "event", "advance", "pause", "save", "stop"}
    for index, step in enumerate(steps):
        if not isinstance(step, dict):
            raise ValueError(f"Шаг {index}: ожидается объект")
        arguments = step.get("args")
        if not isinstance(arguments, list) or not arguments or not all(isinstance(a, str) for a in arguments):
            raise ValueError(f"Шаг {index}: args должен быть списком строк")
        if arguments[0] not in allowed or "--environment" in arguments:
            raise ValueError(f"Шаг {index}: недопустимая команда или переопределение среды")
        try:
            result = subprocess.run([sys.executable, "-X", "utf8", str(Path(__file__).resolve()), *arguments,
                                     "--environment", str(environment)], capture_output=True, text=True, encoding="utf-8", timeout=600)
        except subprocess.TimeoutExpired:
            (environment / "poisoned.json").write_text(json.dumps({"error": "E_SCENARIO_TIMEOUT"}), encoding="utf-8")
            return {"ok": False, "error": "E_SCENARIO_TIMEOUT", "step": index, "results": results,
                    "message": "Срок выполнения шага истёк; сессию нужно перезапустить"}
        try:
            value = json.loads(result.stdout)
        except json.JSONDecodeError:
            return {"ok": False, "error": "E_SCENARIO_OUTPUT", "step": index,
                    "message": result.stderr or result.stdout, "results": results}
        expected = step.get("expect", {"ok": True})
        if not isinstance(expected, dict):
            raise ValueError(f"Шаг {index}: expect должен быть объектом")
        matched = all(key in value and value[key] == expected_value for key, expected_value in expected.items())
        delta = step.get("delta")
        if delta:
            if not isinstance(delta, dict) or not all(key in delta for key in ("from", "field", "value")):
                raise ValueError(f"Шаг {index}: delta должен содержать from, field и value")
            reference = delta.get("from")
            field = delta.get("field")
            if not isinstance(reference, int) or not 0 <= reference < len(results):
                raise ValueError(f"Шаг {index}: неверная ссылка delta.from")
            before, after = results[reference]["result"].get(field), value.get(field)
            matched = matched and isinstance(before, (int, float)) and isinstance(after, (int, float)) and after - before == delta["value"]
        results.append({"args": arguments, "result": value, "passed": matched})
        if not matched:
            return {"ok": False, "error": "E_ASSERTION", "step": index, "results": results}
    return {"ok": True, "results": results}


def request(environment: Path, command: str, timeout: float) -> dict:
    """Ограничивает весь обмен, включая зависший ReadFile, общим сроком."""
    poison = environment / "poisoned.json"
    if poison.exists() and command != "stop":
        return {"ok": False, "error": "E_SESSION_POISONED", "message": "После тайм-аута сессия требует перезапуска"}
    results = queue.Queue(maxsize=1)
    connected, cancelled = threading.Event(), threading.Event()

    def exchange():
        try:
            results.put(_blocking_request(environment, command, timeout, connected, cancelled))
        except Exception as error:
            results.put({"ok": False, "error": "E_IPC", "message": str(error)})

    worker = threading.Thread(target=exchange, daemon=True)
    worker.start()
    worker.join(timeout)
    if worker.is_alive():
        cancelled.set()
        # Отмена системного I/O не отменяет уже начатый игровой эффект.
        in_flight = connected.is_set()
        if in_flight:
            poison.write_text(json.dumps({"error": "E_IPC_TIMEOUT"}), encoding="utf-8")
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenThread.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
        kernel.OpenThread.restype = ctypes.c_void_p
        kernel.CancelSynchronousIo.argtypes = [ctypes.c_void_p]
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.OpenThread(1, False, worker.native_id)
        if handle:
            kernel.CancelSynchronousIo(handle)
            kernel.CloseHandle(handle)
        if not in_flight:
            return {"ok": False, "error": "E_PIPE_BUSY", "message": "Канал занят или недоступен; команда не отправлена"}
        return {"ok": False, "error": "E_IPC_TIMEOUT", "message": "Срок ответа истёк; результат игрового эффекта неизвестен, сессию нужно перезапустить"}
    result = results.get()
    if result.get("error") in {"E_NATIVE_EXCEPTION", "E_TIMEOUT", "E_SESSION_POISONED", "E_IPC"}:
        poison.write_text(json.dumps({"error": result["error"]}), encoding="utf-8")
    return result


def _blocking_request(environment: Path, command: str, timeout: float,
                      connected: threading.Event, cancelled: threading.Event) -> dict:
    metadata = json.loads((environment / "session.json").read_text(encoding="utf-8"))
    token = metadata["token"]
    if not re.fullmatch(r"[a-z0-9-]{1,79}", token):
        raise ValueError("Неверный идентификатор сессии")
    path = "\\\\.\\pipe\\vic2test-" + token
    deadline = time.monotonic() + timeout
    while True:
        if cancelled.is_set() or time.monotonic() >= deadline:
            return {"ok": False, "error": "E_PIPE_BUSY", "message": "Команда не отправлена до истечения срока"}
        try:
            with open(path, "r+b", buffering=0) as pipe:
                connected.set()
                if cancelled.is_set():
                    return {"ok": False, "error": "E_PIPE_BUSY", "message": "Отправка команды отменена"}
                pipe.write(command.encode("ascii") + b"\n")
                response = json.loads(pipe.read(4096).decode("utf-8"))
                if not isinstance(response, dict):
                    raise ValueError("Неверный формат ответа нативного плагина")
                if response.get("error"):
                    response["message"] = {
                        "E_NOT_READY": "Кампания не загружена",
                        "E_NOT_FOUND": "Страна или решение не найдены",
                        "E_CONDITIONS": "Условия решения не выполнены",
                        "E_PLAYER_ONLY": "Выполнение поддерживает только страну игрока",
                        "E_UNVERIFIED_OPERATION": "Операция ещё не реализована",
                        "E_TIMEOUT": "Поток движка не ответил; перезапустите тестовую сессию",
                        "E_NATIVE_EXCEPTION": "Ошибка движка; перезапустите тестовую сессию",
                        "E_EVENT_PENDING": "Предыдущее событие ещё ожидает выбора опции",
                        "E_SESSION_POISONED": "Тестовая сессия повреждена; перезапустите её",
                        "E_EVENT_MISMATCH": "Окно события не соответствует запрошенному событию или стране",
                        "E_EVENT_SCOPE": "Тип события не соответствует выбранной области: страна или провинция",
                        "E_EXISTS": "Сохранение с таким именем уже существует",
                        "E_ARGUMENT": "Недопустимые параметры команды",
                    }.get(response["error"], "Ошибка нативного плагина")
                return response
        except (FileNotFoundError, PermissionError):
            if time.monotonic() >= deadline:
                return {"ok": False, "error": "E_NO_PIPE", "message": "Тестовый плагин не открыл управляющий канал"}
            time.sleep(0.1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    p = commands.add_parser("run", help="Выполнить JSON-сценарий и проверки результатов")
    p.add_argument("scenario", type=Path)
    p.add_argument("--environment", type=Path, required=True)
    p = commands.add_parser("check", help="Проверить структуру скриптов и повторные идентификаторы")
    p.add_argument("--root", type=Path, default=ROOT)
    p = commands.add_parser("prepare", help="Создать отдельную тестовую копию")
    p.add_argument("--destination", type=Path, required=True)
    p.add_argument("--save", type=Path, action="append", default=[])
    p = commands.add_parser("start", help="Запустить тестовую копию; загрузка сейва пока отдельно")
    p.add_argument("--environment", type=Path, required=True)
    p.add_argument("--hidden", action="store_true")
    p.add_argument("--save", help="Имя скопированного сохранения для автоматической загрузки")
    p.add_argument("--mod", action="append", default=[], help="Имя .mod в тестовом каталоге mod")
    p = commands.add_parser("status", help="Получить состояние от нативного плагина")
    p.add_argument("--environment", type=Path, required=True)
    p = commands.add_parser("culture-cores", help="Диагностика нативного модуля национальных провинций")
    p.add_argument("operation", choices=["status", "preview", "snapshot", "run"])
    p.add_argument("--environment", type=Path, required=True)
    p.add_argument("--name", help="Новое имя JSON; по умолчанию уникальное")
    p = commands.add_parser("mp", help="Штатные сетевые действия движка без мыши и клавиатуры")
    p.add_argument("operation", choices=["state", "open", "lobby", "front-state", "host", "join", "load", "country", "play", "ready", "pause", "speed", "events", "option", "save-menu", "save"])
    p.add_argument("--environment", type=Path, required=True)
    p.add_argument("--player")
    p.add_argument("--name")
    p.add_argument("--address")
    p.add_argument("--save")
    p.add_argument("--tag")
    p.add_argument("--paused", choices=["yes", "no"])
    p.add_argument("--level", type=int)
    p.add_argument("--id", type=int)
    p.add_argument("--option", type=int)
    p.add_argument("--timeout", type=float, default=600)
    p = commands.add_parser("stop", help="Завершить только тестовую сессию без сохранения")
    p.add_argument("--environment", type=Path, required=True)
    p = commands.add_parser("save", help="Вызвать штатное сохранение в тестовом каталоге")
    p.add_argument("--environment", type=Path, required=True)
    p.add_argument("--name", required=True)
    p = commands.add_parser("decision", help="Проверить или поставить решение в очередь")
    p.add_argument("operation", choices=["check", "execute"])
    p.add_argument("--environment", type=Path, required=True)
    p.add_argument("--tag", required=True)
    p.add_argument("--id", required=True)
    p = commands.add_parser("snapshot", help="Получить нативный снимок казны страны")
    p.add_argument("--environment", type=Path, required=True)
    p.add_argument("--tag", required=True)
    p = commands.add_parser("pause", help="Поставить кампанию на паузу")
    p.add_argument("--environment", type=Path, required=True)
    p = commands.add_parser("advance", help="Пройти дни и снова остановить симуляцию")
    p.add_argument("--environment", type=Path, required=True)
    p.add_argument("--days", type=int, required=True)
    p.add_argument("--timeout", type=float, default=300)
    p.add_argument("--auto-events", action="store_true", help="Во время этого тестового прогона выбирать первые опции фоновых событий")
    p = commands.add_parser("event", help="Вызвать событие страны или провинции и выбрать опцию")
    p.add_argument("--environment", type=Path, required=True)
    scope = p.add_mutually_exclusive_group(required=True)
    scope.add_argument("--tag", help="Тег страны для country_event")
    scope.add_argument("--province", type=int, help="Номер провинции игрока для province_event")
    p.add_argument("--id", required=True)
    p.add_argument("--option", type=int, help="Номер опции с нуля; поддерживается для события страны игрока")
    args = parser.parse_args()
    try:
        if args.command == "run":
            return emit(run_scenario(args.environment, args.scenario))
        if args.command == "check":
            from scripts import check
            return emit(check(args.root))
        if args.command == "prepare":
            return emit(prepare(args.destination, args.save))
        if args.command == "start":
            return emit(start(args.environment, args.hidden, args.save, args.mod))
        if args.command == "culture-cores":
            if args.operation == 'status':
                return emit(request(args.environment, "culture-cores-status", 30))
            name = args.name or ('gpc_' + uuid.uuid4().hex)
            if not re.fullmatch(r'[A-Za-z0-9_-]{1,48}', name):
                raise ValueError('Имя GPC JSON: 1..48 ASCII-букв, цифр, дефисов или подчёркиваний')
            result = request(args.environment, 'culture-cores-' + args.operation + ' ' + name, 180)
            if result.get('ok'):
                path = Path(result['path']).resolve()
                metadata = json.loads((args.environment / 'environment.json').read_text(encoding='utf-8'))
                allowed = (Path(metadata['userdata']) / 'gpc-debug').resolve()
                if not path.is_relative_to(allowed) or not path.is_file():
                    raise ValueError('Нативный снимок не найден в изолированном userdata')
                result.update(sha256=digest(path), bytes=path.stat().st_size)
            return emit(result)
        if args.command == "mp":
            fields = {"host": [args.player, args.name], "join": [args.player, args.address],
                      "load": [args.save], "country": [args.tag],
                      "pause": [None if args.paused is None else ("1" if args.paused == "yes" else "0")],
                      "option": [None if args.id is None else str(args.id), None if args.option is None else str(args.option)],
                      "save": [args.name],
                      "speed": [None if args.level is None else str(args.level)]}.get(args.operation, [])
            if args.timeout <= 0:
                raise ValueError("Срок сетевой команды должен быть положительным")
            if any(not value or not re.fullmatch(r"[A-Za-z0-9_.-]{1,63}", value) for value in fields):
                raise ValueError("Укажите необходимые аргументы сетевой команды; допустимы ASCII-слова без пробелов")
            return emit(request(args.environment, " ".join(["mp-" + args.operation, *fields]), args.timeout))
        command = "status"
        if args.command == "stop":
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
            kernel.OpenProcess.restype = ctypes.c_void_p
            kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            metadata = json.loads((args.environment / "session.json").read_text(encoding="utf-8"))
            handle = kernel.OpenProcess(0x100000, False, metadata["pid"])
            if not handle:
                return emit({"ok": True, "process_exited": True})
            try:
                try:
                    result = request(args.environment, "stop", 15)
                except BrokenPipeError:
                    result = {"ok": True, "quit_requested": True}
                exited = kernel.WaitForSingleObject(handle, 30000) == 0
                return emit({**result, "ok": bool(result.get("ok")) and exited, "process_exited": exited})
            finally:
                kernel.CloseHandle(handle)
        if args.command == "save":
            if not re.fullmatch(r"[A-Za-z0-9_-]{1,15}", args.name):
                raise ValueError("Имя сохранения: 1..15 латинских букв, цифр, дефисов или подчёркиваний")
            metadata = json.loads((args.environment / "environment.json").read_text(encoding="utf-8"))
            output = Path(metadata["userdata"]) / "save games" / (args.name + ".v2")
            if output.exists():
                raise ValueError("Выходной файл уже существует; выберите новое имя")
            result = request(args.environment, "save-native " + args.name, 180)
            if result.get("ok"):
                if not output.is_file() or not output.stat().st_size:
                    return emit({"ok": False, "error": "E_SAVE_MISSING", "message": "Движок не создал непустое сохранение"})
                return emit({**result, "path": str(output), "sha256": digest(output), "size": output.stat().st_size})
            return emit(result)
        if args.command == "decision":
            if not re.fullmatch(r"[A-Z0-9]{3}", args.tag) or not re.fullmatch(r"[A-Za-z0-9_]{1,500}", args.id):
                raise ValueError("Неверный тег страны или идентификатор решения")
            command = f"decision-{args.operation} {args.tag} {args.id}"
        if args.command == "event":
            if not re.fullmatch(r"[0-9]{1,10}", args.id) or not 1 <= int(args.id) <= 2147483647:
                raise ValueError("Неверный номер события")
            if args.province is not None:
                if not 1 <= args.province <= 99999:
                    raise ValueError("Номер провинции должен находиться в пределах 1..99999")
                target, operation, selection = str(args.province), "event-fire-province", "event-select-province"
            else:
                if not re.fullmatch(r"[A-Z0-9]{3}", args.tag):
                    raise ValueError("Неверный тег страны")
                target, operation, selection = args.tag, "event-fire", "event-select"
            command = f"{operation} {target} {args.id}"
            if args.option is not None:
                if not 0 <= args.option <= 99:
                    raise ValueError("Номер опции должен находиться в пределах 0..99")
                result = request(args.environment, f"{operation}-chosen {target} {args.id}", 15)
                if not result.get("ok"):
                    return emit(result)
                return emit(wait_request(args.environment, f"{selection} {target} {args.option}", 30))
        if args.command == "snapshot":
            if not re.fullmatch(r"[A-Z0-9]{3}", args.tag):
                raise ValueError("Неверный тег страны")
            command = f"snapshot {args.tag}"
        if args.command == "pause":
            command = "pause"
        if args.command == "advance":
            if not 1 <= args.days <= 10000:
                raise ValueError("Число дней должно находиться в пределах 1..10000")
            if args.timeout <= 0:
                raise ValueError("Срок прогона должен быть положительным")
            operation = "advance-auto" if args.auto_events else "advance"
            result = request(args.environment, f"{operation} {args.days}", 15)
            if not result.get("ok"):
                return emit(result)
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                current = request(args.environment, "status", 15)
                if not current.get("ok"):
                    return emit(current)
                if current.get("advance_target") == 0 and current.get("paused"):
                    return emit({**current, "requested_target_date_raw": result["target_date_raw"], "completed": True})
                if current.get("event_windows", 0) and not args.auto_events:
                    paused = request(args.environment, "pause", 15)
                    return emit({"ok": False, "error": "E_EVENT_BLOCKED", "cancelled": bool(paused.get("ok")),
                                 "state": current, "message": "Событие ожидает выбора; прогон остановлен. Для фонового регрессионного прогона можно явно задать --auto-events"})
                time.sleep(0.2)
            cancelled = request(args.environment, "pause", 15)
            if cancelled.get("ok"):
                state = request(args.environment, "status", 15)
                return emit({"ok": False, "error": "E_ADVANCE_TIMEOUT", "cancelled": True,
                             "state": state, "message": "Срок прогона истёк; дальнейшее продвижение отменено и игра поставлена на паузу"})
            (args.environment / "poisoned.json").write_text(json.dumps({"error": "E_ADVANCE_TIMEOUT"}), encoding="utf-8")
            return emit({"ok": False, "error": "E_ADVANCE_TIMEOUT", "cancelled": False,
                         "message": "Срок прогона истёк; остановка не подтверждена, тестовую сессию нужно завершить"})
        return emit(request(args.environment, command, 15))
    except (OSError, ValueError) as error:
        return emit({"ok": False, "error": "E_CLIENT", "message": str(error)})


if __name__ == "__main__":
    sys.exit(main())
