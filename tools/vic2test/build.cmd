@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2026\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if not exist .build mkdir .build
cl /nologo /LD /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /Fo.build\bridge.obj native\bridge.cpp /link user32.lib /OUT:.build\vic2_test_bridge.dll /IMPLIB:.build\vic2_test_bridge.lib /MACHINE:X86
set result=%errorlevel%
popd
exit /b %result%
