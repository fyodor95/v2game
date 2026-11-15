@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2026\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if not exist ..\..\..\.work\gpc-build mkdir ..\..\..\.work\gpc-build
cl /nologo /LD /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /wd4505 /Fo..\..\..\.work\gpc-build\runtime.obj runtime.cpp /link /OUT:..\..\..\.work\gpc-build\ccore_great_power_culture_cores.dll /IMPLIB:..\..\..\.work\gpc-build\ccore_great_power_culture_cores.lib /MACHINE:X86
if errorlevel 1 goto fail
python write_manifest.py
set result=%errorlevel%
popd
exit /b %result%
:fail
popd
exit /b 1
