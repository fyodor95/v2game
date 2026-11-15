@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2026\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if not exist ..\..\..\.work\gpc-build mkdir ..\..\..\.work\gpc-build
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /Fo..\..\..\.work\gpc-build\calculation_test.obj calculation_test.cpp /Fe..\..\..\.work\gpc-build\calculation_test.exe
if errorlevel 1 goto fail
..\..\..\.work\gpc-build\calculation_test.exe
if errorlevel 1 goto fail
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /WX /Fo..\..\..\.work\gpc-build\scheduler_test.obj scheduler_test.cpp /Fe..\..\..\.work\gpc-build\scheduler_test.exe
if errorlevel 1 goto fail
..\..\..\.work\gpc-build\scheduler_test.exe
set result=%errorlevel%
popd
exit /b %result%
:fail
popd
exit /b 1
