#pragma once
// Только читающий снимок из общей фазы симуляции изолированного стенда.
// Подключать после native_probe.h; одноразовость обеспечивает вызывающая сторона.
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <limits>
#include <stdexcept>
#include <cstdio>

namespace gpc_snapshot_probe {
using namespace gpc_probe_native;

static DWORD Read32(DWORD at) {
    DWORD result = 0;
    if (!U32(at, &result)) throw std::runtime_error("unreadable field");
    return result;
}

static std::string Quote(const std::string& source) {
    // Технические имена читаются из CP1251; отчёт всегда UTF-8.
    int count = MultiByteToWideChar(1251, 0, source.data(), (int)source.size(), nullptr, 0);
    std::wstring wide(count, L'\0');
    if (count) MultiByteToWideChar(1251, 0, source.data(), (int)source.size(), &wide[0], count);
    int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, nullptr, 0, nullptr, nullptr);
    std::string utf8(bytes, '\0');
    if (bytes) WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, &utf8[0], bytes, nullptr, nullptr);
    std::ostringstream out;
    out << '"';
    const char hex[] = "0123456789abcdef";
    for (unsigned char c : utf8) {
        if (c == '"' || c == '\\') out << '\\' << char(c);
        else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << char(c);
    }
    out << '"';
    return out.str();
}

struct Culture { DWORD pointer; std::string name; };

static std::vector<Culture> CultureTable(DWORD base) {
    DWORD manager = Read32(base + 0xe587e8);
    DWORD begin = Read32(manager + 0xc), end = Read32(manager + 0x10);
    if (!begin || end < begin || ((end - begin) & 3) ||
        (end - begin) / 4 > 65536 ||
        (end - begin) / 4 != Read32(base + 0xe58a20))
        throw std::runtime_error("culture vector bounds");
    std::vector<Culture> result;
    std::set<std::string> names;
    for (DWORD at = begin; at < end; at += 4) {
        DWORD object = Read32(at);
        char name[1025] = {};
        if (!object || Read32(object + 8) != result.size() ||
            !ReadText(object + 0x18, name, sizeof(name)) || !name[0] ||
            !names.insert(name).second) throw std::runtime_error("culture identity");
        result.push_back({object, name});
    }
    if (result.empty()) throw std::runtime_error("empty culture vector");
    return result;
}

static DWORD CultureID(DWORD pointer, const std::vector<Culture>& cultures) {
    DWORD index = Read32(pointer + 8);
    if (index >= cultures.size() || cultures[index].pointer != pointer)
        throw std::runtime_error("culture pointer identity");
    return index;
}

