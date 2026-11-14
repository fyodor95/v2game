"""Выделяет три исправления из закреплённого исходника без оптимизатора."""
from pathlib import Path


def between(text: str, start: str, end: str) -> str:
    return text[text.index(start):text.index(end, text.index(start))]


def generate(upstream: Path, output: Path) -> None:
    source = (upstream / "Source/v2dll_country_daily_optimizer/v2dll_mod.cpp").read_text(encoding="utf-8-sig")
    constants = between(source, "constexpr uint32_t kTriggerFactoryRva", "constexpr uint32_t kRelationArrayOffset")
    dates = between(source, "static bool ParseDateNumber", "void __stdcall MapRenderAffinityThunk")
    patches = between(source, "bool PatchBytes(", "// Replaces one dword of a vtable when")
    patches += between(source, "bool PatchVtableAbsolute(", "// ALLOW_JUSTIFY_PUPPETS")
    effects = between(source, "uintptr_t g_v2dllEventCountrySkipAsm", "__declspec(naked) void __cdecl v2dll_KeepPuppetRelationThunk")
    effects += between(source, "__declspec(naked) void __cdecl v2dll_EventCountryCandidateThunk", "static bool InstallDailyHotspotHooks")
    effects = effects.replace("return addProvince || addCountry || removeProvince || removeCountry || hasCountry || hasProvince;", "return addProvince && addCountry && removeProvince && removeCountry && hasCountry && hasProvince;")
    dates = dates.replace('"%s\\xA7" "Ydate = %s\\xA7" "W\\n",\n                    (negate & 0xFF) ? "NOT " : "", value', '"%s\\xA7" "Y%s\\xA7" "W\\n",\n                    Localized((negate & 0xFF) ? "CC_DATE_BEFORE" : "CC_DATE_AT_LEAST"), value')
    effects = effects.replace('ConstructEngineString(out, "INVALID MODIFIERS");', 'ConstructEngineString(out, Localized("CC_INVALID_MODIFIER"));')
    header = '''// Выделено из CreatineCore 7d9f9df; исходное авторство сохранено в upstream.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "V2ModAPI.h"
namespace {
using LogFn = void (__cdecl*)(const char*, ...);
using TriggerFactoryFn = void (__fastcall*)(void*, void*, void*, int);
LogFn g_log = nullptr;
uintptr_t g_worldGlobal = 0;
uintptr_t g_gameImageBase = 0;
bool g_fireOnTime = true;
TriggerFactoryFn g_nextTriggerFactory = nullptr;
void** g_dateTriggerVtable = nullptr;
void* g_dateTriggerVtableBuf[17] = {};
__declspec(align(16)) unsigned char g_dateFactoryTrampoline[32] = {};
volatile LONG g_dateTriggersCreated = 0;
volatile LONG g_dateTriggerParseFailures = 0;
// Локализатор ядра возвращает игровые строки в кодировке установленной локализации.
static const char* Localized(const char* key) {
    static thread_local char output[256];
    using Localize = int (__cdecl*)(const char*, char*, unsigned);
    auto fn = reinterpret_cast<Localize>(GetProcAddress(GetModuleHandleW(L"creatinecore.dll"), "CCore_LocalizeKey"));
    if (!fn) fn = reinterpret_cast<Localize>(GetProcAddress(GetModuleHandleW(L"creatinecore.dll"), "_CCore_LocalizeKey"));
    if (!fn || !fn(key, output, sizeof(output))) strncpy_s(output, key, _TRUNCATE);
    return output;
}
'''
    init = '''
extern "C" __declspec(dllexport) int __cdecl V2Mod_Init(const V2ModAPI* api) {
    if (!api || api->size < sizeof(V2ModAPI) || api->apiVersion != V2MOD_API_VERSION || !api->gameBase || !api->Log) return 0;
    g_log = api->Log;
    g_gameImageBase = api->gameBase;
    g_worldGlobal = api->gameBase + 0xE588E8;
    const bool date = InstallDateTrigger(api->gameBase);
    const bool calendar = InstallEventFireOnTime(api->gameBase);
    const bool modifiers = InstallMissingModifierGuard(api->gameBase);
    g_log("Скриптовые исправления: date=%u fire_on_time=%u guard_missing_modifiers=%u", date, calendar, modifiers);
    return date && calendar && modifiers;
}
'''
    output.write_text(header + constants + dates + patches + '}\nextern "C" {\n' + effects + init, encoding="utf-8-sig")


if __name__ == "__main__":
    import sys
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
