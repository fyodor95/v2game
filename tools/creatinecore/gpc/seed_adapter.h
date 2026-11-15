#pragma once

// Единственный начальный seed берётся через штатную counted RNG-обёртку.
// Поддерживается оригинальный entry либо конкретный диагностический RngHookC
// CreatineCore. Произвольный E9 не является разрешением вызвать чужой код.
#include "notify_probe.h"
#include <cstdint>
#include <stdexcept>

namespace gpc {
namespace seed_adapter_detail {

inline DWORD Word(const unsigned char* bytes) {
    DWORD value = 0;
    memcpy(&value, bytes, sizeof(value));
    return value;
}

inline bool OriginalCompare(const unsigned char* bytes, DWORD base) {
    // cmp dword ptr [DAT_00F20C18],0. Единственное relocatable поле
    // проверяется относительно действительной базы, а не по VA 0x00400000.
    return bytes[0] == 0x83 && bytes[1] == 0x3d &&
        Word(bytes + 2) == base + 0xb20c18 && bytes[6] == 0;
}

// SEH изолирован от функций с C++ unwinding. Вызываем entry ровно один раз,
// включая штатную диагностическую обёртку, если она была квалифицирована.
inline bool CallOnce(DWORD entry, std::uint32_t* result) {
    __try {
        *result = reinterpret_cast<std::uint32_t (__cdecl*)()>(entry)();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace seed_adapter_detail

// Без розыгрыша RNG и без изменений мира. Подходит для проверки установщика.
inline bool ValidateSeedEntry(DWORD base) {
    using gpc_probe_native::Read;
    using gpc_probe_native::U32;
    using gpc_probe_notify::InCoreModule;
    using seed_adapter_detail::Word;

    if (!base || base > UINT32_MAX - 0xb20c18) return false;
    const DWORD entry = base + 0x5b7d30;
    unsigned char original[7] = {};
    if (!Read(entry, original, sizeof(original))) return false;
    if (seed_adapter_detail::OriginalCompare(original, base)) return true;
    if (original[0] != 0xe9) return false;

    // RngHookC: pushfd; pushad; mov eax,[esp+24h]; push 3; push eax;
    // call RngProbeNote; add esp,8; popad; popfd; jmp [g_realRngC].
    const DWORD thunkAddress = entry + 5 + Word(original + 1);
    unsigned char thunk[25] = {};
    if (!InCoreModule(thunkAddress) || !Read(thunkAddress, thunk, sizeof(thunk)) ||
        thunk[0] != 0x9c || thunk[1] != 0x60 || thunk[2] != 0x8b ||
        thunk[3] != 0x44 || thunk[4] != 0x24 || thunk[5] != 0x24 ||
        thunk[6] != 0x6a || thunk[7] != 0x03 || thunk[8] != 0x50 ||
        thunk[9] != 0xe8 || thunk[14] != 0x83 || thunk[15] != 0xc4 ||
        thunk[16] != 0x08 || thunk[17] != 0x61 || thunk[18] != 0x9d ||
        thunk[19] != 0xff || thunk[20] != 0x25) return false;

    const DWORD observer = thunkAddress + 14 + Word(thunk + 10);
    const DWORD originalSlot = Word(thunk + 21);
    DWORD trampolineAddress = 0;
    unsigned char trampoline[12] = {};
    if (!InCoreModule(observer) || !InCoreModule(originalSlot) ||
        !U32(originalSlot, &trampolineAddress) || !InCoreModule(trampolineAddress) ||
        !Read(trampolineAddress, trampoline, sizeof(trampoline)) ||
        !seed_adapter_detail::OriginalCompare(trampoline, base) || trampoline[7] != 0xe9)
        return false;
    return trampolineAddress + 12 + Word(trampoline + 8) == base + 0x5b7d37;
}

inline std::uint32_t ReadSeed(DWORD base) {
    if (!ValidateSeedEntry(base))
        throw std::runtime_error("GPC seed: не квалифицирован entry синхронного RNG");
    std::uint32_t result = 0;
    if (!seed_adapter_detail::CallOnce(base + 0x5b7d30, &result))
        throw std::runtime_error("GPC seed: ошибка native RNG; возможен частичный розыгрыш, повтор запрещён");
    return result;
}

} // namespace gpc
