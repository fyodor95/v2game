"""Изолированная сборка и проверки нативного модуля; игру не запускает."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[3]
SOURCE = Path(__file__).resolve().parent
OUTPUT = ROOT / '.work/culture-religion-native'


def main():
    finder = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    developer = Path(install) / 'Common7/Tools/VsDevCmd.bat'
    upstream = ROOT / '.work/creatinecore/upstream/Source/V2/V2'
    OUTPUT.mkdir(parents=True, exist_ok=True)
    commands = ['@echo off', 'setlocal',
        f'call "{developer}" -arch=x86 -host_arch=x64', 'if errorlevel 1 exit /b 1',
        f'cd /d "{OUTPUT}"',
        f'cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 "{SOURCE / "calculation.cpp"}" "{SOURCE / "calculation_test.cpp"}" /Fe:calculation_test.exe',
        'if errorlevel 1 exit /b 1', 'calculation_test.exe', 'if errorlevel 1 exit /b 1',
        f'cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 "{SOURCE / "calculation.cpp"}" "{SOURCE / "native.cpp"}" "{SOURCE / "native_reader_test.cpp"}" /Fe:native_reader_test.exe',
        'if errorlevel 1 exit /b 1', 'native_reader_test.exe', 'if errorlevel 1 exit /b 1',
        f'cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /I"{upstream}" "{SOURCE / "calculation.cpp"}" "{SOURCE / "effect.cpp"}" "{SOURCE / "factory_test.cpp"}" /Fe:factory_test.exe',
        'if errorlevel 1 exit /b 1', 'factory_test.exe', 'if errorlevel 1 exit /b 1',
        f'cl /nologo /c /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /I"{upstream}" "{SOURCE / "effect.cpp"}"',
        'if errorlevel 1 exit /b 1',
        f'cl /nologo /LD /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /I"{upstream}" "{SOURCE / "native.cpp"}" "{SOURCE / "ui.cpp"}" /link /OUT:ccore_state_religion_ui.dll /MACHINE:X86',
        'exit /b %errorlevel%']
    script = OUTPUT / 'build.cmd'
    script.write_bytes(('\r\n'.join(commands) + '\r\n').encode('ascii'))
    subprocess.run(['cmd.exe', '/d', '/c', str(script)], check=True)


if __name__ == '__main__':
    main()
