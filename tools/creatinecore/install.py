"""Установить собранное ядро и профиль в указанную копию Victoria II."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_EXE = '62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    destination = args.destination.resolve()
    if digest(destination / 'v2game.exe') != EXPECTED_EXE:
        raise RuntimeError('Исполняемый файл не соответствует проверенной версии 3.04')
    build = ROOT / '.work/creatinecore/build'
    manifest = json.loads((build / 'manifest.json').read_text(encoding='utf-8'))
    files = {'dinput8.dll': 'dinput8.dll',
             'z_CreatineCore/creatinecore.dll': 'creatinecore.dll',
             'z_CreatineCore/eventmusic.dll': 'eventmusic.dll',
             'z_CreatineCore/dll_Submods/ccore_script_fixes.dll': 'ccore_script_fixes.dll',
             'z_CreatineCore/dll_Submods/ccore_great_power_culture_cores.dll': 'ccore_great_power_culture_cores.dll',
             'z_CreatineCore/great_power_culture_cores.ini': 'great_power_culture_cores.ini'}
    for name in files.values():
        if digest(build / name) != manifest['artifacts'][name]['sha256']:
            raise RuntimeError('Хеш артефакта отличается от результата сборки: ' + name)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = ROOT / '.work/install-backups' / stamp
    backup.mkdir(parents=True)
    sources = {name: build / source for name, source in files.items()}
    settings = 'z_CreatineCore/creatinecore_settings.ini'
    sources[settings] = ROOT / 'tools/creatinecore-profile/creatinecore_settings.ini'
    expected_hashes = {relative: manifest['artifacts'][source]['sha256']
                       for relative, source in files.items()}
    expected_hashes[settings] = digest(sources[settings])
    preserved_settings = {settings, 'z_CreatineCore/great_power_culture_cores.ini'}
    install_manifest = 'z_CreatineCore/install-manifest.json'
    before = {}
    for relative in [*sources, install_manifest]:
        path = destination / relative
        before[relative] = digest(path) if path.exists() else None
        if path.exists():
            saved = backup / relative
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, saved)
    (backup / 'before.json').write_text(json.dumps({'destination': str(destination), 'files': before}, indent=2), encoding='utf-8')
    attempted = []
    try:
        for relative, source in sources.items():
            # Оба INI сохраняются целиком, включая неизвестные опции и кодировку.
            if relative in preserved_settings and before[relative] is not None:
                continue
            path = destination / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            attempted.append(relative)
            shutil.copy2(source, path)
            if digest(path) != expected_hashes[relative]:
                raise RuntimeError('Ошибка проверки установленного файла: ' + relative)
        installed = {**manifest, 'installed': {relative: digest(destination / relative) for relative in sources}}
        attempted.append(install_manifest)
        (destination / install_manifest).write_text(json.dumps(installed, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    except Exception as error:
        (backup / 'result.json').write_text(json.dumps({
            'ok': False, 'partial': bool(attempted), 'destination': str(destination),
            'attempted': attempted, 'error': str(error)}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        raise RuntimeError('Установка прервана; резервная копия: ' + str(backup)) from error
    (backup / 'result.json').write_text(json.dumps({
        'ok': True, 'destination': str(destination), 'attempted': attempted}, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'ok': True, 'destination': str(destination), 'backup': str(backup)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
