#pragma once

// Чистое расписание: никаких обращений к игре, её RNG, файлам или часам ОС.
// Нативный адаптер отдельно подтверждает фазу, seed и порог MTTH.
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

namespace gpc {

using Day = std::uint32_t;
constexpr std::int64_t kScheduleDateEpoch = 0x029C55C0;
constexpr std::uint32_t kScheduleSchema = 1;
constexpr std::uint32_t kScheduleChanceScale = 1000000;
// months=60 даёт этот порог в исследованном EXE; runtime-квалификация отдельно.
constexpr std::uint32_t kScheduleMtth60Threshold = 555;
constexpr std::uint32_t kScheduleZeroSeedReplacement = 0x6D2B79F5u;
constexpr Day kScheduleMaximumDay = static_cast<Day>(
    ((std::numeric_limits<std::int32_t>::max)() - kScheduleDateEpoch) / 24);

inline void ValidateScheduleDay(Day day) {
    if (day > kScheduleMaximumDay)
        throw std::invalid_argument("Дата планировщика вне диапазона движка");
}

// Дневной callback должен предоставлять полную границу суток. Незавершённый
// часовой тик отвергается, чтобы адаптер не скрывал неверно выбранную фазу.
inline Day ScheduleDayFromRaw(std::int64_t raw_date) {
    if (raw_date < kScheduleDateEpoch ||
        raw_date > (std::numeric_limits<std::int32_t>::max)())
        throw std::invalid_argument("Сырая дата вне диапазона планировщика");
    const auto hours = raw_date - kScheduleDateEpoch;
    if (hours % 24 != 0)
        throw std::invalid_argument("Сырая дата не является границей суток");
    return static_cast<Day>(hours / 24);
}

inline bool IsCountryEventDay(Day day) {
    ValidateScheduleDay(day);
    return (day & 1u) != 0;
}

struct PersistState {
    std::uint32_t schema;
    std::uint32_t seed;
    std::uint32_t rng;
    Day last_day;
    std::uint32_t pass_sequence;