// SEH изолирован от функций с объектами C++, чтобы MSVC не требовал unwinding.
static bool NativeWinner(DWORD function, DWORD aggregate, DWORD* result) {
    __try {
        __asm {
            mov eax, aggregate
            call function
            mov edx, result
            mov [edx], eax
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static std::string Tag(const Identity& identity) {
    char tag[5] = {};
    TagText(identity.tag, tag);
    for (int i = 0; i < 3; ++i)
        if (!((tag[i] >= 'A' && tag[i] <= 'Z') || (tag[i] >= '0' && tag[i] <= '9')))
            throw std::runtime_error("country tag characters");
    return tag;
}

static std::string CanonicalTag(const World& world, const Identity& identity) {
    DWORD country = 0; Identity canonical = {};
    if (!Country(world, identity.index, &country, &canonical) || canonical.tag != identity.tag)
        throw std::runtime_error("country identity");
    return Tag(canonical);
}

static std::string Document(DWORD base, const World& world,
                            unsigned* mismatchCount, unsigned* provinceCount) {
    const auto cultures = CultureTable(base);
    DWORD technology = 0, technologyIndex = 0;
    if (!Nationalism(base, &technology, &technologyIndex))
        throw std::runtime_error("nationalism lookup");
    DWORD popDB = Read32(base + 0xf1bb34);
    DWORD popBegin = Read32(popDB + 0xc), popEnd = Read32(popDB + 0x10);
    if (!popBegin || popEnd < popBegin || ((popEnd - popBegin) & 3) ||
        (popEnd - popBegin) / 4 > 4096 || popEnd == popBegin)
        throw std::runtime_error("POP type vector");
    const DWORD types = (popEnd - popBegin) / 4;
    std::ostringstream out;
    out << "{\"schema\":1,\"phase\":\"after-events\",\"date_raw\":"
        << Read32(world.world + 0xb0c) << ",\"technology_present\":"
        << (technology ? "true" : "false") << ",\"culture_order\":[";
    for (size_t n = 0; n < cultures.size(); ++n) {
        if (n) out << ',';
        out << "{\"id\":" << n << ",\"name\":" << Quote(cultures[n].name) << '}';
    }
    out << "],\"great_powers\":[";
    std::set<DWORD> gpIDs;
    for (DWORD n = 0; n < world.gpCount; ++n) {
        DWORD pointer = Read32(world.gpBegin + n * 4);
        Identity identity = {};
        if (!Read(pointer + 0x1c, &identity, sizeof(identity)))
            throw std::runtime_error("GP identity unreadable");
        if (!gpIDs.insert(identity.index).second) throw std::runtime_error("duplicate GP");
        if (n) out << ',';
        out << Quote(CanonicalTag(world, identity));
    }
    out << "],\"countries\":{";
    bool first = true;
    for (DWORD n = 1; n < world.countryCount; ++n) {
        DWORD country = 0; Identity identity = {};
        if (!Country(world, n, &country, &identity)) continue;
        unsigned char gp = 0;
        if (!Read(country + 0x31, &gp, sizeof(gp))) throw std::runtime_error("GP flag unreadable");
        int level = 0;
        if (technology && !TechnologyLevel(country, technologyIndex, &level))
            throw std::runtime_error("technology state unreadable");
        if (!first) out << ',';
        first = false;
        out << Quote(Tag(identity)) << ":{\"id\":" << n << ",\"gp\":" << (gp ? "true" : "false")
            << ",\"gp_list_member\":" << (gpIDs.count(n) ? "true" : "false")
            << ",\"technology_level\":" << level << ",\"technology_researched\":"
            << ((technology && level > 0) ? "true" : "false") << ",\"primary_culture\":";
        DWORD primary = Read32(country + 0xe64);
        if (primary) out << Quote(cultures[CultureID(primary, cultures)].name);
        else out << "null";
        out << ",\"accepted_cultures\":[";
        DWORD node = Read32(country + 0xe54), previous = 0;
        std::set<DWORD> visited;
        bool acceptedFirst = true;
        while (node) {
            DWORD data[3] = {};
            if (visited.size() >= 4096 || !visited.insert(node).second ||
                !Read(node, data, sizeof(data)) || data[1] != previous)
                throw std::runtime_error("accepted culture list");
            if (!acceptedFirst) out << ',';
            acceptedFirst = false;
            out << Quote(cultures[CultureID(data[0], cultures)].name);
            previous = node; node = data[2];
        }
        out << "]}";
    }
    out << "},\"provinces\":[";
    first = true;
    for (DWORD id = 1; id < world.provinceCount; ++id) {
        DWORD province = Read32(world.provinces + id * 4);
        if (!province) continue;
        if (Read32(province + 0x58) != id) throw std::runtime_error("province identity");
        if (!first) out << ',';
        first = false;
        ++*provinceCount;
        out << "{\"id\":" << id << ",\"owner\":";
        Identity owner = {};
        if (!Read(province + 0x128, &owner, sizeof(owner)))
            throw std::runtime_error("province owner");
        if (owner.index) out << Quote(CanonicalTag(world, owner));
        else out << "null";
        out << ",\"cores\":[";
        DWORD begin = Read32(province + 0x148), end = Read32(province + 0x14c);
        if (end < begin || ((end - begin) & 7) || (end - begin) / 8 > world.countryCount ||
            (begin != end && !begin)) throw std::runtime_error("core vector bounds");
        for (DWORD at = begin; at < end; at += 8) {
            Identity core = {};
            if (!Read(at, &core, sizeof(core))) throw std::runtime_error("core identity unreadable");
            if (at != begin) out << ',';
            out << Quote(CanonicalTag(world, core));
        }
        std::vector<uint64_t> totals(cultures.size(), 0);
        std::set<DWORD> seenPops;
        DWORD lists = Read32(province + 0x194);
        for (DWORD type = 0; lists && type < types; ++type) {
            DWORD expectedType = Read32(popBegin + type * 4);
            DWORD pop = Read32(lists + type * 16);
            while (pop) {
                if (seenPops.size() >= 1000000 || !seenPops.insert(pop).second)
                    throw std::runtime_error("POP list cycle or duplicate");
                int size = 0;
                if (!Read(pop + 0x58, &size, sizeof(size)) || size < 0 ||
                    Read32(pop + 0x64) != province || Read32(pop + 0x68) != expectedType)
                    throw std::runtime_error("POP size/province/type");
                DWORD culture = CultureID(Read32(pop + 0x6c), cultures);
                if (uint64_t(size) > (std::numeric_limits<uint64_t>::max)() - totals[culture])
                    throw std::runtime_error("POP sum overflow");
                totals[culture] += size;
                pop = Read32(pop + 0x27c);
            }
        }
        out << "],\"population\":{";
        uint64_t maximum = 0;
        bool popFirst = true;
        DWORD cache = Read32(province + 0x1f0);
        bool matches = true;
        for (DWORD c = 0; c < cultures.size(); ++c) {
            if (totals[c] > maximum) maximum = totals[c];
            if ((cache ? Read32(cache + c * 4) : 0) != totals[c]) matches = false;
            if (totals[c]) {
                if (!popFirst) out << ',';
                popFirst = false;
                out << Quote(cultures[c].name) << ':' << totals[c];
            }
        }
        out << "},\"dominant_cultures\":[";
        bool dominantFirst = true;
        DWORD expectedWinner = 0;
        for (DWORD c = 0; maximum && c < cultures.size(); ++c) if (totals[c] == maximum) {
            if (dominantFirst) expectedWinner = c;
            else out << ',';
            dominantFirst = false;
            out << Quote(cultures[c].name);
        }
        DWORD native = 0;
        if (cache && !NativeWinner(base + 0x5692c0, province + 0x1a8, &native))
            throw std::runtime_error("native winner exception");
        DWORD nativeID = native ? CultureID(native, cultures) : 0;
        const bool winnerMatches = !maximum || (native && expectedWinner == nativeID);
        if (!matches || !winnerMatches) ++*mismatchCount;
        out << "],\"native_winner\":";
        if (native) out << Quote(cultures[nativeID].name);
        else out << "null";
        out << ",\"native_winner_id\":";
        if (native) out << nativeID;
        else out << "null";
        out << ",\"pop_count\":" << seenPops.size()
            << ",\"cache_matches\":" << (matches ? "true" : "false")
            << ",\"winner_matches\":" << (winnerMatches ? "true" : "false") << '}';
    }
    out << "],\"mismatches\":" << *mismatchCount << "}\n";
    return out.str();
}

} // namespace gpc_snapshot_probe

static void GpcProbeSnapshot(DWORD base, GpcProbeLog log) {
    using namespace gpc_probe_native;
    char enabled[8] = {};
    if (!log || !GetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr, 0) ||
        !GetEnvironmentVariableA("VIC2_GPC_PROBE_SNAPSHOT", enabled, sizeof(enabled)) || strcmp(enabled, "1")) return;
    try {
        World world = {};
        const unsigned char before[] = {0x53, 0x8b, 0x1d};
        const unsigned char after[] = {0x56, 0x57, 0x8b, 0x78, 0x48};
        DWORD countAddress = 0;
        if (!Contracts(base, false) || !GetWorld(base, &world) ||
            !Signature(base, 0x5692c0, before, sizeof(before)) ||
            !U32(base + 0x5692c3, &countAddress) || countAddress != base + 0xe58a20 ||
            !Signature(base, 0x5692c7, after, sizeof(after)))
            throw std::runtime_error("snapshot contracts");
        unsigned mismatches = 0, provinces = 0;
        const auto json = gpc_snapshot_probe::Document(base, world, &mismatches, &provinces);
        char exe[MAX_PATH] = {}, output[MAX_PATH] = {}, relative[MAX_PATH] = {};
        DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
        if (!n || n >= MAX_PATH) throw std::runtime_error("executable path");
        char* separator = strrchr(exe, '\\');
        if (!separator) throw std::runtime_error("executable directory");
        *separator = 0;
        const char* directory = strrchr(exe, '\\');
        if (!directory || _stricmp(directory + 1, "game"))
            throw std::runtime_error("snapshot requires isolated game/userdata layout");
        char userdata[MAX_PATH] = {};
        if (sprintf_s(userdata, "%s\\..\\userdata", exe) < 0 ||
            !(GetFileAttributesA(userdata) & FILE_ATTRIBUTE_DIRECTORY) ||
            GetFileAttributesA(userdata) == INVALID_FILE_ATTRIBUTES)
            throw std::runtime_error("isolated userdata directory missing");
        if (sprintf_s(relative, "%s\\..\\userdata\\gpc_snapshot_%lu.json", exe, GetCurrentProcessId()) < 0 ||
            !(n = GetFullPathNameA(relative, MAX_PATH, output, nullptr)) || n >= MAX_PATH)
            throw std::runtime_error("snapshot path");
        HANDLE file = CreateFileA(output, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("snapshot exclusive create");
        DWORD written = 0;
        bool success = json.size() <= MAXDWORD && WriteFile(file, json.data(), (DWORD)json.size(), &written, nullptr)
                       && written == json.size() && FlushFileBuffers(file);
        CloseHandle(file);
        if (!success) throw std::runtime_error("snapshot write incomplete");
        log("GPC snapshot: path=%s bytes=%lu provinces=%u mismatches=%u", output, written, provinces, mismatches);
    } catch (const std::exception& error) {
        log("GPC snapshot: ERROR %s", error.what());
    }
}
