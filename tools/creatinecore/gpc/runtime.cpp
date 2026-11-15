// Модуль включается только после квалификации SP/MP и проверки манифеста.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include "world_reader.h"
#include "productionflag_adapter.h"
#include "native_operations.h"
#include "seed_adapter.h"
#include "runtime_status.h"

// MSVC x86 добавляет '_' к C __cdecl symbol. У CLI есть стабильное имя export,
// независимо от декорирования V2Mod_Init существующей сборочной цепочкой.
#pragma comment(linker, "/EXPORT:V2Gpc_GetRuntimeStatus=_V2Gpc_GetRuntimeStatus")

struct V2ModAPI {
    uint32_t size, apiVersion, gameBase;
    const char* modFolder;
    void (__cdecl* Log)(const char*, ...);
    int (__cdecl* PatchSlot)(uint32_t, int, void*, void**);
    int (__cdecl* StealToTrampoline)(uint32_t, unsigned, unsigned char*, int,
                                    const unsigned char*, void*, void**, const char*);
};
static DWORD imageBase, originalScan;
static void (__cdecl* logger)(const char*, ...);
static unsigned threshold;
static bool failed;
static DWORD simulationThread;
static SRWLOCK statusLock = SRWLOCK_INIT;
static V2GpcRuntimeStatus runtimeStatus = {
    sizeof(V2GpcRuntimeStatus), V2_GPC_RUNTIME_STATUS_VERSION, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0
};
static unsigned callbackDepth;

// Защищаются только небольшие POD-копии. Под lock нет вызовов движка,
// журналирования, ожидания сетевого подтверждения и callback пользователя.
class CallbackStatusGuard {
public:
    CallbackStatusGuard() {
        AcquireSRWLockExclusive(&statusLock);
        ++callbackDepth;
        runtimeStatus.busy = 1;
        ReleaseSRWLockExclusive(&statusLock);
    }
    ~CallbackStatusGuard() {
        AcquireSRWLockExclusive(&statusLock);
        if (callbackDepth) --callbackDepth;
        runtimeStatus.busy = callbackDepth != 0 ? 1u : 0u;
        ReleaseSRWLockExclusive(&statusLock);
    }
    CallbackStatusGuard(const CallbackStatusGuard&) = delete;
    CallbackStatusGuard& operator=(const CallbackStatusGuard&) = delete;
};

static void StatusCallbackDate(std::uint32_t rawDate) {
    AcquireSRWLockExclusive(&statusLock);
    runtimeStatus.last_callback_raw_date = rawDate;
    ReleaseSRWLockExclusive(&statusLock);
}

static void StatusFailure() {
    AcquireSRWLockExclusive(&statusLock);
    runtimeStatus.failed = 1;
    ReleaseSRWLockExclusive(&statusLock);
}

static int InitializationFailure() {
    StatusFailure();
    return 0;
}

static void StatusCompleted(std::uint32_t rawDate, const gpc::PersistState& state,
                            bool fullPass, std::uint32_t coreDigest) {
    const auto checksum = gpc::ScheduleChecksum(state);
    AcquireSRWLockExclusive(&statusLock);
    runtimeStatus.has_schedule = 1;
    runtimeStatus.last_completed_raw_date = rawDate;
    runtimeStatus.pass = state.pass_sequence;
    runtimeStatus.rng = state.rng;
    runtimeStatus.state_digest = checksum;
    if (fullPass) runtimeStatus.core_digest = coreDigest;
    ReleaseSRWLockExclusive(&statusLock);
}

