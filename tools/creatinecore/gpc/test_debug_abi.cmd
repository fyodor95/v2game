@echo off
chcp 65001 >nul
setlocal
rem Проверяется готовая DLL; этот скрипт не собирает и не устанавливает модуль.
if not "%~2"=="" exit /b 2
if "%~1"=="" (
    set "gpc_abi_dll=%~dp0..\..\..\.work\gpc-build\ccore_great_power_culture_cores.dll"
) else (
    set "gpc_abi_dll=%~f1"
)
if not exist "%gpc_abi_dll%" (
    echo Не найден готовый артефакт GPC DLL: "%gpc_abi_dll%"
    exit /b 2
)
set "gpc_abi_vs="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "gpc_abi_vs=%%i"
if not defined gpc_abi_vs exit /b 2
call "%gpc_abi_vs%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if not exist ..\..\..\.work\gpc-build mkdir ..\..\..\.work\gpc-build
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /WX /Fo..\..\..\.work\gpc-build\debug_abi_test.obj debug_abi_test.cpp /Fe..\..\..\.work\gpc-build\debug_abi_test.exe /link /MACHINE:X86
if errorlevel 1 goto fail
..\..\..\.work\gpc-build\debug_abi_test.exe "%gpc_abi_dll%"
set "gpc_abi_result=%errorlevel%"
popd
exit /b %gpc_abi_result%
:fail
popd
exit /b 1
