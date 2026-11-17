"""Собирает закреплённый CreatineCore и отдельные скриптовые исправления."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

from extract_script_fixes import generate

COMMIT = "7d9f9df9b356096cde475ebd550fbb0102fa1042"
URL = "https://github.com/artyom-kuznetsov/V2DLL_Victoria-2-Extension.git"
ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / ".work/creatinecore"
UPSTREAM = WORK / "upstream"
BUILD = WORK / "build"
GPC_RESOURCES = ("events/great_power_culture_cores.txt",
                 "localisation/great_power_culture_cores.csv")
RELIGION_RESOURCES = ("interface/country_politics.gui",
                      "localisation/country_state_religion.csv")
PATCHES = ("v2game.patch", "religion.patch")


def run(*args: str, cwd: Path = ROOT) -> None:
    subprocess.run(args, cwd=cwd, check=True)


def prepare() -> None:
    WORK.mkdir(parents=True, exist_ok=True)
    if not (UPSTREAM / ".git").exists():
        run("git", "clone", "--filter=blob:none", "--no-checkout", URL, str(UPSTREAM))
        run("git", "sparse-checkout", "init", "--cone", cwd=UPSTREAM)
        run("git", "sparse-checkout", "set", "Source/V2/V2", "Source/v2dll_country_daily_optimizer", "CREATINECORE_LATEST_BUILD", cwd=UPSTREAM)
        run("git", "checkout", "--detach", COMMIT, cwd=UPSTREAM)
    actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=UPSTREAM, text=True).strip()
    if actual != COMMIT:
        raise RuntimeError("Рабочее дерево исходников относится к другому коммиту")
    for name in PATCHES:
        patch = Path(__file__).with_name(name)
        clean = subprocess.run(["git", "apply", "--check", str(patch)], cwd=UPSTREAM, capture_output=True)
        if clean.returncode == 0:
            run("git", "apply", str(patch), cwd=UPSTREAM)
        else:
            # Повторный запуск разрешён только для уже применённого патча.
            run("git", "apply", "--reverse", "--check", str(patch), cwd=UPSTREAM)


def msbuild_path() -> Path:
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    install = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    path = Path(install) / "MSBuild/Current/Bin/MSBuild.exe"
    if not path.is_file():
        raise RuntimeError("Не найден MSBuild с инструментами MSVC x86")
    return path


def core_project() -> Path:
    ET.register_namespace("", "http://schemas.microsoft.com/developer/msbuild/2003")
    original = UPSTREAM / "Source/V2/V2/V2.vcxproj"
    tree = ET.parse(original)
    ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    # Вспомогательные проекты собираются отдельно, с собственными obj-каталогами.
    for target in list(tree.getroot().findall("m:Target", ns)):
        tree.getroot().remove(target)
    namespace = "{http://schemas.microsoft.com/developer/msbuild/2003}"
    group = ET.SubElement(tree.getroot(), namespace + "ItemGroup")
    for name in ("calculation.cpp", "effect.cpp", "native.cpp"):
        item = ET.SubElement(group, namespace + "ClCompile", {
            "Include": str(Path(__file__).with_name("religion") / name)})
        ET.SubElement(item, namespace + "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"
    output = original.with_name("V2.v2game.vcxproj")
    tree.write(output, encoding="utf-8", xml_declaration=True)
    return output


def dll_artifact(path: Path) -> dict:
    content = path.read_bytes()
    pe = int.from_bytes(content[60:64], "little") if len(content) >= 64 else 0
    if (content[:2] != b"MZ" or pe < 64 or pe + 6 > len(content)
            or content[pe:pe + 4] != b"PE\0\0"
            or int.from_bytes(content[pe + 4:pe + 6], "little") != 0x14C):
        raise RuntimeError(f"Ожидалась x86 DLL: {path.name}")
    return {"sha256": hashlib.sha256(content).hexdigest(), "bytes": len(content), "machine": "x86"}


def stage_gpc_artifacts(source: Path, product: Path, destination: Path,
                        expected_exe: str) -> dict:
    """Проверить весь исходный образ GPC, затем перенести его в общую сборку."""
    record = json.loads((product / "manifest.json").read_text(encoding="utf-8"))
    if (record.get("schema") != 1 or record.get("machine") != "x86"
            or record.get("expected_exe_sha256") != expected_exe):
        raise RuntimeError("Манифест GPC не соответствует проверенному EXE/ABI")
    files = sorted(source.glob("*.h")) + [source / name for name in
        ("runtime.cpp", "build_runtime.cmd", "write_manifest.py", "great_power_culture_cores.ini")]
    actual_sources = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
    if record.get("sources") != actual_sources:
        raise RuntimeError("Исходники GPC изменились после сборки; требуется новая сборка")
    binary = product / "ccore_great_power_culture_cores.dll"
    artifact = dll_artifact(binary)
    expected = {"name": binary.name, "sha256": artifact["sha256"], "bytes": artifact["bytes"]}
    if record.get("artifact") != expected:
        raise RuntimeError("Артефакт GPC отличается от его манифеста")
    ini = source / "great_power_culture_cores.ini"
    for path in (binary, ini):
        target = destination / path.name
        shutil.copy2(path, target)
        if hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(path.read_bytes()).digest():
            raise RuntimeError("Ошибка копирования артефакта GPC: " + path.name)
    return record


def stage_resources(source: Path, destination: Path, resources: tuple[str, ...]) -> dict:
    """Зафиксировать игровые ресурсы вместе с устанавливаемым пакетом."""
    artifacts = {}
    # Отсутствие любого ресурса обнаруживается до копирования первого файла.
    for relative in resources:
        path = source / relative
        if not path.is_file():
            raise RuntimeError("Отсутствует игровой ресурс: " + relative)
        content = path.read_bytes()
        artifacts[relative] = {"sha256": hashlib.sha256(content).hexdigest(),
                               "bytes": len(content)}
    for relative, record in artifacts.items():
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        # Игровые CP1251/CRLF сохраняются без декодирования и преобразований.
        shutil.copy2(source / relative, target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != record["sha256"]:
            raise RuntimeError("Ошибка копирования игрового ресурса: " + relative)
    return artifacts


def stage_gpc_resources(source: Path, destination: Path) -> dict:
    return stage_resources(source, destination, GPC_RESOURCES)


def religion_sources() -> dict:
    source = Path(__file__).with_name("religion")
    return {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(source.iterdir()) if path.is_file()}


def build() -> None:
    prepare()
    religion_inputs = religion_sources()
    BUILD.mkdir(exist_ok=True)
    tool = str(msbuild_path())
    projects = [(core_project(), "core"), (UPSTREAM / "Source/V2/V2/V2Proxy.vcxproj", "proxy"), (UPSTREAM / "Source/V2/V2/EventMusic.vcxproj", "music")]
    generate(UPSTREAM, WORK / "script_fixes.cpp")
    shutil.copyfile(UPSTREAM / "Source/V2/V2/V2ModAPI.h", WORK / "V2ModAPI.h")
    shutil.copyfile(Path(__file__).with_name("script_fixes.vcxproj"), WORK / "script_fixes.vcxproj")
    projects.append((WORK / "script_fixes.vcxproj", "script-fixes"))
    projects.append((Path(__file__).with_name("religion") / "ui.vcxproj", "religion-ui"))
    for project, name in projects:
        run(tool, str(project), "/t:Rebuild", "/p:Configuration=Release", "/p:Platform=Win32", f"/p:OutDir={BUILD}\\", f"/p:IntDir={WORK / ('obj-' + name)}\\", "/m:1", "/v:minimal", "/nologo")
    gpc_source = Path(__file__).with_name("gpc")
    run("cmd.exe", "/d", "/c", str(gpc_source / "build_runtime.cmd"))
    gpc = stage_gpc_artifacts(gpc_source, ROOT / ".work/gpc-build", BUILD,
                              hashlib.sha256((ROOT / "v2game.exe").read_bytes()).hexdigest())
    artifacts = {}
    for name in ("creatinecore.dll", "dinput8.dll", "eventmusic.dll", "ccore_script_fixes.dll",
                 "ccore_great_power_culture_cores.dll", "ccore_state_religion_ui.dll"):
        artifacts[name] = dll_artifact(BUILD / name)
    ini = BUILD / "great_power_culture_cores.ini"
    artifacts[ini.name] = {"sha256": hashlib.sha256(ini.read_bytes()).hexdigest(), "bytes": ini.stat().st_size}
    artifacts.update(stage_gpc_resources(ROOT, BUILD))
    artifacts.update(stage_resources(ROOT, BUILD, RELIGION_RESOURCES))
    record = {"upstream_url": URL, "upstream_commit": COMMIT, "patch_sha256": hashlib.sha256(Path(__file__).with_name("v2game.patch").read_bytes()).hexdigest(), "msbuild": tool, "configuration": "Release|Win32", "artifacts": artifacts, "great_power_culture_cores": gpc}
    record["patches"] = {name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                         for name in PATCHES}
    if religion_sources() != religion_inputs:
        raise RuntimeError("Исходники религии изменились во время сборки")
    record["state_religion_sources"] = religion_inputs
    (BUILD / "manifest.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Сборка завершена:", BUILD)


if __name__ == "__main__":
    build()
