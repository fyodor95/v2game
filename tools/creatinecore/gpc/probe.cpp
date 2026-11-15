// Квалификация общей фазы: диагностика без изменения игрового мира.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include "native_probe.h"
#include "state_probe.h"
#include "snapshot_probe.h"
#include "notify_probe.h"
#include "world_reader.h"
struct V2ModAPI {
    uint32_t size, apiVersion, gameBase;
    const char* modFolder;
    void (__cdecl* Log)(const char*, ...);
    int (__cdecl* PatchSlot)(uint32_t, int, void*, void**);
    int (__cdecl* StealToTrampoline)(uint32_t, unsigned, unsigned char*, int,
                                    const unsigned char*, void*, void**, const char*);
};
static DWORD base, originalLoop;
static void (__cdecl* logger)(const char*, ...);
static unsigned calls;
static void PreviewWorld() {
    if (!GetEnvironmentVariableA("VIC2_GPC_PROBE_PREVIEW", nullptr, 0)) return;
    try {
        const auto started = GetTickCount64();
        const auto world = gpc_world_reader::ReadWorld(base, true);
        const auto additions = gpc::Calculate(world.countries, world.provinces, world.technology_present);
        logger("GPC preview: date=%lu countries=%zu provinces=%zu pops=%llu additions=%zu elapsed_ms=%llu",
            world.date_raw, world.countries.size(), world.provinces.size(), static_cast<unsigned long long>(world.stats.pops),
            additions.size(), GetTickCount64() - started);
        for (const auto& pair : additions)
            logger("GPC preview pair: province=%u tag=%s", pair.province, world.tags.at(pair.country).c_str());
    } catch (const std::exception& error) {
        logger("GPC preview: ERROR %s", error.what());
    }
}
static DWORD Read(DWORD at) { return *(DWORD*)at; }
static DWORD EventObject(unsigned id) {
    DWORD manager = Read(base + 0xE58A98);
    if (!manager) return 0;
    DWORD count = Read(manager + 4), table = Read(manager + 8);
    if (!count || count > 1000000 || !table) return 0;
    DWORD node = Read(table + (((0x27u << 16) + id) % count) * 4);
    for (unsigned n = 0; node && n < 100000; ++n, node = Read(node + 4)) {
        DWORD object = Read(node);
        if (object && Read(object + 8) == 0x27 && Read(object + 12) == id) return object;
    }
    return 0;
}
static void CheckMtth() {
    const unsigned ids[] = {3997201, 3997202, 3997203};
    const unsigned expectedBase[] = {1800000, 30000, 1000};
    const unsigned expectedThreshold[] = {555, 33333, 1000000};
    const unsigned char signature[] = {0x55, 0x8b, 0xec};
    if (memcmp((void*)(base + 0x4AE380), signature, sizeof(signature))) {
        logger("GPC qualification: MTTH signature mismatch"); return;
    }
    for (unsigned i = 0; i < 3; ++i) {
        DWORD event = EventObject(ids[i]);
        if (!event) { logger("GPC qualification: MTTH fixture missing id=%u", ids[i]); continue; }
        DWORD object = event + 0xC0;
        if (Read(object + 12) != Read(object + 16)) {
            logger("GPC qualification: unexpected MTTH modifiers id=%u", ids[i]); continue;
        }
        unsigned char scope[64] = {};
        unsigned threshold = ((unsigned (__stdcall*)(void*, void*, int))(base + 0x4AE380))((void*)object, scope, 1);
        logger("GPC qualification: MTTH id=%u base=%lu threshold=%u ok=%d", ids[i],
            Read(object + 8), threshold, Read(object + 8) == expectedBase[i] && threshold == expectedThreshold[i]);
    }
}
static void __stdcall Observe() {
    DWORD world = *(DWORD*)(base + 0xE588E8);
    if (world) logger("GPC qualification: phase=after-events call=%u date_raw=%lu thread=%lu world=%08lX",
        ++calls, *(DWORD*)(world + 0xB0C), GetCurrentThreadId(), world);
    if (world && calls == 1) {
        CheckMtth();
        GpcProbeObserve(base, logger, calls);
        GpcProbeSnapshot(base, logger);
        PreviewWorld();
    }
    if (world) ProbeSchedule(base, world, logger);
    if (world && calls <= 2) {
        char target[64] = {};
        if (GetEnvironmentVariableA("VIC2_GPC_PROBE_TARGET", target, sizeof(target))) {
            unsigned province = 0; char tag[4] = {}, extra = 0;
            if (sscanf_s(target, "%u:%3[A-Z0-9]%c", &province, tag, (unsigned)sizeof(tag), &extra, 1u) == 2)
                GpcProbeAddCore(base, province, tag, logger);
            else logger("GPC qualification: неверная тестовая цель");
        }
    }
    if (world && calls == 1) {
        char recipient[8] = {};
        if (GetEnvironmentVariableA("VIC2_GPC_PROBE_NOTIFY_TAG", recipient, sizeof(recipient)))
            GpcProbeNotify(base, recipient, 3997200, logger);
    }
}
static __declspec(naked) void EventLoop() {
    __asm {
        push dword ptr [esp + 4]
        call dword ptr [originalLoop]
        pushfd
        pushad
        call Observe
        popad
        popfd
        ret 4
    }
}
extern "C" __declspec(dllexport) int __cdecl V2Mod_Init(const V2ModAPI* api) {
    if (!api || api->size < sizeof(V2ModAPI) || api->apiVersion != 1) return 0;
    if (!GetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr, 0)) return 0;
    base = api->gameBase; logger = api->Log;
    unsigned char* site = (unsigned char*)(base + 0x2861E8);
    originalLoop = base + 0x4A7CD0;
    if (site[0] != 0xE8 || (DWORD)(site + 5) + *(DWORD*)(site + 1) != originalLoop) {
        logger("GPC qualification: отказ, event-loop call не совпал"); return 0;
    }
    const unsigned char signature[] = {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8};
    if (memcmp((void*)originalLoop, signature, sizeof(signature))) return 0;
    DWORD protect;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &protect)) return 0;
    *(DWORD*)(site + 1) = (DWORD)&EventLoop - (DWORD)(site + 5);
    DWORD ignored;
    VirtualProtect(site, 5, protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    logger("GPC qualification: after-events probe установлен; тестовые мутации требуют отдельных переменных среды");
    return 1;
}
