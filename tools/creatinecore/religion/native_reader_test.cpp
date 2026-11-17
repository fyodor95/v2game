#include "native.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Object = std::vector<DWORD>;
DWORD Pointer(Object& object) { return reinterpret_cast<DWORD>(object.data()); }
void Vector(Object& object, unsigned offset, Object& items) {
    object.at(offset / 4) = Pointer(items);
    object.at(offset / 4 + 1) = Pointer(items) + static_cast<DWORD>(items.size() * 4);
}
template<class F> bool Rejects(F run) {
    try { run(); } catch (const std::runtime_error&) { return true; }
    return false;
}
}

int main() {
    using namespace state_religion_native;
    const DWORD base = reinterpret_cast<DWORD>(VirtualAlloc(nullptr, 0xf20000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(base);
    Object world(0xb64 / 4), manager(3), country(0xe6c / 4), countries(2);
    Object religiousManager(0x24 / 4), religionA(0x74 / 4), religionB(0x74 / 4), religions(3);
    Object popManager(0x14 / 4), type(1), types(1), owned(2), provinces(3);
    Object provinceA(0x198 / 4), provinceB(0x198 / 4), listA(4), listB(4);
    Object popA(0x280 / 4), popB(0x280 / 4), otherCulture(0x280 / 4), scope(16);
    constexpr DWORD tag = 0x555250, primary = 0x111100;
    *reinterpret_cast<DWORD*>(base + 0xe588e8) = Pointer(world);
    *reinterpret_cast<DWORD*>(base + 0xe587e4) = Pointer(manager);
    *reinterpret_cast<DWORD*>(base + 0xe5ea24) = Pointer(religiousManager);
    *reinterpret_cast<DWORD*>(base + 0xf1bb34) = Pointer(popManager);
    manager[0] = 2;
    Vector(manager, 4, countries);
    countries[1] = Pointer(country);
    country[0x1c / 4] = tag;
    country[0x20 / 4] = 1;
    country[0xe64 / 4] = primary;
    country[0xe68 / 4] = Pointer(religionA);
    world[0xb5c / 4] = tag;
    world[0xb60 / 4] = 1;
    religions[1] = Pointer(religionA);
    religions[2] = Pointer(religionB);
    religionA[2] = 1;
    religionB[2] = 2;
    Vector(religiousManager, 0x1c, religions);
    types[0] = Pointer(type);
    Vector(popManager, 0x0c, types);
    owned = {1, 2};
    Vector(country, 0x9d8, owned);
    provinces[1] = Pointer(provinceA);
    provinces[2] = Pointer(provinceB);
    Vector(world, 0xacc, provinces);
    for (auto* province : {&provinceA, &provinceB}) {
        (*province)[0x128 / 4] = tag;
        (*province)[0x12c / 4] = 1;
    }
    provinceA[0x58 / 4] = 1;
    provinceB[0x58 / 4] = 2;
    provinceA[0x194 / 4] = Pointer(listA);
    provinceB[0x194 / 4] = Pointer(listB);
    listA[0] = Pointer(popA);
    listB[0] = Pointer(popB);
    for (auto* pop : {&popA, &popB, &otherCulture}) {
        (*pop)[0x68 / 4] = Pointer(type);
        (*pop)[0x6c / 4] = primary;
    }
    popA[0x58 / 4] = 50;
    popB[0x58 / 4] = 100;
    otherCulture[0x58 / 4] = 100000;
    popA[0x64 / 4] = Pointer(provinceA);
    popB[0x64 / 4] = Pointer(provinceB);
    otherCulture[0x64 / 4] = Pointer(provinceA);
    popA[0x70 / 4] = Pointer(religionA);
    popB[0x70 / 4] = Pointer(religionB);
    otherCulture[0x70 / 4] = Pointer(religionA);
    otherCulture[0x6c / 4] = 0x222200;
    popA[0x27c / 4] = Pointer(otherCulture);
    scope[2] = tag;
    scope[3] = 1;
    Object emptyPop(0x278 / 4);
    *reinterpret_cast<DWORD*>(base + 0xe58a10) = Pointer(emptyPop);
    scope[5] = Pointer(emptyPop); // Так native ctor создаёт country scope.
    const auto snapshot = ReadPopulation(base, scope.data());
    assert(snapshot.country == Pointer(country) && snapshot.pops.size() == 2);
    assert(snapshot.current == 1 && snapshot.primary == primary);
    assert(PlayerCountry(base) == Pointer(country));
    assert(state_religion::Choose(snapshot.index, snapshot.primary, snapshot.current,
        snapshot.pops).religion == 2);
    scope[4] = 1;
    assert(Rejects([&] { ReadPopulation(base, scope.data()); }));
    scope[4] = 0;
    scope[5] = Pointer(popA);
    assert(Rejects([&] { ReadPopulation(base, scope.data()); }));
    scope[5] = 0;
    assert(ReadPopulation(base, scope.data()).pops.size() == 2);
    scope[5] = Pointer(emptyPop);
    provinceB[0x12c / 4] = 2;
    assert(Rejects([&] { ReadPopulation(base, scope.data()); }));
    provinceB[0x12c / 4] = 1;
    otherCulture[0x27c / 4] = Pointer(popA);
    assert(Rejects([&] { ReadPopulation(base, scope.data()); }));
    otherCulture[0x27c / 4] = 0;
    popB[0x70 / 4] = 0;
    assert(Rejects([&] { ReadPopulation(base, scope.data()); }));
    popB[0x70 / 4] = Pointer(religionB);
    religionB[2] = 3;
    assert(Rejects([&] { ReadPopulation(base, scope.data()); }));
    religionB[2] = 2;
    country[0x9dc / 4] = country[0x9d8 / 4];
    assert(ReadPopulation(base, scope.data()).pops.empty());
    assert(!ExecutableMatches()); // Этот тестовый EXE не является Victoria II.
    assert(!Contracts(base));
    VirtualFree(reinterpret_cast<void*>(base), 0, MEM_RELEASE);
    std::cout << "state religion native reader: PASS\n";
}
