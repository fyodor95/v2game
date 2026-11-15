#pragma once

// Адаптер глобальных флагов Victoria II 3.04 x86. Вызывать только в проверенной
// фазе симуляции, после проверки EXE. Указатели не сохраняются между вызовами.
// Layout trie подтверждён статически lookup/insert/saver; квалификация чтения
// ветки и неизвестной версии на настоящей игре обязательна перед включением.
#if !defined(_MSC_VER) || !defined(_M_IX86)
#error GPC_flag_adapter_requires_MSVC_x86
#endif

#include "scheduler.h"
#include <cstring>
#include <excpt.h>
#include <set>
#include <vector>

namespace gpc {
namespace native_flags_detail {

using Address = std::uint32_t;
static_assert(sizeof(void*) == 4, "Требуется ABI Windows x86");

inline Address Add(Address address, std::uint32_t offset) {
    const std::uint64_t result = static_cast<std::uint64_t>(address) + offset;
    if (!address || result > (std::numeric_limits<Address>::max)())
        throw std::runtime_error("Недопустимый адрес контейнера глобальных флагов");
    return static_cast<Address>(result);
}

// Отдельная POD-функция: MSVC не допускает SEH и C++ unwinding в одном теле.
inline bool TryCopy(void* target, Address source, std::size_t bytes) {
    if (!source || static_cast<std::uint64_t>(source) + bytes > (std::uint64_t{1} << 32)) return false;
    __try {
        std::memcpy(target, reinterpret_cast<const void*>(source), bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<class T>
inline T Read(Address address) {
    T value{};
    if (!TryCopy(&value, address, sizeof(value)))
        throw std::runtime_error("Не удалось прочитать память глобальных флагов");
    return value;
}

template<std::size_t N>
inline void CheckSignature(Address address, const unsigned char (&expected)[N]) {
    unsigned char actual[N]{};
    if (!TryCopy(actual, address, N) || std::memcmp(actual, expected, N))
        throw std::runtime_error("Не совпала сигнатура операции глобальных флагов");
}

inline Address Container(Address base, Address world) {
    if (!world || Read<Address>(Add(base, 0xE588E8)) != world)
        throw std::runtime_error("Устаревший указатель мира при обращении к флагам");
    const Address container = Add(world, 0x64);
    const Address vtable = Read<Address>(container);
    if (vtable != Add(base, 0x9F817C) ||
        Read<Address>(Add(vtable, 8)) != Add(base, 0x5E6B10) ||
        Read<unsigned char>(Add(container, 8)) != 0)
        throw std::runtime_error("Неизвестное устройство контейнера глобальных флагов");
    return container;
}

inline bool TryLookup(Address function, Address container, const char* name, Address* result) {
    __try {
        *result = reinterpret_cast<Address (__thiscall*)(void*, const char*)>(function)(
            reinterpret_cast<void*>(container), name);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline Address Lookup(Address base, Address container, const char* name) {
    Address result = 0;
    if (!TryLookup(Add(base, 0x5E6B10), container, name, &result))
        throw std::runtime_error("Нативный поиск глобального флага завершился ошибкой");
    return result;
}

inline std::string EntryName(Address entry) {
    const auto length = Read<std::uint32_t>(Add(entry, 0x10));
    const auto capacity = Read<std::uint32_t>(Add(entry, 0x14));
    if (capacity < 15 || length > capacity || length > 4096 ||
        (capacity < 16 && length > 15))
        throw std::runtime_error("Недопустимое имя глобального флага");
    const Address text = capacity < 16 ? entry : Read<Address>(entry);
    std::vector<char> data(static_cast<std::size_t>(length) + 1);
    if (!TryCopy(data.data(), text, data.size()) || data[length] != 0 ||
        std::memchr(data.data(), 0, length))
        throw std::runtime_error("Повреждённая строка глобального флага");
    std::string name(data.data(), length);
    // Lookup нормализует символы. Для ASCII namespace не зависим от locale ОС.
    for (auto& ch : name) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
    return name;
}

// Это ternary search tree, не RB-tree: нет parent/sentinel, все дети nullptr
// при отсутствии. +8 = символ меньше, +C = больше, +10 = следующий символ.
// Источники: 009E6C10 lookup, 00A5C820 insert, 0043C490 saver.
struct Node {
    Address entry;
    std::int8_t symbol;
    unsigned char padding[3];
    Address less;
    Address greater;
    Address equal;
};
static_assert(sizeof(Node) == 0x14, "Размер узла trie не совпадает");

inline Address PrefixNode(Address root) {
    constexpr const char prefix[] = "cc_gpc_";
    std::set<Address> visited;
    Address at = root;
    std::size_t character = 0;
    while (at) {
        if (visited.size() >= 65536 || !visited.insert(at).second)
            throw std::runtime_error("Цикл или чрезмерная глубина поиска глобальных флагов");
        const auto node = Read<Node>(at);
        const int comparison = static_cast<unsigned char>(prefix[character]) - node.symbol;
        if (comparison < 0) at = node.less;
        else if (comparison > 0) at = node.greater;
        else {
            if (++character == sizeof(prefix) - 1) return at;
            at = node.equal;
        }
    }
    return 0;
}

inline bool IsActive(Address base, Address container, const char* name) {
    const auto entry = Lookup(base, container, name);
    return entry && Read<unsigned char>(Add(entry, 0x1C)) != 0;
}

inline bool TrySet(Address function, Address container, const char* name) {
    __try {
        __asm {
            mov eax, name
            mov esi, container
            call function
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline bool TryClear(Address function, Address container, const char* name) {
    __try {
        __asm {
            mov ecx, container
            mov edx, name
            call function
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline std::size_t SetFlag(Address base, Address container, const char* name, bool active) {
    if (IsActive(base, container, name) == active) return 0;
    const bool returned = active ? TrySet(Add(base, 0x3C450), container, name) :
                                  TryClear(Add(base, 0x3C470), container, name);
    if (!returned || IsActive(base, container, name) != active)
        throw std::runtime_error("Не подтверждена запись глобального флага; повтор запрещён");
    return 1;
}

inline ScheduleFlags ActiveOwn(const ScheduleFlags& flags) {
    ScheduleFlags result;
    for (const auto& flag : flags)
        if (flag.second && flag.first.compare(0, 7, "cc_gpc_") == 0) result.emplace(flag);
    return result;
}

} // namespace native_flags_detail

inline void ValidateScheduleWriteContracts(std::uint32_t base, std::uint32_t world) {
    using namespace native_flags_detail;
    Container(base, world);
    const unsigned char setter[] = {0x57,0x8B,0xF8,0x8B,0x06,0x8B,0x50,0x08,0x57,0x8B,0xCE,0xFF,0xD2};
    const unsigned char clearer[] = {0x8B,0x01,0x8B,0x40,0x08,0x52,0xFF,0xD0,0x85,0xC0,0x74,0x04,0xC6,0x40,0x1C,0x00};
    CheckSignature(Add(base, 0x3C450), setter);
    CheckSignature(Add(base, 0x3C470), clearer);
}

// Возвращает ВСЕ активные cc_gpc_* из соответствующей ветки trie, включая
// неизвестные cc_gpc_v2_* и ошибки имён. Фиксированный список из 386 имён
// здесь недостаточен: он позволил бы принять будущую версию за новый сейв.
inline ScheduleFlags ReadScheduleFlags(std::uint32_t base, std::uint32_t world) {
    using namespace native_flags_detail;
    const auto container = Container(base, world);
    const auto root = PrefixNode(Read<Address>(Add(container, 4)));
    ScheduleFlags flags;
    if (!root) return flags;

    struct Visit { Address address; bool siblings; };
    std::vector<Visit> pending{{root, false}};
    std::set<Address> visited;
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!visit.address) continue;
        if (visited.size() >= 65536 || !visited.insert(visit.address).second)
            throw std::runtime_error("Цикл или чрезмерный размер ветки глобальных флагов GPC");
        const auto node = Read<Node>(visit.address);
        if (node.entry) {
            const auto active = Read<unsigned char>(Add(node.entry, 0x1C));
            if (active > 1) throw std::runtime_error("Повреждённый bool глобального флага");
            if (active) {
                const auto name = EntryName(node.entry);
                if (name.compare(0, 7, "cc_gpc_") != 0 || Lookup(base, container, name.c_str()) != node.entry)
                    throw std::runtime_error("Имя записи не соответствует ветке глобальных флагов");
                if (!flags.emplace(name, true).second)
                    throw std::runtime_error("Повтор имени глобального флага GPC");
            }
        }
        // На последнем символе префикса соседние less/greater принадлежат
        // другим префиксам. Дальше все три дочерних ветки принадлежат cc_gpc_.
        if (visit.siblings) pending.push_back({node.greater, true});
        pending.push_back({node.equal, true});
        if (visit.siblings) pending.push_back({node.less, true});
    }
    return flags;
}

// Принимает считанный образ и желаемый полный образ EncodeSchedule. writing
// снимается только после проверки данных; при ошибке нет очистки/отката/повтора.
// Отказ самого финального сброса означает неизвестный результат записи.
// Возвращает число нативных изменений, включая служебные маркеры.
inline std::size_t WriteScheduleFlags(std::uint32_t base, std::uint32_t world,
                                     const ScheduleFlags& existing,
                                     const ScheduleFlags& desired,
                                     bool transaction_in_progress = false) {
    using namespace native_flags_detail;
    ValidateScheduleWriteContracts(base, world);
    DecodeSchedule(existing); // Повреждённое состояние не ремонтируется записью.
    const auto next = DecodeSchedule(desired);
    if (!next) throw std::invalid_argument("Нельзя записать пустое состояние GPC");
    const auto image = EncodeSchedule(*next);
    const auto expected = ActiveOwn(image);
    auto actual = ReadScheduleFlags(base, world);
    if (transaction_in_progress) {
        if (!schedule_detail::Active(actual, kScheduleWritingFlag))
            throw std::runtime_error("Отсутствует маркер начатой выдачи корок");
        actual.erase(kScheduleWritingFlag);
    }
    if (actual != ActiveOwn(existing))
        throw std::runtime_error("Снимок глобальных флагов устарел перед записью");
    if (actual == expected && !transaction_in_progress) return 0;

    const auto container = Container(base, world);
    std::size_t changed = SetFlag(base, container, kScheduleWritingFlag, true);
    for (const auto& flag : image) {
        if (flag.first == kScheduleWritingFlag || flag.first == kScheduleReadyFlag) continue;
        if (schedule_detail::Active(actual, flag.first) != flag.second)
            changed += SetFlag(base, container, flag.first.c_str(), flag.second);
    }
    changed += SetFlag(base, container, kScheduleReadyFlag, true);

    auto verified = ReadScheduleFlags(base, world);
    if (!schedule_detail::Active(verified, kScheduleWritingFlag))
        throw std::runtime_error("Исчез маркер незавершённой записи GPC");
    verified.erase(kScheduleWritingFlag);
    const auto decoded = DecodeSchedule(verified);
    if (verified != expected || !decoded || !(*decoded == *next))
        throw std::runtime_error("Не совпала обратная проверка записи GPC");
    changed += SetFlag(base, container, kScheduleWritingFlag, false);
    if (ReadScheduleFlags(base, world) != expected)
        throw std::runtime_error("Не подтверждено завершённое состояние GPC");
    return changed;
}

} // namespace gpc
