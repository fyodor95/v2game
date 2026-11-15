// Экспериментальная квалификация сериализации; не подключать к обычной игре.
#pragma once
#include "scheduler.h"

static bool ProbeFlag(DWORD world, const char* name) {
    DWORD container = world + 0x64;
    DWORD function = *(DWORD*)(*(DWORD*)container + 8);
    DWORD entry = ((DWORD (__thiscall*)(void*, const char*))function)((void*)container, name);
    return entry && *(unsigned char*)(entry + 0x1C) != 0;
}
static void ProbeWriteFlag(DWORD base, DWORD world, const char* name, bool active) {
    if (ProbeFlag(world, name) == active) return;
    DWORD container = world + 0x64;
    DWORD setter = base + 0x3C450, clearer = base + 0x3C470;
    if (active) {
        __asm {
            mov eax, name
            mov esi, container
            call setter
        }
    } else {
        __asm {
            mov ecx, container
            mov edx, name
            call clearer
        }
    }
    if (ProbeFlag(world, name) != active) throw std::runtime_error("Не совпал результат записи флага");
}
static void ProbeSchedule(DWORD base, DWORD world, GpcProbeLog log) {
    if (!GetEnvironmentVariableA("VIC2_GPC_PROBE_STATE", nullptr, 0)) return;
    try {
        const unsigned char setSignature[] = {0x57, 0x8b, 0xf8};
        // Проверяется точный helper до первого вызова: неверный ABI не исполнять.
        if (memcmp((void*)(base + 0x3C450), setSignature, sizeof(setSignature))) {
            log("GPC state probe: set signature mismatch"); return;
        }
        auto flags = gpc::EmptyScheduleFlagDictionary();
        for (auto& flag : flags) flag.second = ProbeFlag(world, flag.first.c_str());
        auto previous = gpc::DecodeSchedule(flags);
        auto today = gpc::ScheduleDayFromRaw(*(DWORD*)(world + 0xB0C));
        gpc::PersistState next;
        bool attempted = false;
        if (!previous) {
            DWORD seedFunction = base + 0x5B7D30;
            if (*(WORD*)seedFunction != 0x3D83 || *(DWORD*)(seedFunction + 2) != base + 0xB20C18)
                throw std::runtime_error("Не совпала сигнатура seed RNG");
            unsigned seed = ((unsigned (__cdecl*)())seedFunction)();
            next = gpc::InitializeSchedule(seed, today);
            log("GPC state probe: INIT seed=%u day=%u", seed, today);
        } else {
            auto tick = gpc::TickSchedule(*previous, today, true, 555);
            next = tick.next; attempted = tick.attempted;
            log("GPC state probe: RESTORE/TICK old_day=%u day=%u attempted=%u sequence=%u rng=%u",
                previous->last_day, today, attempted ? 1u : 0u, next.pass_sequence, next.rng);
        }
        auto encoded = gpc::EncodeSchedule(next);
        ProbeWriteFlag(base, world, gpc::kScheduleWritingFlag, true);
        for (const auto& flag : encoded) {
            if (flag.first == gpc::kScheduleWritingFlag || flag.first == gpc::kScheduleReadyFlag) continue;
            ProbeWriteFlag(base, world, flag.first.c_str(), flag.second);
        }
        ProbeWriteFlag(base, world, gpc::kScheduleReadyFlag, true);
        ProbeWriteFlag(base, world, gpc::kScheduleWritingFlag, false);
        for (auto& flag : flags) flag.second = ProbeFlag(world, flag.first.c_str());
        auto verified = gpc::DecodeSchedule(flags);
        if (!verified || !(*verified == next))
            throw std::runtime_error("Не совпала обратная проверка состояния");
        log("GPC state probe: COMMIT ok=1 day=%u rng=%u flags=193", today, next.rng);
    } catch (const std::exception& error) {
        log("GPC state probe: ERROR %s", error.what());
    }
}
