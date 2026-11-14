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
    patch = Path(__file__).with_name("v2game.patch")
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
    output = original.with_name("V2.v2game.vcxproj")
    tree.write(output, encoding="utf-8", xml_declaration=True)
    return output


def build() -> None:
    prepare()
    BUILD.mkdir(exist_ok=True)
    tool = str(msbuild_path())
    projects = [(core_project(), "core"), (UPSTREAM / "Source/V2/V2/V2Proxy.vcxproj", "proxy"), (UPSTREAM / "Source/V2/V2/EventMusic.vcxproj", "music")]
    generate(UPSTREAM, WORK / "script_fixes.cpp")
    shutil.copyfile(UPSTREAM / "Source/V2/V2/V2ModAPI.h", WORK / "V2ModAPI.h")
    shutil.copyfile(Path(__file__).with_name("script_fixes.vcxproj"), WORK / "script_fixes.vcxproj")
    projects.append((WORK / "script_fixes.vcxproj", "script-fixes"))
    for project, name in projects:
        run(tool, str(project), "/p:Configuration=Release", "/p:Platform=Win32", f"/p:OutDir={BUILD}\\", f"/p:IntDir={WORK / ('obj-' + name)}\\", "/m:1", "/v:minimal", "/nologo")
    artifacts = {}
    for name in ("creatinecore.dll", "dinput8.dll", "eventmusic.dll", "ccore_script_fixes.dll"):
        content = (BUILD / name).read_bytes()
        pe = int.from_bytes(content[60:64], "little")
        if int.from_bytes(content[pe + 4:pe + 6], "little") != 0x14C:
            raise RuntimeError(f"Ожидалась x86 DLL: {name}")
        artifacts[name] = {"sha256": hashlib.sha256(content).hexdigest(), "bytes": len(content), "machine": "x86"}
    record = {"upstream_url": URL, "upstream_commit": COMMIT, "patch_sha256": hashlib.sha256(Path(__file__).with_name("v2game.patch").read_bytes()).hexdigest(), "msbuild": tool, "configuration": "Release|Win32", "artifacts": artifacts}
    (BUILD / "manifest.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Сборка завершена:", BUILD)


if __name__ == "__main__":
    build()
