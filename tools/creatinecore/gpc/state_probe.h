// Экспериментальная квалификация сериализации; не подключать к обычной игре.
#pragma once
#include "scheduler.h"
#include "productionflag_adapter.h"
#include "seed_adapter.h"

static void ProbeSchedule(DWORD base, DWORD world, GpcProbeLog log) {
    if (!GetEnvironmentVariableA("VIC2_GPC_PROBE_STATE", nullptr, 0)) return;
    try {
        const unsigned char setSignature[] = {0x57, 0x8b, 0xf8};
        // Проверяется точный helper до первого вызова: неверный ABI не исполнять.
        if (memcmp((void*)(base + 0x3C450), setSignature, sizeof(setSignature))) {
            log("GPC state probe: set signature mismatch"); return;
        }
        auto flags = gpc::ReadScheduleFlags(base, world);
        auto previous = gpc::DecodeSchedule(flags);
        auto today = gpc::ScheduleDayFromRaw(*(DWORD*)(world + 0xB0C));
        gpc::PersistState next;
        bool attempted = false;
        if (!previous) {
            unsigned seed = gpc::ReadSeed(base);
            next = gpc::InitializeSchedule(seed, today);
            log("GPC state probe: INIT seed=%u day=%u", seed, today);
        } else {
            auto tick = gpc::TickSchedule(*previous, today, true, 555);
            next = tick.next; attempted = tick.attempted;
            log("GPC state probe: RESTORE/TICK old_day=%u day=%u attempted=%u sequence=%u rng=%u",
                previous->last_day, today, attempted ? 1u : 0u, next.pass_sequence, next.rng);
        }
        auto encoded = gpc::EncodeSchedule(next);
        auto changes = gpc::WriteScheduleFlags(base, world, flags, encoded);
        flags = gpc::ReadScheduleFlags(base, world);
        auto verified = gpc::DecodeSchedule(flags);
        if (!verified || !(*verified == next))
            throw std::runtime_error("Не совпала обратная проверка состояния");
        log("GPC state probe: COMMIT ok=1 day=%u rng=%u flags=%zu changes=%zu", today, next.rng, flags.size(), changes);
    } catch (const std::exception& error) {
        log("GPC state probe: ERROR %s", error.what());
    }
}
