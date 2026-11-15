#pragma once
#include "debug_api.h"
#include "runtime_status.h"
#include "world_reader.h"
#include "scheduler.h"
#include <locale>
#include <sstream>

namespace gpc_runtime_debug {

class Error : public std::runtime_error {
public:
    std::uint32_t code;
    Error(std::uint32_t value, const char* message) : std::runtime_error(message), code(value) {}
};

inline void Require(bool condition, std::uint32_t code, const char* message) {
    if (!condition) throw Error(code, message);
}

inline void Validate(const V2GpcDebugRequest& request) {
    Require(request.size == sizeof(request) && request.version == V2_GPC_DEBUG_VERSION &&
            request.operation >= V2_GPC_DEBUG_PREVIEW && request.operation <= V2_GPC_DEBUG_RUN,
            V2_GPC_DEBUG_ARGUMENT, "E_GPC_DEBUG_ARGUMENT");
    const auto length = strnlen_s(request.name, sizeof(request.name));
    Require(length && length <= 48 &&
            strspn(request.name, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == length,
            V2_GPC_DEBUG_ARGUMENT, "E_GPC_DEBUG_NAME");
}

inline bool Flag(DWORD address) {
    unsigned char value = 0;
    Require(gpc_probe_native::Read(address, &value, 1) && value <= 1,
            V2_GPC_DEBUG_NATIVE, "E_GPC_DEBUG_ENGINE_FLAG");
    return value != 0;
}

struct Phase { DWORD world, idler, manager, raw_date; bool multiplayer; };

inline Phase Guard(DWORD base, DWORD simulationThread, const V2GpcRuntimeStatus& cache, bool force) {
    using gpc_snapshot_probe::Read32;
    Require(GetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr, 0) != 0,
            V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_TEST_SESSION");
    Require(simulationThread && simulationThread == GetCurrentThreadId(),
            V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_OWNER_THREAD");
    Require(!cache.failed, V2_GPC_DEBUG_FAILED, "E_GPC_DEBUG_FAILED");
    Require(cache.enabled && cache.has_schedule && !cache.busy,
            V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_PHASE");
    const unsigned char getter[] = {0x8b,0x81,0xe8,0x17,0,0,0x8b,0x80,0x2c,0x01,0,0,0xc3};
    const unsigned char paused[] = {0x8a,0x81,0x38,0x15,0,0,0xc3};
    const unsigned char world_mode[] = {0x88,0x82,0xb8,0x0b,0,0,0x88,0x43,0x5c};
    Require(gpc_probe_native::Signature(base, 0x251d10, getter, sizeof(getter)) &&
            gpc_probe_native::Signature(base, 0x247560, paused, sizeof(paused)) &&
            gpc_probe_native::Signature(base, 0x24bd2a, world_mode, sizeof(world_mode)),
            V2_GPC_DEBUG_NATIVE, "E_GPC_DEBUG_ENGINE_ABI");
    Phase phase = {};
    phase.world = Read32(base + 0xe588e8);
    phase.idler = Read32(phase.world + 0xb24);
    Require(phase.idler && Read32(phase.idler) == base + 0xa0a34c &&
            Read32(base + 0xa0a34c + 0x44) == base + 0x251d10 &&
            Read32(base + 0xa0a34c + 0x158) == base + 0x247560,
            V2_GPC_DEBUG_NATIVE, "E_GPC_DEBUG_IDLER");
    const bool idlerMP = Flag(phase.idler + 0x5c), worldMP = Flag(phase.world + 0xbb8);
    Require(!force || (!idlerMP && !worldMP), V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_MULTIPLAYER_RUN");
    Require(idlerMP == worldMP, V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_MODE_MISMATCH");
    phase.multiplayer = idlerMP;
    phase.manager = Read32(Read32(phase.idler + 0x17e8) + 0x12c);
    Require(phase.manager && !Flag(phase.manager + 0x234),
            V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_COMMAND_EXECUTING");
    Require(Flag(phase.idler + 0x1538), V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_NOT_PAUSED");
    phase.raw_date = Read32(phase.world + 0xb0c);
    Require(phase.raw_date && cache.last_completed_raw_date == phase.raw_date &&
            cache.last_callback_raw_date == phase.raw_date,
            V2_GPC_DEBUG_GUARD, "E_GPC_DEBUG_INCOMPLETE_DATE");
    gpc::ScheduleDayFromRaw(phase.raw_date);
    return phase;
}

inline std::string Utf8(const char* input) {
    int count = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, input, -1, nullptr, 0);
    Require(count > 0, V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH_ENCODING");
    std::vector<wchar_t> wide(static_cast<size_t>(count));
    Require(MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, input, -1, wide.data(), count) == count,
            V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH_ENCODING");
    count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), -1, nullptr, 0, nullptr, nullptr);
    Require(count > 0, V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH_ENCODING");
    std::string result(static_cast<size_t>(count), '\0');
    Require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), -1, &result[0], count,
                               nullptr, nullptr) == count,
            V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH_ENCODING");
    result.pop_back();
    return result;
}

class NewJson {
    HANDLE file_ = INVALID_HANDLE_VALUE;
public:
    std::string path;
    explicit NewJson(const char* name) {
        char exe[MAX_PATH] = {};
        DWORD used = GetModuleFileNameA(nullptr, exe, sizeof(exe));
        Require(used && used < sizeof(exe) && strstr(exe, "\\.work\\"),
                V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_ISOLATED_PATH");
        char* slash = strrchr(exe, '\\');
        Require(slash != nullptr, V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_ISOLATED_PATH");
        *slash = 0; slash = strrchr(exe, '\\');
        Require(slash && !strcmp(slash + 1, "game"), V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_ISOLATED_PATH");
        *slash = 0;
        const std::string userdata = std::string(exe) + "\\userdata";
        Require(userdata.size() < MAX_PATH, V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH");
        DWORD attributes = GetFileAttributesA(userdata.c_str());
        Require(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(attributes & FILE_ATTRIBUTE_REPARSE_POINT), V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_USERDATA");
        const std::string directory = userdata + "\\gpc-debug";
        Require(directory.size() < MAX_PATH, V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH");
        Require(CreateDirectoryA(directory.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS,
                V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_OUTPUT_DIRECTORY");
        attributes = GetFileAttributesA(directory.c_str());
        Require(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(attributes & FILE_ATTRIBUTE_REPARSE_POINT), V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_OUTPUT_DIRECTORY");
        const std::string filename = directory + "\\" + name + ".json";
        Require(filename.size() < MAX_PATH, V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH");
        path = Utf8(filename.c_str());
        Require(path.size() < sizeof(((V2GpcDebugResult*)nullptr)->path),
                V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_PATH");
        file_ = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(file_ != INVALID_HANDLE_VALUE, V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_OUTPUT_EXISTS_OR_DENIED");
    }
    ~NewJson() { if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_); }
    NewJson(const NewJson&) = delete;
    NewJson& operator=(const NewJson&) = delete;
    void Write(const std::string& json) {
        LARGE_INTEGER zero = {};
        Require(SetFilePointerEx(file_, zero, nullptr, FILE_BEGIN) != 0,
                V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_OUTPUT_SEEK");
        size_t offset = 0;
        while (offset < json.size()) {
            DWORD written = 0;
            DWORD amount = static_cast<DWORD>((std::min)(json.size() - offset, size_t(1024 * 1024)));
            Require(WriteFile(file_, json.data() + offset, amount, &written, nullptr) && written,
                    V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_OUTPUT_WRITE");
            offset += written;
        }
        Require(SetEndOfFile(file_) && FlushFileBuffers(file_),
                V2_GPC_DEBUG_OUTPUT, "E_GPC_DEBUG_OUTPUT_FLUSH");
    }
};

inline void Quoted(std::ostream& stream, const std::string& value) {
    static const char hex[] = "0123456789abcdef";
    stream << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') stream << '\\' << char(c);
        else if (c < 32) stream << "\\u00" << hex[c >> 4] << hex[c & 15];
        else stream << char(c);
    }
    stream << '"';
}

inline void Ids(std::ostream& stream, std::vector<gpc::Id> ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    stream << '[';
    for (size_t n = 0; n < ids.size(); ++n) { if (n) stream << ','; stream << ids[n]; }
    stream << ']';
}

inline std::string Json(const gpc_world_reader::WorldSnapshot& snapshot,
                        const std::vector<gpc::CoreGrant>& grants,
                        const gpc::PersistState& before, const V2GpcDebugResult& result,
                        std::uint32_t operation, bool multiplayer) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schema\":1,\"operation\":";
    Quoted(out, operation == V2_GPC_DEBUG_RUN ? "run" : operation == V2_GPC_DEBUG_SNAPSHOT ? "snapshot" : "preview");
    out << ",\"raw_date\":" << snapshot.date_raw << ",\"multiplayer\":" << (multiplayer ? "true" : "false")
        << ",\"primary_only\":true,\"all_ties\":true,\"technology_present\":"
        << (snapshot.technology_present ? "true" : "false")
        << ",\"schedule_before\":{\"schema\":" << before.schema << ",\"seed\":" << before.seed
        << ",\"rng\":" << before.rng << ",\"last_day\":" << before.last_day << ",\"pass\":" << before.pass_sequence
        << ",\"checksum\":" << gpc::ScheduleChecksum(before) << "},\"result\":{\"committed\":"
        << (result.committed ? "true" : "false") << ",\"pairs\":" << result.pairs << ",\"added\":" << result.added
        << ",\"pass\":" << result.pass << ",\"rng\":" << result.rng << ",\"state_digest\":" << result.state_digest
        << ",\"core_digest\":" << result.core_digest << "},\"countries\":[";
    for (size_t n = 0; n < snapshot.countries.size(); ++n) {
        const auto& country = snapshot.countries[n];
        if (n) out << ',';
        out << "{\"id\":" << country.id << ",\"tag\":"; Quoted(out, snapshot.tags.at(country.id));
        out << ",\"exists\":" << (country.exists ? "true" : "false")
            << ",\"great_power\":" << (country.great_power ? "true" : "false")
            << ",\"technology_researched\":" << (country.technology_researched ? "true" : "false")
            << ",\"primary_culture\":" << country.primary_culture << '}';
    }
    out << "],\"provinces\":[";
    for (size_t n = 0; n < snapshot.provinces.size(); ++n) {
        const auto& province = snapshot.provinces[n];
        if (n) out << ',';
        out << "{\"id\":" << province.id << ",\"owner\":" << province.owner << ",\"dominant_cultures\":";
        Ids(out, province.dominant_cultures); out << ",\"cores\":"; Ids(out, province.cores); out << '}';
    }
    out << "],\"grants\":[";
    for (size_t n = 0; n < grants.size(); ++n) {
        if (n) out << ',';
        out << "{\"province\":" << grants[n].province << ",\"country\":" << grants[n].country << '}';
    }
    out << "]}\n";
    return out.str();
}

} // namespace gpc_runtime_debug
