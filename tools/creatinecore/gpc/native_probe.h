// Только экспериментальный адаптер квалификации в изолированном стенде.
// Вызывать из подтверждённой фазы симуляции после штатных событий.
#pragma once
#include <windows.h>
#include <stdint.h>
#include <cstring>

typedef void (__cdecl* GpcProbeLog)(const char*, ...);

namespace gpc_probe_native {

static bool Read(DWORD address, void* output, size_t bytes) {
    if (!address || !output || !bytes || bytes > 16 * 1024 * 1024 ||
        address + bytes < address) return false;
    uintptr_t at = address, end = at + bytes;
    while (at < end) {
        MEMORY_BASIC_INFORMATION memory = {};
        if (!VirtualQuery((void*)at, &memory, sizeof(memory)) ||
            memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        DWORD protection = memory.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY) return false;
        uintptr_t next = (uintptr_t)memory.BaseAddress + memory.RegionSize;
        if (next <= at) return false;
        at = next;
    }
    __try { memcpy(output, (void*)address, bytes); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static bool U32(DWORD address, DWORD* output) { return Read(address, output, sizeof(*output)); }

static bool Signature(DWORD base, DWORD rva, const unsigned char* expected, size_t bytes) {
    unsigned char actual[32] = {};
    return bytes <= sizeof(actual) && Read(base + rva, actual, bytes) && !memcmp(actual, expected, bytes);
}

static bool Contracts(DWORD base, bool mutation) {
    const unsigned char lookup[] = {0x55,0x8b,0xec,0x51,0x8b,0x4d,0x08,0x53,0x56,0x8b,0xf0,0x8b,0x41,0x14,0x57,0x89};
    const unsigned char gp[] = {0x55,0x8b,0xec,0x83,0xec,0x08,0x56,0x8b,0x75,0x08,0x57,0x8b,0xf9,0x8b,0x4e,0x14};
    const unsigned char tech[] = {0x55,0x8b,0xec,0x83,0xec,0x08,0x8b,0x45,0x08,0x8b,0x50,0x08,0x8b,0x40,0x0c,0x56};
    const unsigned char add[] = {0x55,0x8b,0xec,0x81,0xec,0x10,0x01,0x00,0x00,0x53,0x8b,0x5d,0x0c,0x56,0x8b,0x75};
    const unsigned char ret12[] = {0xc2,0x0c,0x00};
    const unsigned char dirty[] = {0xc6,0x81,0x34,0x14,0x00,0x00,0x01};
    // Эти сигнатуры не содержат абсолютных адресов: ASLR-маски не нужны.
    // Все адреса ниже — RVA, поэтому base может отличаться от 0x00400000.
    WORD mz = 0;
    return Read(base, &mz, sizeof(mz)) && mz == IMAGE_DOS_SIGNATURE &&
        Signature(base, 0x17b0a0, lookup, sizeof(lookup)) &&
        Signature(base, 0x4ee1f0, gp, sizeof(gp)) &&
        Signature(base, 0x4c5c30, tech, sizeof(tech)) &&
        (!mutation || (Signature(base, 0x0b61a0, add, sizeof(add)) &&
                       Signature(base, 0x0b6283, ret12, sizeof(ret12)) &&
                       Signature(base, 0x47e8a4, dirty, sizeof(dirty))));
}

struct World {
    DWORD world, manager, countries, countryCount;
    DWORD provinces, provinceCount, gpBegin, gpCount;
};

static bool GetWorld(DWORD base, World* out) {
    memset(out, 0, sizeof(*out));
    DWORD provinceEnd = 0, gpEnd = 0;
    if (!U32(base + 0xe588e8, &out->world) || !out->world ||
        !U32(base + 0xe587e4, &out->manager) || !out->manager ||
        !U32(out->manager, &out->countryCount) || !out->countryCount || out->countryCount > 65536 ||
        !U32(out->manager + 4, &out->countries) || !out->countries ||
        !U32(out->world + 0xacc, &out->provinces) || !out->provinces ||
        !U32(out->world + 0xad0, &provinceEnd) || provinceEnd < out->provinces ||
        ((provinceEnd - out->provinces) & 3) ||
        !U32(out->world + 0xbe0, &out->gpBegin) ||
        !U32(out->world + 0xbe4, &gpEnd) || gpEnd < out->gpBegin ||
        ((gpEnd - out->gpBegin) & 3)) return false;
    out->provinceCount = (provinceEnd - out->provinces) / 4;
    out->gpCount = (gpEnd - out->gpBegin) / 4;
    return out->provinceCount && out->provinceCount <= 1000000 &&
        out->gpCount <= out->countryCount && (!out->gpCount || out->gpBegin);
}

struct Identity { DWORD tag, index; };

static void TagText(DWORD packed, char output[5]) {
    memcpy(output, &packed, 4);
    output[3] = output[4] = 0;
}

static bool Country(const World& world, DWORD index, DWORD* country, Identity* identity) {
    if (!index || index >= world.countryCount ||
        !U32(world.countries + index * 4, country) || !*country ||
        !Read(*country + 0x1c, identity, sizeof(*identity)) || identity->index != index ||
        !identity->tag || identity->tag == 0x2d2d2d) return false;
    return true;
}

struct GameString {
    union { char inlineText[16]; const char* pointer; } text;
    DWORD length, capacity;
};
static_assert(sizeof(GameString) == 24, "Probe requires x86 game string layout");

static bool ReadText(DWORD stringAddress, char* output, size_t capacity) {
    GameString value = {};
    if (!capacity || !Read(stringAddress, &value, sizeof(value)) ||
        value.length > value.capacity || value.length >= capacity || value.length > 1024 ||
        (value.capacity < 16 && value.length >= 16)) return false;
    DWORD chars = value.capacity < 16 ? stringAddress : (DWORD)value.text.pointer;
    if (value.length && !Read(chars, output, value.length)) return false;
    output[value.length] = 0;
    return true;
}

static bool Nationalism(DWORD base, DWORD* technology, DWORD* index) {
    *technology = *index = 0;
    DWORD manager = 0, buckets = 0, tablePointer = 0;
    if (!U32(base + 0xe5eadc, &manager) || !manager ||
        !U32(manager + 0x14, &buckets) || !buckets || buckets > 1000000 ||
        !U32(manager + 0x18, &tablePointer) || !tablePointer) return false;
    // Lookup читает строку и не сохраняет её: заимствованный буфер живёт
    // весь вызов. Он не передаётся игровому деструктору или аллокатору.
    const char name[] = "nationalism_n_imperialism";
    GameString key = {};
    key.text.pointer = name;
    key.length = (DWORD)strlen(name);
    key.capacity = key.length;
    DWORD table = manager + 0x10, target = base + 0x17b0a0, found = 0;
    GameString* keyPointer = &key;
    __try {
        __asm {
            push keyPointer
            mov eax, table
            call target
            mov found, eax
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!found) return true; // Валидный lookup: технология отсутствует.
    char actual[64] = {};
    if (!ReadText(found + 0x2ac, actual, sizeof(actual)) || strcmp(actual, name) ||
        !U32(found + 0x2e4, index)) return false;
    *technology = found;
    return true;
}

static bool TechnologyLevel(DWORD country, DWORD index, int* level) {
    DWORD state = 0, begin = 0, end = 0;
    return U32(country + 0xbcc, &state) && state && U32(state + 0x2b8, &begin) && begin &&
        U32(state + 0x2bc, &end) && end >= begin && !((end - begin) & 3) &&
        index < (end - begin) / 4 && Read(begin + index * 4, level, sizeof(*level));
}

static bool CoreCount(const World& world, DWORD province, DWORD recipient, unsigned* matches) {
    DWORD begin = 0, end = 0;
    *matches = 0;
    if (!U32(province + 0x148, &begin) || !U32(province + 0x14c, &end) ||
        end < begin || ((end - begin) & 7) || (end - begin) / 8 > world.countryCount ||
        (end != begin && !begin)) return false;
    for (DWORD at = begin; at < end; at += 8) {
        Identity core = {}, canonical = {};
        DWORD country = 0;
        if (!Read(at, &core, sizeof(core)) ||
            !Country(world, core.index, &country, &canonical) || core.tag != canonical.tag) return false;
        if (core.index == recipient) ++*matches;
    }
    return true;
}

static bool ReverseCoreCount(const World& world, DWORD country, DWORD provinceID, unsigned* matches) {
    DWORD at = 0, tail = 0, expectedCount = 0, previous = 0, walked = 0;
    *matches = 0;
    if (!U32(country + 0x9f8, &at) || !U32(country + 0x9fc, &tail) ||
        !U32(country + 0xa00, &expectedCount) || expectedCount > world.provinceCount) return false;
    while (at) {
        DWORD node[3] = {};
        if (++walked > expectedCount || !Read(at, node, sizeof(node)) ||
            node[1] != previous || node[0] >= world.provinceCount) return false;
        if (node[0] == provinceID) ++*matches;
        previous = at;
        at = node[2];
    }
    return walked == expectedCount && previous == tail;
}

static bool InvokeAddCore(DWORD function, DWORD province, const Identity* recipient, DWORD country) {
    typedef void (__stdcall* AddCore)(void*, const Identity*, int);
    __try {
        ((AddCore)function)((void*)province, recipient, 1);
        // Отдельная запись из штатного add_core Execute (0087E8A4).
        *(unsigned char*)(country + 0x1434) = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

} // namespace gpc_probe_native

static void GpcProbeObserve(DWORD base, GpcProbeLog log, unsigned callcount) {
    using namespace gpc_probe_native;
    if (!log || callcount != 1) return;
    World world = {};
    if (!Contracts(base, false) || !GetWorld(base, &world)) {
        log("GPC native observe: отказ проверки EXE/мира"); return;
    }
    DWORD technology = 0, technologyIndex = 0;
    bool lookup = Nationalism(base, &technology, &technologyIndex);
    log("GPC native observe: countries=%lu provinces=%lu gp_count=%lu nationalism_lookup=%s technology=%08lX index=%lu",
        world.countryCount, world.provinceCount, world.gpCount, lookup ? "ok" : "ERROR",
        technology, technologyIndex);
    for (DWORD n = 0; n < world.gpCount; ++n) {
        DWORD pointer = 0, country = 0, primary = 0, accepted = 0, begin = 0, end = 0;
        Identity identity = {};
        unsigned char gp = 0;
        char tag[5] = {};
        if (!U32(world.gpBegin + n * 4, &pointer) || !pointer ||
            !Read(pointer + 0x1c, &identity, sizeof(identity)) ||
            !Country(world, identity.index, &country, &identity) || country != pointer ||
            !Read(country + 0x31, &gp, sizeof(gp)) ||
            !U32(country + 0xe64, &primary) || !U32(country + 0xe54, &accepted) ||
            !U32(country + 0x9d8, &begin) || !U32(country + 0x9dc, &end) ||
            end < begin || ((end - begin) & 3)) {
            log("GPC native observe: GP entry=%lu INVALID", n); continue;
        }
        TagText(identity.tag, tag);
        int level = -1;
        bool levelValid = lookup && (!technology || TechnologyLevel(country, technologyIndex, &level));
        log("GPC native observe: GP=%s id=%lu gp_flag=%u owned=%lu primary_culture_ptr=%08lX nationalism_valid=%u level=%d",
            tag, identity.index, (unsigned)gp, (end - begin) / 4, primary, (unsigned)levelValid, level);
        // Смещения имён culture не квалифицированы: указатели явно помечены
        // как указатели и не выдаются за стабильные межпроцессные ID.
        unsigned acceptedCount = 0;
        while (accepted && acceptedCount < 4096) {
            DWORD node[3] = {};
            if (!Read(accepted, node, sizeof(node))) break;
            log("GPC native observe: GP=%s accepted[%u]_culture_ptr=%08lX", tag, acceptedCount, node[0]);
            ++acceptedCount;
            accepted = node[2];
        }
        log("GPC native observe: GP=%s accepted_count=%u list_valid=%u", tag, acceptedCount, accepted == 0 ? 1u : 0u);
    }
}

// Не производственный API. Дополнительное разрешение задаёт только
// контроллер изолированной одиночной сессии; функция сама ничего не запускает.
static bool GpcProbeAddCore(DWORD base, unsigned provinceID, const char* tag, GpcProbeLog log) {
    using namespace gpc_probe_native;
    if (!log) return false;
    char permission[64] = {};
    if (!GetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr, 0) ||
        !GetEnvironmentVariableA("VIC2_GPC_MUTATION_ALLOWED", permission, sizeof(permission)) ||
        strcmp(permission, "ISOLATED_SINGLE_PLAYER")) {
        log("GPC native add_core: отказ, отсутствует разрешение изолированного SP-теста"); return false;
    }
    World world = {};
    if (!tag || strlen(tag) != 3 || !Contracts(base, true) || !GetWorld(base, &world) ||
        !provinceID || provinceID >= world.provinceCount) {
        log("GPC native add_core: отказ проверки аргументов/EXE/мира"); return false;
    }
    DWORD packed = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!((tag[i] >= 'A' && tag[i] <= 'Z') || (tag[i] >= '0' && tag[i] <= '9'))) {
            log("GPC native add_core: недопустимый TAG"); return false;
        }
        packed |= (DWORD)(unsigned char)tag[i] << (i * 8);
    }
    DWORD recipient = 0;
    Identity identity = {};
    unsigned resolved = 0;
    for (DWORD index = 1; index < world.countryCount; ++index) {
        DWORD candidate = 0;
        Identity candidateIdentity = {};
        if (Country(world, index, &candidate, &candidateIdentity) && candidateIdentity.tag == packed) {
            recipient = candidate; identity = candidateIdentity; ++resolved;
        }
    }
    DWORD province = 0, actualID = 0;
    unsigned before = 0, reverseBefore = 0;
    if (resolved != 1 || !U32(world.provinces + provinceID * 4, &province) || !province ||
        !U32(province + 0x58, &actualID) || actualID != provinceID ||
        !CoreCount(world, province, identity.index, &before) ||
        !ReverseCoreCount(world, recipient, provinceID, &reverseBefore) ||
        before != reverseBefore || before > 1) {
        log("GPC native add_core: отказ, identity/контейнеры не согласованы province=%u tag=%s", provinceID, tag); return false;
    }
    if (before == 1) {
        log("GPC native add_core: уже имеется province=%u tag=%s, вызова/истории нет", provinceID, tag); return true;
    }
    log("GPC native add_core: BEGIN province=%u tag=%s id=%lu history=1", provinceID, tag, identity.index);
    if (!InvokeAddCore(base + 0xb61a0, province, &identity, recipient)) {
        log("GPC native add_core: ERROR native exception, возможна частичная выдача; автоматического повтора нет"); return false;
    }
    unsigned after = 0, reverseAfter = 0;
    unsigned char dirty = 0;
    bool ok = CoreCount(world, province, identity.index, &after) &&
        ReverseCoreCount(world, recipient, provinceID, &reverseAfter) &&
        Read(recipient + 0x1434, &dirty, sizeof(dirty)) && after == 1 && reverseAfter == 1 && dirty == 1;
    log("GPC native add_core: END ok=%u province=%u tag=%s forward=%u reverse=%u dirty=%u",
        ok ? 1u : 0u, provinceID, tag, after, reverseAfter, (unsigned)dirty);
    return ok;
}
