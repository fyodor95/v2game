#pragma once

// Только отдельная диагностическая сборка. Заголовок не подключается без macro;
// часы и форматирование не попадают в производственную ветку препроцессора.
#ifndef VIC2_GPC_PROFILE_PHASES
#error phase_profile.h requires VIC2_GPC_PROFILE_PHASES
#endif

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace gpc_phase_profile {

enum class Phase : unsigned {
    SnapshotSetup, SnapshotProvinces, CandidateIndex, CandidateScan, Preflight,
    GrantBatch, NotificationDispatch, Digest, PassFinalize, SchedulePublish, Count
};

inline constexpr unsigned kPhases = static_cast<unsigned>(Phase::Count);
inline constexpr const char* kNames[kPhases] = {
    "snapshot_setup", "snapshot_provinces", "candidate_index", "candidate_scan", "preflight",
    "grant_batch", "notification_dispatch", "digest", "pass_finalize", "schedule_publish"
};

class Session;
inline thread_local Session* active = nullptr;
using Log = void (__cdecl*)(const char*, ...);

class Session {
public:
    explicit Session(bool enabled) noexcept : enabled_(enabled) {
        if (!enabled_) return;
        previous_ = active;
        active = this;
        if (previous_) {
            previous_->Invalidate("nested_profile");
            Invalidate("nested_profile");
            return;
        }
        LARGE_INTEGER frequency = {}, now = {};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
            !QueryPerformanceCounter(&now) || now.QuadPart < 0) {
            Invalidate("clock_unavailable");
            return;
        }
        frequency_ = static_cast<std::uint64_t>(frequency.QuadPart);
        start_ = last_ = static_cast<std::uint64_t>(now.QuadPart);
    }

    ~Session() noexcept {
        if (enabled_ && active == this) active = previous_;
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void Next(Phase next) noexcept {
        if (!enabled_ || !valid_ || finished_) return;
        const unsigned value = static_cast<unsigned>(next);
        if (value != phase_ + 1 || value >= kPhases) {
            Invalidate("phase_order");
            return;
        }
        std::uint64_t now = 0;
        if (!ReadClock(&now)) return;
        ticks_[phase_] = now - last_;
        last_ = now;
        phase_ = value;
    }

    void World(std::uint64_t countries, std::uint64_t provinces, std::uint64_t pops,
               std::uint64_t cultures, std::uint64_t population) noexcept {
        countries_ = countries; provinces_ = provinces; pops_ = pops;
        cultures_ = cultures; population_ = population;
    }
    void Grants(std::uint64_t count) noexcept { grants_ = count; }
    void Recipients(std::uint64_t count) noexcept { recipients_ = count; }

    // Последняя отметка стоит после WriteScheduleFlags/StatusCompleted, до
    // журналирования. Разности соседних отметок не перекрываются.
    void Finish() noexcept {
        if (!enabled_ || finished_) return;
        if (valid_) {
            if (phase_ + 1 != kPhases) Invalidate("incomplete_phases");
            else {
                std::uint64_t now = 0;
                if (ReadClock(&now)) {
                    ticks_[phase_] = now - last_;
                    total_ = now - start_;
                    std::uint64_t sum = 0;
                    for (const auto ticks : ticks_) sum += ticks;
                    if (sum != total_) Invalidate("phase_sum");
                }
            }
        }
        finished_ = true;
        if (active == this) active = previous_;
    }

    // Только диагностическая строка. Отказ часов/форматирования/логгера не
    // переводит уже завершённую игровую операцию в failed и не вызывает повтор.
    void Report(Log log, unsigned pass, unsigned day, std::uint64_t added) noexcept {
        if (!enabled_ || !finished_ || !log) return;
        if (added > grants_ || recipients_ > added) Invalidate("counter_order");
        try {
            char phases[2048] = {};
            std::size_t used = 0;
            if (valid_) {
                for (unsigned n = 0; n < kPhases; ++n) {
                    int count = std::snprintf(phases + used, sizeof(phases) - used,
                        " %s_ticks=%llu %s_us=%llu", kNames[n],
                        static_cast<unsigned long long>(ticks_[n]), kNames[n],
                        static_cast<unsigned long long>(Microseconds(ticks_[n])));
                    if (count < 0 || static_cast<std::size_t>(count) >= sizeof(phases) - used) {
                        Invalidate("format_overflow");
                        phases[0] = 0;
                        break;
                    }
                    used += static_cast<std::size_t>(count);
                }
            }
            log("GPC profile: valid=%u reason=%s pass=%u day=%u frequency=%llu total_ticks=%llu total_us=%llu countries=%llu provinces=%llu pops=%llu cultures=%llu population=%llu grants=%llu added=%llu recipients=%llu%s",
                valid_ ? 1u : 0u, reason_, pass, day,
                static_cast<unsigned long long>(frequency_),
                static_cast<unsigned long long>(total_),
                static_cast<unsigned long long>(Microseconds(total_)),
                static_cast<unsigned long long>(countries_),
                static_cast<unsigned long long>(provinces_),
                static_cast<unsigned long long>(pops_),
                static_cast<unsigned long long>(cultures_),
                static_cast<unsigned long long>(population_),
                static_cast<unsigned long long>(grants_),
                static_cast<unsigned long long>(added),
                static_cast<unsigned long long>(recipients_), phases);
        } catch (...) {
            // Ошибка наблюдения не меняет состояние производственной механики.
        }
    }

private:
    void Invalidate(const char* reason) noexcept {
        if (valid_) reason_ = reason;
        valid_ = false;
    }
    bool ReadClock(std::uint64_t* output) noexcept {
        LARGE_INTEGER now = {};
        if (!QueryPerformanceCounter(&now) || now.QuadPart < 0 ||
            static_cast<std::uint64_t>(now.QuadPart) < last_) {
            Invalidate("clock_unavailable");
            return false;
        }
        *output = static_cast<std::uint64_t>(now.QuadPart);
        return true;
    }
    std::uint64_t Microseconds(std::uint64_t ticks) const noexcept {
        if (!frequency_) return 0;
        // Деление до умножения предотвращает переполнение QPCticks*1000000.
        const auto seconds = ticks / frequency_;
        if (seconds > (std::numeric_limits<std::uint64_t>::max)() / 1000000u)
            return (std::numeric_limits<std::uint64_t>::max)();
        const auto fraction = static_cast<std::uint64_t>(
            static_cast<long double>(ticks % frequency_) * 1000000.0L / frequency_);
        const auto whole = seconds * 1000000u;
        return fraction > (std::numeric_limits<std::uint64_t>::max)() - whole ?
            (std::numeric_limits<std::uint64_t>::max)() : whole + fraction;
    }

    Session* previous_ = nullptr;
    bool enabled_ = false, valid_ = true, finished_ = false;
    const char* reason_ = "ok";
    unsigned phase_ = 0;
    std::uint64_t frequency_ = 0, start_ = 0, last_ = 0, total_ = 0;
    std::uint64_t ticks_[kPhases] = {};
    std::uint64_t countries_ = 0, provinces_ = 0, pops_ = 0, cultures_ = 0,
        population_ = 0, grants_ = 0, recipients_ = 0;
};

inline void Next(Phase phase) noexcept {
    if (active) active->Next(phase);
}
inline void Recipients(std::uint64_t count) noexcept {
    if (active) active->Recipients(count);
}

} // namespace gpc_phase_profile
