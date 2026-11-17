@echo off
setlocal
set "VIC2_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VIC2_VSWHERE%" (
    >&2 echo Visual Studio Installer vswhere.exe was not found.
    exit /b 1
)
set "VIC2_VS_INSTALL="
for /f "usebackq delims=" %%I in (`"%VIC2_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VIC2_VS_INSTALL=%%I"
if not defined VIC2_VS_INSTALL (
    >&2 echo No Visual Studio installation with x86/x64 C++ tools was found.
    exit /b 1
)
call "%VIC2_VS_INSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if errorlevel 1 exit /b %errorlevel%
if not exist .build mkdir .build
cl /nologo /LD /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /Fo.build\bridge.obj native\bridge.cpp /link user32.lib /OUT:.build\vic2_test_bridge.dll /IMPLIB:.build\vic2_test_bridge.lib /MACHINE:X86
set result=%errorlevel%
popd
exit /b %result%
