@echo off
setlocal
chcp 65001 >nul
rem Фиксированный протокол: docs/plans/gpc-distribution.md. JSON только в stdout.
call "C:\Program Files\Microsoft Visual Studio\2026\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 1>&2
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if not exist ..\..\..\.work\gpc-distribution mkdir ..\..\..\.work\gpc-distribution
if errorlevel 1 goto fail
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /Fo..\..\..\.work\gpc-distribution\scheduler_distribution.obj /Fd..\..\..\.work\gpc-distribution\scheduler_distribution.pdb /Fe..\..\..\.work\gpc-distribution\scheduler_distribution.exe scheduler_distribution.cpp /link /PDB:..\..\..\.work\gpc-distribution\scheduler_distribution_link.pdb /MACHINE:X86 1>&2
if errorlevel 1 goto fail
..\..\..\.work\gpc-distribution\scheduler_distribution.exe
set result=%errorlevel%
popd
exit /b %result%
:fail
popd
exit /b 1