    bool operator==(const PersistState& other) const {
        return schema == other.schema && seed == other.seed && rng == other.rng &&
               last_day == other.last_day && pass_sequence == other.pass_sequence;
    }
};

inline void ValidateScheduleState(const PersistState& state) {
    if (state.schema != kScheduleSchema)
        throw std::invalid_argument("Неизвестная версия состояния планировщика");
    if (!state.seed || !state.rng)
        throw std::invalid_argument("Нулевой seed или RNG в сохранённом состоянии");
    ValidateScheduleDay(state.last_day);
}

inline PersistState InitializeSchedule(std::uint32_t game_seed, Day today) {
    ValidateScheduleDay(today);
    const auto seed = game_seed ? game_seed : kScheduleZeroSeedReplacement;
    // Инициализация запоминает сегодняшний день без попытки выдачи.
    return {kScheduleSchema, seed, seed, today, 0};
}

inline std::uint32_t XorShift32(std::uint32_t state) {
    if (!state) throw std::invalid_argument("Нулевое состояние xorshift32");
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

struct BoundedDraw {
    std::uint32_t value;
    std::uint64_t draws;
};

// Полный ненулевой цикл xorshift содержит 2^32-1 значений. После вычитания
// единицы имеем [0, 2^32-2]. Отбрасываем неполный хвост; обычный алгоритм
// для полного uint32-диапазона здесь дал бы смещение из-за отсутствия нуля.
// Next возвращает uint32; ноль не участвует в распределении. Производственный
// XorShift32 его не создаёт, но контракт явно определён и для тестового Next.
template<class Next>
inline BoundedDraw DrawNonzeroBounded(Next&& next, std::uint32_t bound) {
    if (!bound) throw std::invalid_argument("Нулевой диапазон случайного числа");
    constexpr std::uint64_t domain = (std::numeric_limits<std::uint32_t>::max)();
    const std::uint64_t limit = domain - domain % bound;
    std::uint64_t draws = 0;
    for (;;) {
        const std::uint32_t raw = next();
        ++draws;
        if (!raw) continue;
        const std::uint64_t sample = static_cast<std::uint64_t>(raw) - 1;
        if (sample < limit)
            return {static_cast<std::uint32_t>(sample % bound), draws};
    }
}

struct ScheduleTick {
    PersistState next;
    bool attempted;
    bool due;
    std::uint64_t draws;
};

// Возвращает предложенное новое состояние; вход не изменяется даже при ошибке.
// Нативный слой отвечает за применение/фиксацию результата и ошибку частичной
// выдачи. Пропущенные дни не воспроизводятся. На выключенной механике меняется
// лишь last_day: повторное включение в тот же день не создаёт лишней попытки.
inline ScheduleTick TickSchedule(const PersistState& current, Day today,
                                 bool enabled, std::uint32_t threshold) {
    ValidateScheduleState(current);
    ValidateScheduleDay(today);
    if (threshold > kScheduleChanceScale)
        throw std::invalid_argument("Порог вероятности больше миллиона");
    if (today < current.last_day)
        throw std::invalid_argument("Дата движется назад без загрузки состояния");

    ScheduleTick result{current, false, false, 0};
    if (today == current.last_day) return result;
    result.next.last_day = today;
    if (!enabled || !IsCountryEventDay(today)) return result;

    result.attempted = true;
    const auto draw = DrawNonzeroBounded([&]() {
        result.next.rng = XorShift32(result.next.rng);
        return result.next.rng;
    }, kScheduleChanceScale);
    result.draws = draw.draws;
    result.due = draw.value < threshold;
    if (result.due) {
        if (current.pass_sequence == (std::numeric_limits<std::uint32_t>::max)())
            throw std::overflow_error("Переполнение номера прохода планировщика");
        ++result.next.pass_sequence;
    }
    return result;
}

using ScheduleFlags = std::map<std::string, bool>;
inline constexpr const char* kScheduleReadyFlag = "cc_gpc_v1_ready";
inline constexpr const char* kScheduleWritingFlag = "cc_gpc_v1_writing";

inline std::uint32_t ScheduleChecksum(const PersistState& state) {
    // FNV-1a: ровно пять uint32 в little-endian, без padding структур.
    const std::array<std::uint32_t, 5> words{
        state.schema, state.seed, state.rng, state.last_day, state.pass_sequence};
    std::uint32_t checksum = 2166136261u;
    for (auto word : words) {
        for (unsigned byte = 0; byte < 4; ++byte) {
            checksum ^= (word >> (byte * 8)) & 0xFFu;
            checksum *= 16777619u;
        }
    }
    return checksum;
}

namespace schedule_detail {

inline constexpr std::array<const char*, 6> kFields{
    "schema", "seed", "rng", "last_day", "pass_sequence", "checksum"};

inline std::string BitFlag(std::size_t field, unsigned bit, bool one) {
    std::string name = "cc_gpc_v1_";
    name += kFields.at(field);
    name += "_b";
    name += static_cast<char>('0' + bit / 10);
    name += static_cast<char>('0' + bit % 10);
    name += one ? "_1" : "_0";
    return name;
}

inline bool Active(const ScheduleFlags& flags, const std::string& name) {
    const auto found = flags.find(name);
    return found != flags.end() && found->second;
}

} // namespace schedule_detail

// Словарь не зависит от значений, дат и числа проходов: 384 флага битов + 2
// маркера. При сохранении через движок неактивные значения могут отсутствовать.
inline ScheduleFlags EmptyScheduleFlagDictionary() {
    ScheduleFlags flags{{kScheduleReadyFlag, false}, {kScheduleWritingFlag, false}};
    for (std::size_t field = 0; field < schedule_detail::kFields.size(); ++field) {
        for (unsigned bit = 0; bit < 32; ++bit) {
            flags.emplace(schedule_detail::BitFlag(field, bit, false), false);
            flags.emplace(schedule_detail::BitFlag(field, bit, true), false);
        }
    }
    return flags;
}

// Полный образ завершённого состояния. writing выставляется адаптером ДО
// начала пакетной записи и снимается только ПОСЛЕ её успешного завершения.
inline ScheduleFlags EncodeSchedule(const PersistState& state) {
    ValidateScheduleState(state);
    auto flags = EmptyScheduleFlagDictionary();
    const std::array<std::uint32_t, 6> words{state.schema, state.seed, state.rng,
        state.last_day, state.pass_sequence, ScheduleChecksum(state)};
    for (std::size_t field = 0; field < words.size(); ++field) {
        for (unsigned bit = 0; bit < 32; ++bit)
            flags.at(schedule_detail::BitFlag(field, bit, ((words[field] >> bit) & 1u) != 0)) = true;
    }
    flags.at(kScheduleReadyFlag) = true;
    return flags;
}

// nullopt означает отсутствие активного состояния; присутствующие в памяти
// очищенные узлы (false) тоже не являются состоянием. Любой активный фрагмент
// собственного пространства имён требует полного корректного образа.
inline std::optional<PersistState> DecodeSchedule(const ScheduleFlags& flags) {
    const auto known = EmptyScheduleFlagDictionary();
    bool present = false;
    for (const auto& flag : flags) {
        if (!flag.second || flag.first.compare(0, 7, "cc_gpc_") != 0) continue;
        if (known.find(flag.first) == known.end())
            throw std::invalid_argument("Неизвестный активный флаг состояния GPC");
        present = true;
    }
    if (!present) return std::nullopt;
    if (schedule_detail::Active(flags, kScheduleWritingFlag))
        throw std::invalid_argument("Незавершённая запись состояния GPC");
    if (!schedule_detail::Active(flags, kScheduleReadyFlag))
        throw std::invalid_argument("Фрагмент состояния GPC без маркера готовности");

    std::array<std::uint32_t, 6> words{};
    for (std::size_t field = 0; field < words.size(); ++field) {
        for (unsigned bit = 0; bit < 32; ++bit) {
            const bool zero = schedule_detail::Active(flags, schedule_detail::BitFlag(field, bit, false));
            const bool one = schedule_detail::Active(flags, schedule_detail::BitFlag(field, bit, true));
            if (zero == one)
                throw std::invalid_argument("Отсутствующий или двойной бит состояния GPC");
            if (one) words[field] |= std::uint32_t{1} << bit;
        }
    }
    PersistState state{words[0], words[1], words[2], words[3], words[4]};
    ValidateScheduleState(state);
    if (words[5] != ScheduleChecksum(state))
        throw std::invalid_argument("Контрольная сумма состояния GPC не совпадает");
    return state;
}

} // namespace gpc
