"""Исполняет ветви реального C++-парсера в отдельном Win32 процессе."""
from pathlib import Path
import subprocess

from build import UPSTREAM, WORK, ROOT, msbuild_path


def main() -> None:
    source = (UPSTREAM / "Source/V2/V2/V2TechButton.cpp").read_text(encoding="utf-8-sig")
    state = source[source.index("static bool g_goodsConsumptionCanonicalSeen"):source.index("static void ApplySetting(")]
    branch = source[source.index('    if (_stricmp(key, "BUILDING_GOODS_CONSUMPTION")'):source.index('    if (_stricmp(key, "ENABLE_ASSIMILATE_BLOCK")')]
    diagnostic = next(line for line in source.splitlines() if 'if (_stricmp(key, "PATCH_FACTORY_DUMP_SCAN")' in line)
    harness = '''#include <cstring>
#include <cassert>
struct { bool goodsConsumption; bool patchFactoryDumpScan = false; } g_settings;
int warnings = 0;
void Log(const char*, ...) { ++warnings; }
'''+state+'\nvoid Apply(const char* key, bool v) {\n'+diagnostic+'\n'+branch+'''}
void Reset() { g_goodsConsumptionCanonicalSeen = false; g_goodsConsumptionLegacySeen = false; warnings = 0; }
int main() {
    assert(!g_settings.patchFactoryDumpScan);
    Apply("PATCH_FACTORY_DUMP_SCAN", true); assert(g_settings.patchFactoryDumpScan);
    Apply("PATCH_FACTORY_DUMP_SCAN", false); assert(!g_settings.patchFactoryDumpScan);
    Reset(); Apply("BUILDING_GOODS_CONSUMPTION", false); assert(!g_settings.goodsConsumption);
    Reset(); Apply("ENABLE_GOODS_CONSUMPTION", true); assert(g_settings.goodsConsumption);
    Reset(); Apply("ENABLE_GOODS_CONSUMPTION", false); assert(!g_settings.goodsConsumption);
    Reset(); Apply("BUILDING_GOODS_CONSUMPTION", false); Apply("ENABLE_GOODS_CONSUMPTION", true); assert(!g_settings.goodsConsumption && warnings == 1);
    Reset(); Apply("ENABLE_GOODS_CONSUMPTION", true); Apply("BUILDING_GOODS_CONSUMPTION", false); assert(!g_settings.goodsConsumption && warnings == 1);
    Reset(); Apply("BUILDING_GOODS_CONSUMPTION", true); Apply("ENABLE_GOODS_CONSUMPTION", true); assert(g_settings.goodsConsumption && warnings == 0);
    Reset(); Apply("building_goods_consumption", true); assert(g_settings.goodsConsumption);
    // Новый слой конфигурации может использовать старое имя, даже после canonical в предыдущем слое.
    Reset(); Apply("ENABLE_GOODS_CONSUMPTION", false); assert(!g_settings.goodsConsumption);
}
'''
    (WORK / "config_alias_test.cpp").write_text(harness, encoding="utf-8-sig")
    template = Path(__file__).with_name("script_fixes.vcxproj").read_text(encoding="utf-8")
    template = template.replace("DynamicLibrary", "Application").replace("ccore_script_fixes", "config_alias_test").replace("script_fixes.cpp", "config_alias_test.cpp").replace("obj-script-fixes", "obj-config-test").replace("<SubSystem>Windows</SubSystem>", "<SubSystem>Console</SubSystem>")
    project = WORK / "config_alias_test.vcxproj"
    project.write_text(template, encoding="utf-8")
    subprocess.run([str(msbuild_path()), str(project), "/p:Configuration=Release", "/p:Platform=Win32", "/m:1", "/v:minimal", "/nologo"], cwd=ROOT, check=True)
    subprocess.run([str(WORK / "build/config_alias_test.exe")], check=True)
    print("Проверки обоих имён, приоритета, конфликта и сброса слоя пройдены")


if __name__ == "__main__":
    main()
