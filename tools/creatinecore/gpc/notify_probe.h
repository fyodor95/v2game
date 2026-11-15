// Квалификационная доставка странового уведомления штатным CEvent::run.
// Только из post-events фазы изолированной одиночной тестовой сессии.
#pragma once
#include "native_probe.h"

namespace gpc_probe_notify {
using namespace gpc_probe_native;

static bool InCoreModule(DWORD address) {
    MEMORY_BASIC_INFORMATION memory = {};
    HMODULE module = GetModuleHandleA("creatinecore.dll");
    return module && VirtualQuery((void*)address, &memory, sizeof(memory)) &&
        memory.AllocationBase == module;
}

static bool EventRunEntry(DWORD base) {
    const unsigned char original[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8};
    unsigned char entry[6] = {};
    if (!Read(base + 0x4a8120, entry, sizeof(entry))) return false;
    if (!memcmp(entry, original, sizeof(entry))) return true;
    // DEBUG_LOG ядра может установить диагностический EventRunProbeThunk.
    // Произвольный E9 не разрешён. Проверяем конкретный thunk, его модуль,
    // original trampoline и возврат после тех же шести украденных байтов.
    if (entry[0] != 0xe9) return false;
    DWORD relative = 0;
    memcpy(&relative, entry + 1, sizeof(relative));
    DWORD target = base + 0x4a8125 + relative;
    unsigned char thunk[20] = {}, trampoline[11] = {};
    if (!InCoreModule(target) || !Read(target, thunk, sizeof(thunk)) ||
        thunk[0] != 0x60 || thunk[1] != 0xff || thunk[2] != 0x74 ||
        thunk[3] != 0x24 || thunk[4] != 0x28 || thunk[5] != 0xe8 ||
        thunk[10] != 0x83 || thunk[11] != 0xc4 || thunk[12] != 0x04 ||
        thunk[13] != 0x61 || thunk[14] != 0xff || thunk[15] != 0x25) return false;
    DWORD callRelative = 0, originalSlot = 0, originalTarget = 0;
    memcpy(&callRelative, thunk + 6, sizeof(callRelative));
    memcpy(&originalSlot, thunk + 16, sizeof(originalSlot));
    if (!InCoreModule(target + 10 + callRelative) || !InCoreModule(originalSlot) ||
        !U32(originalSlot, &originalTarget) || !InCoreModule(originalTarget) ||
        !Read(originalTarget, trampoline, sizeof(trampoline)) ||
        memcmp(trampoline, original, sizeof(original)) || trampoline[6] != 0xe9) return false;
    memcpy(&relative, trampoline + 7, sizeof(relative));
    return originalTarget + 11 + relative == base + 0x4a8126;
}

static bool Contracts(DWORD base) {
    const unsigned char body[] = {0x8b,0x75,0x0c,0x8b,0x46,0x08,0x8b,0x56,0x18,0x8b,0x5e,0x0c,0x57,0x8b,0xf9};
    const unsigned char ctor[] = {0x55,0x8b,0xec,0x6a,0xff,0x68};
    const unsigned char self[] = {0x89,0x76,0x24,0x89,0x5e,0x28,0x89,0x56,0x2c};
    const unsigned char popupSelf[] = {0x89,0x77,0x34,0xc6,0x47,0x58,0x01};
    const unsigned char commandSelf[] = {0x89,0xbe,0x8c,0x00,0x00,0x00};
    // Все сравниваемые последовательности не содержат relocatable адресов.
    return gpc_probe_native::Contracts(base, false) && EventRunEntry(base) &&
        Signature(base, 0x4a8140, body, sizeof(body)) &&
        Signature(base, 0x4a8650, ctor, sizeof(ctor)) &&
        Signature(base, 0x4a86f2, self, sizeof(self)) &&
        Signature(base, 0x4a9062, popupSelf, sizeof(popupSelf)) &&
        Signature(base, 0x507e75, commandSelf, sizeof(commandSelf));
}

static bool FindCountryEvent(DWORD base, unsigned id, DWORD* event, DWORD* registry) {
    *event = *registry = 0;
    DWORD manager = 0, count = 0, table = 0, node = 0;
    if (!U32(base + 0xe58a98, &manager) || !manager ||
        !U32(manager + 4, &count) || !count || count > 1000000 ||
        !U32(manager + 8, &table) || !table ||
        !U32(table + (((0x27u << 16) + id) % count) * 4, &node)) return false;
    for (unsigned n = 0; node && n < 100000; ++n) {
        DWORD pair[2] = {}, key[2] = {};
        if (!Read(node, pair, sizeof(pair)) || !pair[0] ||
            !Read(pair[0] + 8, key, sizeof(key))) return false;
        if (key[0] == 0x27 && key[1] == id) { *event = pair[0]; break; }
        node = pair[1];
    }
    if (!*event || !U32(base + 0xf19b80, registry) || !*registry) return false;
    // Тот же discriminator, что у квалифицированного EventKind в CLI-мосте:
    // registry+0x20/+0x24 содержит province_event. Его элемент нельзя
    // вызывать как страновое уведомление.
    DWORD begin = 0, end = 0;
    if (!U32(*registry + 0x20, &begin) || !U32(*registry + 0x24, &end) ||
        end < begin || ((end - begin) & 3) || end - begin > 4000000 ||
        (end != begin && !begin)) return false;
    for (DWORD at = begin; at < end; at += 4) {
        DWORD candidate = 0;
        if (!U32(at, &candidate) || candidate == *event) return false;
    }
    return true;
}

static bool Dispatch(DWORD base, DWORD event, DWORD registry, const Identity& identity) {
    // CEventScope — 0x48 байт. Его временная жизнь повторяет штатный
    // country event loop 008A7CF9..008A7D0B / 008A7EE9..008A7EF4.
    DWORD scope[0x48 / 4] = {};
    typedef void* (__thiscall* ScopeCtor)(void*, int);
    typedef void (__thiscall* EventRun)(void*, void*, void*);
    __try {
        ((ScopeCtor)(base + 0x4a8650))(scope, 0);
        if (scope[0] != base + 0xa2c904 || scope[0x24 / 4] != (DWORD)scope) return false;
        scope[8 / 4] = identity.tag;
        scope[0x0c / 4] = identity.index;
        ((EventRun)(base + 0x4a8120))((void*)event, (void*)registry, scope);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    // Не исправляем никаких popup или command объектов вручную:
    // popup constructor 008A9062: [popup+0x34] = popup+0x10;
    // option command Execute 00907E75: [command+0x8C] = command+0x68.
    // Оба значения являются canonical pointer embedded scope+0x24.
    return true;
}

} // namespace gpc_probe_notify

// true означает возврат штатного dispatch; не доказывает появления окна,
// выбора кнопки или завершения AI command. Это проверяет контроллер/GUI.
static bool GpcProbeNotify(DWORD base, const char* tag, unsigned eventID, GpcProbeLog log) {
    using namespace gpc_probe_native;
    if (!log) return false;
    char allowed[64] = {};
    if (!GetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr, 0) ||
        !GetEnvironmentVariableA("VIC2_GPC_NOTIFY_ALLOWED", allowed, sizeof(allowed)) ||
        strcmp(allowed, "ISOLATED_SINGLE_PLAYER")) {
        log("GPC notify: отказ, отсутствует разрешение изолированного SP-теста"); return false;
    }
    World world = {};
    if (!tag || strlen(tag) != 3 || !eventID || !gpc_probe_notify::Contracts(base) || !GetWorld(base, &world)) {
        log("GPC notify: отказ проверки аргументов/EXE/мира"); return false;
    }
    DWORD packed = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!((tag[i] >= 'A' && tag[i] <= 'Z') || (tag[i] >= '0' && tag[i] <= '9'))) return false;
        packed |= (DWORD)(unsigned char)tag[i] << (8 * i);
    }
    Identity identity = {};
    DWORD recipient = 0;
    unsigned found = 0;
    for (DWORD index = 1; index < world.countryCount; ++index) {
        DWORD candidate = 0;
        Identity candidateIdentity = {};
        if (Country(world, index, &candidate, &candidateIdentity) && candidateIdentity.tag == packed) {
            recipient = candidate; identity = candidateIdentity; ++found;
        }
    }
    DWORD event = 0, registry = 0, dummyProvince = 0, dummyProvinceField = 0;
    DWORD ownedBegin = 0, ownedEnd = 0, optionsBegin = 0, optionsEnd = 0, option = 0;
    DWORD humans = 0, humansEnd = 0, human = 0, local = 0;
    if (found != 1 || !U32(recipient + 0x9d8, &ownedBegin) || !ownedBegin ||
        !U32(recipient + 0x9dc, &ownedEnd) || ownedEnd <= ownedBegin || ((ownedEnd - ownedBegin) & 3) ||
        !gpc_probe_notify::FindCountryEvent(base, eventID, &event, &registry) ||
        // Квалифицируется уведомление с одной кнопкой, как в плане.
        !U32(event + 0xb0, &optionsBegin) || !optionsBegin ||
        !U32(event + 0xb4, &optionsEnd) || optionsEnd < optionsBegin || optionsEnd - optionsBegin != 4 ||
        !U32(optionsBegin, &option) || !option ||
        // CEvent::run читает province[scope+0x18] даже в country scope.
        !U32(world.provinces, &dummyProvince) || !dummyProvince ||
        !U32(dummyProvince + 0x188, &dummyProvinceField) ||
        !U32(world.world + 0xaec, &humans) || !humans ||
        !U32(world.world + 0xaf0, &humansEnd) || humansEnd < humans ||
        ((humansEnd - humans) & 3) || identity.index >= (humansEnd - humans) / 4 ||
        !U32(humans + identity.index * 4, &human) || !U32(world.world + 0xb60, &local)) {
        log("GPC notify: отказ разрешения страны/события/scope event=%u tag=%s", eventID, tag); return false;
    }
    log("GPC notify: BEGIN event=%u tag=%s id=%lu human=%u local_recipient=%u registry=%08lX",
        eventID, tag, identity.index, human ? 1u : 0u, local == identity.index ? 1u : 0u, registry);
    bool ok = gpc_probe_notify::Dispatch(base, event, registry, identity);
    log("GPC notify: END dispatch_returned=%u event=%u tag=%s; GUI/AI completion требует отдельной проверки",
        ok ? 1u : 0u, eventID, tag);
    return ok;
}
