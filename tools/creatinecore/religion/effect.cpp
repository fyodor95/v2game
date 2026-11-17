#include "native.h"
#include "CCore_Shared.h"
#include <cstring>
#include <exception>
#include <stdexcept>

namespace {
constexpr int kAssimilate = 0xa5b;
constexpr unsigned kSlots = 14; // Последний — native formatter0087C450.
void* vtable[kSlots + 1]{};
bool enabled = false;
bool registered = false;

void ParseInactive(void* self, void* edx, void* ctx, tCoreEffectFactory original) {
    // CBoolEffect::Parse (0087C410) считает true только токен 0x29F.
    // Ноль даёт false, включая failure-пути до замены vtable. Поле текста
    // и позиция lexer не меняются; исходный токен восстанавливается всегда.
    auto token = reinterpret_cast<DWORD*>(static_cast<char*>(ctx) + 0x228);
    const DWORD saved = *token;
    *token = 0;
    __try { original(self, edx, ctx, kAssimilate); }
    __finally { *token = saved; }
}

void __fastcall Execute(void* self, void*, void* context, int) {
    using namespace state_religion_native;
    try {
        if (!self || !Read32(reinterpret_cast<DWORD>(self) + 0x20)) {
            Log("PrimaryCultureReligion: REJECT value must be yes");
            return;
        }
        const auto population = ReadPopulation(g_base, context);
        const auto choice = state_religion::Choose(population.index,
            population.primary, population.current, population.pops);
        if (!choice.found) {
            Log("PrimaryCultureReligion: NO_POPULATION country_index=%lu religion_unchanged=1",
                population.index);
            return;
        }
        if (choice.religion == population.current) {
            Log("PrimaryCultureReligion: UNCHANGED country_index=%lu religion_index=%u people=%llu",
                population.index, choice.religion, choice.people);
            return;
        }
        if (Read32(population.country + 0xe64) != population.primary)
            throw std::runtime_error("primary culture changed during execution");
        const DWORD winner = population.religions.at(choice.religion);
        if (!ApplyReligion(g_base, context, winner) || Read32(population.country + 0xe68) != winner)
            throw std::runtime_error("native CReligionEffect failed");
        Log("PrimaryCultureReligion: SUCCESS country_index=%lu old_index=%lu religion=%s people=%llu",
            population.index, population.current, String(winner + 0x10).c_str(), choice.people);
    } catch (const std::exception& error) {
        Log("PrimaryCultureReligion: REJECT %s", error.what());
        DWORD scope[8]{};
        DWORD emptyPop = 0;
        const bool readable = Read(reinterpret_cast<DWORD>(context), scope, sizeof(scope));
        Read(g_base + 0xe58a10, &emptyPop, sizeof(emptyPop));
        Log("PrimaryCultureReligion: scope_context=%p readable=%d words00_1C=%08lX,%08lX,%08lX,%08lX,%08lX,%08lX,%08lX,%08lX empty_pop=%08lX",
            context, readable ? 1 : 0, scope[0], scope[1], scope[2], scope[3],
            scope[4], scope[5], scope[6], scope[7], emptyPop);
    }
}

void* __fastcall Tooltip(void*, void*, void* output,
    uintptr_t, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t a6, uintptr_t a7, uintptr_t, uintptr_t) {
    // ABI00894260: [ebp+10..24] — GameString, переданный по значению.
    // Native callee освобождает heap-buffer на обоих путях перед ret28h.
    DWORD argument[6] = {a2, a3, a4, a5, a6, a7};
    __try {
        if (output && !CoreLocalizeKeyToGameString("CC_SET_RELIGION_FROM_PRIMARY_CULTURE", output)) {
            state_religion_native::InitString(static_cast<state_religion_native::GameString*>(output));
            state_religion_native::SetString(g_base,
                static_cast<state_religion_native::GameString*>(output),
                "CC_SET_RELIGION_FROM_PRIMARY_CULTURE");
        }
        return output;
    } __finally {
        CoreFreeGameString(argument);
    }
}
}

bool CapturePrimaryCultureReligionEffect(void* self, void* edx, void* ctx, int,
                                        tCoreEffectFactory original) {
    if (!enabled || !ctx ||
        _stricmp(static_cast<const char*>(ctx) + 0x24, "set_religion_from_primary_culture"))
        return false;
    if (!self || !original) {
        Log("PrimaryCultureReligion: REJECT factory context");
        return true;
    }
    // Carrier создаётся НЕАКТИВНЫМ. При любом отказе до замены vtable
    // настоящий assimilate не получает разрешения изменять POP.
    const bool yes = _stricmp(static_cast<const char*>(ctx) + 0x22c, "yes") == 0;
    try {
        using namespace state_religion_native;
        const DWORD before = Read32(reinterpret_cast<DWORD>(self) + 0x0c);
        ParseInactive(self, edx, ctx, original);
        const DWORD node = Read32(reinterpret_cast<DWORD>(self) + 0x0c);
        if (!node || node == before)
            throw std::runtime_error("parser did not append an effect");
        const DWORD effect = Read32(node);
        const DWORD nativeTable = Read32(effect);
        if (nativeTable != g_base + 0xa2bae8)
            throw std::runtime_error("unexpected parser effect class");
        *reinterpret_cast<void***>(effect) = &vtable[1];
        // Не принимать неявные bool aliases: контракт метода разрешает yes.
        *reinterpret_cast<DWORD*>(effect + 0x20) = yes ? 1 : 0;
        if (!yes) Log("PrimaryCultureReligion: REJECT parser value must be yes");
        if (!registered) {
            registered = true;
            Log("PrimaryCultureReligion: parser registered keyword=set_religion_from_primary_culture");
        }
    } catch (const std::exception& error) {
        Log("PrimaryCultureReligion: REJECT parser %s", error.what());
    }
    return true;
}

bool InstallPrimaryCultureReligionEffect() {
    if (!state_religion_native::ExecutableMatches() || !state_religion_native::Contracts(g_base)) {
        Log("PrimaryCultureReligion: NOT_INSTALLED executable/native contract mismatch");
        return false;
    }
    if (!state_religion_native::Read(g_base + 0xa2bae8 - 4, vtable, sizeof(vtable))) {
        Log("PrimaryCultureReligion: NOT_INSTALLED carrier vtable unreadable");
        return false;
    }
    vtable[1 + 9] = reinterpret_cast<void*>(&Tooltip);
    vtable[1 + 11] = reinterpret_cast<void*>(&Execute);
    enabled = InstallEffectFactoryHook(false);
    Log("PrimaryCultureReligion: installed=%d country_scope=1 explicit_effect_only=1", enabled ? 1 : 0);
    return enabled;
}