extern "C" int __cdecl V2Gpc_GetRuntimeStatus(
    V2GpcRuntimeStatus* output, std::uint32_t outputSize) {
    if (!output || outputSize < sizeof(V2GpcRuntimeStatus)) return 0;
    V2GpcRuntimeStatus snapshot;
    AcquireSRWLockShared(&statusLock);
    snapshot = runtimeStatus;
    ReleaseSRWLockShared(&statusLock);
    // Только POD внутри SEH. Ошибка указателя вызывающего кода не должна
    // превращаться в мутацию механики или исключение через C ABI.
    __try { std::memcpy(output, &snapshot, sizeof(snapshot)); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static std::uint32_t CoreDigest(gpc_world_reader::WorldSnapshot snapshot, const std::vector<gpc::CoreGrant>& grants) {
    for (const auto& grant : grants) {
        auto at = std::lower_bound(snapshot.provinces.begin(), snapshot.provinces.end(), grant.province,
            [](const auto& province, auto id) { return province.id < id; });
        if (at == snapshot.provinces.end() || at->id != grant.province) throw std::runtime_error("Нет провинции для digest");
        at->cores.push_back(grant.country);
    }
    std::uint32_t digest = 2166136261u;
    auto add = [&](std::uint32_t word) {
        for (unsigned n = 0; n < 4; ++n) { digest ^= (word >> (n * 8)) & 255; digest *= 16777619u; }
    };
    for (auto& province : snapshot.provinces) {
        std::sort(province.cores.begin(), province.cores.end());
        province.cores.erase(std::unique(province.cores.begin(), province.cores.end()), province.cores.end());
        add(province.id); add(static_cast<std::uint32_t>(province.cores.size()));
        for (auto country : province.cores) add(country);
    }
    return digest;
}

static unsigned Setting(const char* path, const char* key, unsigned fallback,
                        unsigned minimum, unsigned maximum) {
    char defaultValue[32], value[32];
    sprintf_s(defaultValue, "%u", fallback);
    if (GetPrivateProfileStringA("GreatPowerCultureCores", key, defaultValue, value, sizeof(value), path) >= sizeof(value) - 1)
        throw std::invalid_argument("Слишком длинное значение настройки GPC");
    char* end = nullptr;
    auto number = strtoul(value, &end, 10);
    if (!value[0] || *end || number < minimum || number > maximum)
        throw std::invalid_argument("Недопустимое значение настройки GPC");
    return static_cast<unsigned>(number);
}

static void __stdcall OnDay() {
    CallbackStatusGuard callbackStatus;
    if (failed) return;
    try {
        if (!simulationThread) simulationThread = GetCurrentThreadId();
        if (simulationThread != GetCurrentThreadId())
            throw std::runtime_error("Изменился поток симуляции GPC");
        const DWORD world = gpc_snapshot_probe::Read32(imageBase + 0xE588E8);
        const auto rawDate = gpc_snapshot_probe::Read32(world + 0xB0C);
        StatusCallbackDate(rawDate);
        const auto today = gpc::ScheduleDayFromRaw(rawDate);
        const auto flags = gpc::ReadScheduleFlags(imageBase, world);
        const auto current = gpc::DecodeSchedule(flags);
        if (!current) {
            const auto seed = gpc::ReadSeed(imageBase);
            const auto initial = gpc::InitializeSchedule(seed, today);
            gpc::WriteScheduleFlags(imageBase, world, flags, gpc::EncodeSchedule(initial));
            StatusCompleted(rawDate, initial, false, 0);
            logger("GPC runtime: initialized day=%u seed=%u", today, initial.seed);
            return;
        }
        const auto tick = gpc::TickSchedule(*current, today, true, threshold);
        if (tick.next == *current) {
            StatusCompleted(rawDate, *current, false, 0);
            return;
        }
        std::size_t added = 0;
        ULONGLONG elapsed = 0;
        std::uint32_t digest = 0;
        if (tick.due) {
            auto started = GetTickCount64();
            const auto snapshot = gpc_world_reader::ReadWorld(imageBase);
            const auto grants = gpc::Calculate(snapshot.countries, snapshot.provinces, snapshot.technology_present);
            gpc_native_ops::Preflight(imageBase, snapshot, grants, 3997200);
            gpc::ValidateScheduleWriteContracts(imageBase, world);
            // Маркер остаётся активным при любой ошибке частичной выдачи.
            gpc::native_flags_detail::SetFlag(imageBase, world + 0x64, gpc::kScheduleWritingFlag, true);
            std::set<gpc::Id> recipients;
            for (const auto& grant : grants) {
                if (gpc_native_ops::ApplyCore(imageBase, snapshot, grant, logger)) {
                    ++added;
                    recipients.insert(grant.country);
                }
            }
            for (auto country : recipients)
                gpc_native_ops::Notify(imageBase, snapshot, country, 3997200, logger);
            digest = CoreDigest(snapshot, grants);
            elapsed = GetTickCount64() - started;
        }
        gpc::WriteScheduleFlags(imageBase, world, flags, gpc::EncodeSchedule(tick.next), tick.due);
        StatusCompleted(rawDate, tick.next, tick.due, digest);
        if (tick.due) logger("GPC runtime: pass=%u day=%u additions=%zu elapsed_ms=%llu rng=%u cores=%08X state=%08X",
            tick.next.pass_sequence, today, added, elapsed, tick.next.rng, digest, gpc::ScheduleChecksum(tick.next));
    } catch (const std::exception& error) {
        failed = true;
        StatusFailure();
        logger("GPC runtime: ERROR %s; обработка остановлена, автоматический повтор запрещён", error.what());
    }
}

static __declspec(naked) void Scan() {
    __asm {
        push dword ptr [esp + 4]
        call dword ptr [originalScan]
        pushfd
        pushad
        call OnDay
        popad
        popfd
        ret 4
    }
}

extern "C" __declspec(dllexport) int __cdecl V2Mod_Init(const V2ModAPI* api) {
    if (!api || api->size < sizeof(V2ModAPI) || api->apiVersion != 1) return InitializationFailure();
    imageBase = api->gameBase; logger = api->Log;
    try {
        char exe[MAX_PATH], config[MAX_PATH];
        auto length = GetModuleFileNameA(nullptr, exe, MAX_PATH);
        if (!length || length >= MAX_PATH || !strrchr(exe, '\\')) return InitializationFailure();
        *strrchr(exe, '\\') = 0;
        if (sprintf_s(config, "%s\\z_CreatineCore\\great_power_culture_cores.ini", exe) < 0) return InitializationFailure();
        const auto enabled = Setting(config, "ENABLE_GREAT_POWER_CULTURE_CORES", 0, 0, 1);
        AcquireSRWLockExclusive(&statusLock);
        runtimeStatus.enabled = enabled;
        ReleaseSRWLockExclusive(&statusLock);
        const auto months = Setting(config, "GREAT_POWER_CULTURE_CORES_MTTH_MONTHS", 60, 1, 1200);
        threshold = 1000000 / (months * 30);
        // Ускорение доступно только явно помеченному изолированному стенду.
        char test[32] = {};
        if (GetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr, 0) &&
            GetEnvironmentVariableA("VIC2_GPC_TEST_ALWAYS", test, sizeof(test)) && !strcmp(test, "1"))
            threshold = 1000000;
        logger("GPC runtime: version=1 primary_only=1 all_ties=1 enabled=%u months=%u threshold=%u", enabled, months, threshold);
        if (!enabled) return 1;
        auto site = (unsigned char*)(imageBase + 0x2861E8);
        originalScan = imageBase + 0x4A7CD0;
        if (site[0] != 0xE8 || (DWORD)(site + 5) + *(DWORD*)(site + 1) != originalScan)
            throw std::runtime_error("Конфликт перехвата сканирования событий");
        const unsigned char signature[] = {0x55,0x8B,0xEC,0x83,0xE4,0xF8};
        if (memcmp((void*)originalScan, signature, sizeof(signature))) return InitializationFailure();
        DWORD old, ignored;
        if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) return InitializationFailure();
        *(DWORD*)(site + 1) = (DWORD)&Scan - (DWORD)(site + 5);
        VirtualProtect(site, 5, old, &ignored);
        FlushInstructionCache(GetCurrentProcess(), site, 5);
        logger("GPC runtime: installed post-events hook");
        return 1;
    } catch (const std::exception& error) {
        StatusFailure();
        logger("GPC runtime: install ERROR %s", error.what()); return 0;
    }
}
