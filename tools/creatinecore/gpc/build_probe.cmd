@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2026\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if not exist ..\..\..\.work\gpc-build mkdir ..\..\..\.work\gpc-build
cl /nologo /LD /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /Fo..\..\..\.work\gpc-build\probe.obj probe.cpp /link /OUT:..\..\..\.work\gpc-build\gpc_qualification.dll /IMPLIB:..\..\..\.work\gpc-build\gpc_qualification.lib /MACHINE:X86
set result=%errorlevel%
popd
exit /b %result%
