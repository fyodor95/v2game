#include "native.h"
#include "CCore_Shared.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

bool CapturePrimaryCultureReligionEffect(void*, void*, void*, int, tCoreEffectFactory);
bool InstallPrimaryCultureReligionEffect();
DWORD g_base = 0;
namespace {
enum class Fault { none, wrong_class, tail_unreadable, no_append, factory_exception };
Fault fault = Fault::none;
DWORD unreadable = 0;
bool rejectVtable = false;
DWORD effect[9]{};
DWORD node[3]{};
unsigned calls = 0;
unsigned freedArguments = 0;
bool localized = false;
bool localizationThrows = false;
void __fastcall Factory(void* self, void*, void* context, int keyword) {
    ++calls;
    assert(keyword == 0xa5b);
    const DWORD token = *reinterpret_cast<DWORD*>(static_cast<char*>(context) + 0x228);
    // Семантика 0087C441..0087C449 подтверждена проверкой настоящего EXE.
    effect[0] = g_base + 0xa2bae8 + (fault == Fault::wrong_class ? 4 : 0);
    effect[8] = token == 0x29f;
    assert(!effect[8]); // До установки собственного Execute carrier неактивен.
    node[0] = reinterpret_cast<DWORD>(effect);
    if (fault != Fault::no_append)
        static_cast<DWORD*>(self)[3] = reinterpret_cast<DWORD>(node);
    if (fault == Fault::tail_unreadable)
        unreadable = reinterpret_cast<DWORD>(self) + 0x0c;
    if (fault == Fault::factory_exception)
        throw std::runtime_error("injected factory exception after append");
}
}

void Log(const char*, ...) {}
bool InstallEffectFactoryHook(bool) { return true; }
bool CoreLocalizeKeyToGameString(const char*, void* output) {
    if (localizationThrows) throw std::runtime_error("injected localization exception");
    state_religion_native::InitString(static_cast<state_religion_native::GameString*>(output));
    return localized;
}
void CoreFreeGameString(void* raw) {
    auto string = static_cast<state_religion_native::GameString*>(raw);
    if (string->capacity >= 16) {
        assert(string->length == 31 && string->capacity == 31);
        assert(!strcmp(string->data.pointer, "1234567890123456789012345678901"));
        ++freedArguments;
        std::free(string->data.pointer);
    }
}

// Подменён только адаптер среды. Capture/Install собираются из effect.cpp.
namespace state_religion_native {
bool Read(DWORD at, void* out, unsigned size) {
    if (rejectVtable || !at || at == unreadable) return false;
    memcpy(out, reinterpret_cast<void*>(at), size);
    return true;
}
DWORD Read32(DWORD at) {
    DWORD result = 0;
    if (!Read(at, &result, 4)) throw std::runtime_error("injected unreadable field");
    return result;
}
bool Contracts(DWORD) { return true; }
bool ExecutableMatches() { return true; }
CountryPopulation ReadPopulation(DWORD, void*) { throw std::runtime_error("unexpected Execute in parser test"); }
bool ApplyReligion(DWORD, void*, DWORD) { return false; }
std::string String(DWORD) { return {}; }
void InitString(GameString* value) { memset(value, 0, sizeof(*value)); value->capacity = 15; }
void SetString(DWORD, GameString*, const char*) {}
}

int main() {
    g_base = reinterpret_cast<DWORD>(VirtualAlloc(nullptr, 0xb00000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(g_base);
    reinterpret_cast<DWORD*>(g_base + 0xa2bae8)[13] = 0x12345678;
    rejectVtable = true;
    assert(!InstallPrimaryCultureReligionEffect());
    rejectVtable = false;
    assert(InstallPrimaryCultureReligionEffect());
    for (Fault injected : {Fault::none, Fault::wrong_class, Fault::tail_unreadable,
                          Fault::no_append, Fault::factory_exception}) {
        fault = injected;
        unreadable = 0;
        DWORD parent[5]{};
        std::vector<char> context(0x330);
        strcpy_s(context.data() + 0x24, 256, "set_religion_from_primary_culture");
        strcpy_s(context.data() + 0x22c, 256, "yes");
        *reinterpret_cast<DWORD*>(context.data() + 0x228) = 0x29f;
        assert(CapturePrimaryCultureReligionEffect(parent, nullptr, context.data(), 0, &Factory));
        assert(*reinterpret_cast<DWORD*>(context.data() + 0x228) == 0x29f);
        assert(!strcmp(context.data() + 0x22c, "yes"));
        if (injected == Fault::none) {
            assert(effect[0] != g_base + 0xa2bae8 && effect[8] == 1);
            assert(reinterpret_cast<DWORD*>(effect[0])[13] == 0x12345678);
        } else {
            assert(effect[8] == 0); // Во всех отказах настоящий carrier безопасен.
        }
    }
    fault = Fault::none;
    unreadable = 0;
    DWORD parent[5]{};
    std::vector<char> context(0x330);
    strcpy_s(context.data() + 0x24, 256, "set_religion_from_primary_culture");
    strcpy_s(context.data() + 0x22c, 256, "no");
    assert(CapturePrimaryCultureReligionEffect(parent, nullptr, context.data(), 0, &Factory));
    assert(effect[8] == 0);
    strcpy_s(context.data() + 0x24, 256, "assimilate");
    const unsigned before = calls;
    assert(!CapturePrimaryCultureReligionEffect(parent, nullptr, context.data(), 0, &Factory));
    assert(calls == before);
    // Вызов настоящего replacement Tooltip через его thiscall slot9:
    // callee получает GameString по значению и обязан освободить длинный буфер.
    using Tooltip = void*(__thiscall*)(void*, void*, uintptr_t, uintptr_t,
        uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    auto tooltip = reinterpret_cast<Tooltip>(reinterpret_cast<DWORD*>(effect[0])[9]);
    state_religion_native::GameString output{};
    for (unsigned mode = 0; mode < 4; ++mode) {
        localized = mode == 0;
        localizationThrows = mode == 2;
        char* argument = static_cast<char*>(std::malloc(32));
        assert(argument);
        strcpy_s(argument, 32, "1234567890123456789012345678901");
        bool threw = false;
        void* destination = mode == 3 ? nullptr : &output;
        try {
            assert(tooltip(effect, destination, 0, reinterpret_cast<DWORD>(argument),
                0, 0, 0, 31, 31, 0, 0) == destination);
        } catch (const std::runtime_error&) { threw = true; }
        assert(threw == localizationThrows);
        assert(freedArguments == mode + 1);
    }
    localizationThrows = false;
    assert(tooltip(effect, &output, 0, 0x00636261, 0, 0, 0, 3, 15, 0, 0) == &output);
    assert(freedArguments == 4); // SSO не передаётся игровому delete.
    VirtualFree(reinterpret_cast<void*>(g_base), 0, MEM_RELEASE);
    std::cout << "state religion factory failure paths: PASS\n";
}
