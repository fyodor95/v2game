#pragma once

#if !defined(_MSC_VER) || !defined(_M_IX86)
#error mp_control.h requires MSVC x86
#endif

#include <cstdint>
#include <cstring>
#include <excpt.h>
#include <stdexcept>

// Статические контракты: docs/research/gpc-mp-control-contracts.md.
// Вызывающий мост проверяет owner thread, изоляцию стенда и отсутствие другого
// pending-запроса. Эти функции не обеспечивают acknowledgment или дедупликацию IPC.
namespace vic2_mp {

struct State {
    std::uint32_t world = 0;
    std::uint32_t idler = 0;
    std::uint32_t manager = 0;
    std::uint32_t queue = 0;
    std::uint32_t handler = 0;  // idler+0x1D18: объект CIngameLobby.
    unsigned mode = 0;  // Только 0 (SP) или 1 (MP); неизвестное значение запрещено.
    bool host = false;  // manager+0x34; server бывает также в SP.
    unsigned speed = 0;  // Индекс 0..4, соответствующий обычным кнопкам скорости.
    bool paused = false;
    bool network_ready = false;  // CIngameLobby+0xE1 == 1.
    bool executing = false;
};

struct RequestResult {
    State before;
    bool queued = false;
    bool expected_paused = false;
    unsigned expected_speed = 0;
};

namespace detail {

using Address = std::uint32_t;

inline Address Add(Address address, Address offset) {
    if (!address || offset > UINT32_MAX - address)
        throw std::runtime_error("E_MP_LAYOUT: недопустимый адрес игрового объекта");
    return address + offset;
}

// Только POD внутри SEH: внешний C++-код преобразует ошибку чтения в отказ.
inline bool TryCopy(Address address, void* destination, std::size_t size) noexcept {
    __try {
        std::memcpy(destination, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<class T>
inline T Read(Address address) {
    T value{};
    if (!address || !TryCopy(address, &value, sizeof(value)))
        throw std::runtime_error("E_MP_LAYOUT: игровая память недоступна");
    return value;
}

inline Address Pointer(Address address) {
    const Address result = Read<Address>(address);
    if (!result)
        throw std::runtime_error("E_MP_LAYOUT: отсутствует текущий игровой объект");
    return result;
}

inline bool Flag(Address address) {
    const auto value = Read<std::uint8_t>(address);
    if (value > 1)
        throw std::runtime_error("E_MP_LAYOUT: неизвестное значение игрового флага");
    return value != 0;
}

template<std::size_t N>
inline void Signature(Address base, Address rva, const unsigned char (&expected)[N]) {
    unsigned char actual[N]{};
    if (!TryCopy(Add(base, rva), actual, N) || std::memcmp(actual, expected, N))
        throw std::runtime_error("E_MP_LAYOUT: сигнатура движка не совпадает");
}

inline void Value(Address address, Address expected) {
    if (Read<Address>(address) != expected)
        throw std::runtime_error("E_MP_LAYOUT: неизвестная таблица или функция движка");
}

inline void Signatures(Address base) {
    // Ветка штатной консоли, которая запрещает команды при idler+0x5C != 0.
    static const unsigned char console_mode[] = {
        0x8B, 0x80, 0x24, 0x0B, 0x00, 0x00, 0x38, 0x58, 0x5C, 0x74, 0x5F
    };
    static const unsigned char manager_getter[] = {
        0x8B, 0x81, 0xE8, 0x17, 0x00, 0x00, 0x8B, 0x80, 0x2C, 0x01, 0x00, 0x00, 0xC3
    };
    static const unsigned char paused_getter[] = {0x8A, 0x81, 0x38, 0x15, 0x00, 0x00, 0xC3};
    static const unsigned char prologue[] = {0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0};
    static const unsigned char speed_gate[] = {
        0x8A, 0x46, 0x5C, 0x84, 0xC0, 0x74, 0x11, 0x8B, 0x06, 0x8B, 0x50, 0x44,
        0xFF, 0xD2, 0x80, 0x78, 0x34, 0x00, 0x0F, 0x84, 0x8D, 0x00, 0x00, 0x00
    };
    static const unsigned char speed_enqueue[] = {
        0x8B, 0x06, 0x8B, 0x50, 0x44, 0x8B, 0xCE, 0xFF, 0xD2, 0x8B,
        0x48, 0x38, 0x8B, 0x01, 0x8B, 0x50, 0x18, 0x57, 0xFF, 0xD2
    };
    static const unsigned char pause_gate[] = {
        0x8B, 0xF1, 0x8B, 0x86, 0x18, 0x1D, 0x00, 0x00, 0x33, 0xDB, 0x57, 0x3B,
        0xC3, 0x74, 0x64, 0x38, 0x98, 0xE1, 0x00, 0x00, 0x00, 0x74, 0x5C, 0x38,
        0x5E, 0x5C, 0x74, 0x57
    };
    static const unsigned char pause_enqueue[] = {
        0x8B, 0x16, 0x8B, 0x42, 0x44, 0x8B, 0xCE, 0xFF, 0xD0, 0x8B,
        0x48, 0x38, 0x8B, 0x11, 0x8B, 0x42, 0x18, 0x53, 0xFF, 0xD0
    };
    static const unsigned char topbar_pause[] = {
        0x8B, 0x49, 0x24, 0x8B, 0x01, 0x8B, 0x90, 0x5C, 0x01, 0x00, 0x00, 0xFF, 0xE2
    };
    static const unsigned char topbar_increase[] = {
        0x8B, 0x49, 0x24, 0x8B, 0x01, 0x8B, 0x90, 0x64, 0x01, 0x00, 0x00, 0xFF, 0xE2
    };
    static const unsigned char topbar_decrease[] = {
        0x8B, 0x49, 0x24, 0x8B, 0x01, 0x8B, 0x90, 0x68, 0x01, 0x00, 0x00, 0xFF, 0xE2
    };
    Signature(base, 0x213CB1, console_mode);
    Signature(base, 0x251D10, manager_getter);
    Signature(base, 0x247560, paused_getter);
    Signature(base, 0x26A0D0, prologue);
    Signature(base, 0x26A1B0, prologue);
    Signature(base, 0x26A2C0, prologue);
    Signature(base, 0x26A0EE, speed_gate);
    Signature(base, 0x26A1CE, speed_gate);
    Signature(base, 0x26A132, speed_enqueue);
    Signature(base, 0x26A212, speed_enqueue);
    Signature(base, 0x26A2E0, pause_gate);
    Signature(base, 0x26A32E, pause_enqueue);
    Signature(base, 0x318D00, topbar_pause);
    Signature(base, 0x318CE0, topbar_increase);
    Signature(base, 0x318CF0, topbar_decrease);
}

inline void RequireHost(const State& state) {
    if (state.mode != 1)
        throw std::runtime_error("E_MP_MODE: требуется сетевая кампания");
    if (!state.host)
        throw std::runtime_error("E_MP_HOST_REQUIRED: управление разрешено только хосту");
    if (!state.network_ready)
        throw std::runtime_error("E_MP_NOT_READY: сетевая кампания ещё не активна");
    // executing отмечает исполнение прежней команды, а не занятость producer.
    // Эти upper-вызовы только ставят новую команду в очередь; контракт — §11 отчёта.
}

inline bool TryInvoke(Address function, Address idler) noexcept {
    __try {
        reinterpret_cast<void (__thiscall*)(void*)>(function)(reinterpret_cast<void*>(idler));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline void Invoke(Address base, Address rva, Address idler) {
    if (!TryInvoke(Add(base, rva), idler))
        throw std::runtime_error(
            "E_MP_NATIVE_CALL: исход отправки неизвестен; повторная отправка запрещена");
}

}  // namespace detail

inline State Inspect(std::uint32_t base) {
    using namespace detail;
    Signatures(base);
    State state;
    state.world = Pointer(Add(base, 0xE588E8));
    state.idler = Pointer(Add(state.world, 0xB24));
    const Address idler_vtable = Add(base, 0xA0A34C);
    Value(state.idler, idler_vtable);
    Value(Add(idler_vtable, 0x44), Add(base, 0x251D10));
    Value(Add(idler_vtable, 0x158), Add(base, 0x247560));
    Value(Add(idler_vtable, 0x15C), Add(base, 0x26A2C0));
    Value(Add(idler_vtable, 0x164), Add(base, 0x26A0D0));
    Value(Add(idler_vtable, 0x168), Add(base, 0x26A1B0));
    state.mode = Flag(Add(state.idler, 0x5C)) ? 1u : 0u;
    state.manager = Pointer(Add(Pointer(Add(state.idler, 0x17E8)), 0x12C));
    state.host = Flag(Add(state.manager, 0x34));
    state.executing = Flag(Add(state.manager, 0x234));
    state.queue = Pointer(Add(state.manager, 0x38));
    const Address queue_vtable = Pointer(state.queue);
    if (state.mode == 1) {
        Value(state.queue, Add(base, state.host ? 0xA42728u : 0xA427C8u));
        Value(Add(queue_vtable, 0x18), Add(base, state.host ? 0x662EA0u : 0x666B30u));
    }
    state.speed = Read<std::uint32_t>(Add(state.world, 0xB28));
    if (state.speed > 4)
        throw std::runtime_error("E_MP_LAYOUT: неизвестная скорость игры");
    state.paused = Flag(Add(state.idler, 0x1538));
    state.handler = Read<Address>(Add(state.idler, 0x1D18));
    if (state.mode == 1 && state.handler) {
        Value(state.handler, Add(base, 0xA0B310));
        state.network_ready = Flag(Add(state.handler, 0xE1));
    }
    return state;
}

inline RequestResult RequestPause(std::uint32_t base, bool paused) {
    RequestResult result;
    result.before = Inspect(base);
    detail::RequireHost(result.before);
    result.expected_paused = paused;
    result.expected_speed = result.before.speed;
    if (result.before.paused == paused)
        return result;
    // mode=1 и handler+E1=1 уже проверены: локальная ветка исключена.
    detail::Invoke(base, 0x26A2C0, result.before.idler);
    result.queued = true;
    return result;
}

inline RequestResult RequestSpeedStep(std::uint32_t base, unsigned target) {
    if (target > 4)
        throw std::runtime_error("E_MP_ARGUMENT: скорость должна быть в диапазоне 0..4");
    RequestResult result;
    result.before = Inspect(base);
    detail::RequireHost(result.before);
    result.expected_paused = result.before.paused;
    result.expected_speed = result.before.speed;
    if (result.before.speed == target)
        return result;
    const bool increase = target > result.before.speed;
    result.expected_speed = increase ? result.before.speed + 1 : result.before.speed - 1;
    detail::Invoke(base, increase ? 0x26A0D0u : 0x26A1B0u, result.before.idler);
    result.queued = true;
    return result;
}

}  // namespace vic2_mp
