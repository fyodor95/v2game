@echo off
chcp 65001 >nul
setlocal
rem Отдельная диагностическая сборка; production output и установка не меняются.
call "C:\Program Files\Microsoft Visual Studio\2026\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
if not exist ..\..\..\.work\gpc-profile mkdir ..\..\..\.work\gpc-profile
if errorlevel 1 goto fail
cl /nologo /LD /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /wd4505 /DVIC2_GPC_PROFILE_PHASES /Fo..\..\..\.work\gpc-profile\runtime.obj /Fd..\..\..\.work\gpc-profile\runtime.pdb runtime.cpp /link /OUT:..\..\..\.work\gpc-profile\ccore_great_power_culture_cores.dll /IMPLIB:..\..\..\.work\gpc-profile\ccore_great_power_culture_cores.lib /PDB:..\..\..\.work\gpc-profile\ccore_great_power_culture_cores.pdb /MACHINE:X86
if errorlevel 1 goto fail
rem write_manifest.py использует production gpc-build; сюда его не вызываем.
python -c "from pathlib import Path; import hashlib,json,struct; source=Path.cwd(); root=source.parents[2]; build=root/'.work/gpc-profile'; binary=build/'ccore_great_power_culture_cores.dll'; content=binary.read_bytes(); pe=struct.unpack_from('<I',content,60)[0]; assert content[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',content,pe+4)[0]==0x14c, 'Expected x86 PE'; digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest(); expected='62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648'; assert digest(root/'v2game.exe')==expected, 'Unsupported EXE'; production=root/'z_CreatineCore/dll_Submods/ccore_great_power_culture_cores.dll'; sources=sorted(source.glob('*.h'))+[source/'runtime.cpp',source/'build_profile.cmd']; record={'schema':1,'diagnostic_only':True,'machine':'x86','defines':['VIC2_GPC_PROFILE_PHASES'],'expected_exe_sha256':expected,'artifact':{'name':binary.name,'bytes':len(content),'sha256':digest(binary)},'production_sha256_read_only':digest(production) if production.is_file() else None,'sources':{p.name:digest(p) for p in sorted(sources)}}; (build/'manifest.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8'); print(build/'manifest.json')"
set result=%errorlevel%
popd
exit /b %result%
:fail
popd
exit /b 1
