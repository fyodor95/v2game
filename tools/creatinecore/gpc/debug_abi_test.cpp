// Независимая проверка публичного C ABI готовой DLL до инициализации движка.
// V2Mod_Init не вызывается: imageBase и simulationThread остаются нулевыми.
// 82 сценария: status/buffers 10, debug buffers 10, NOACCESS 5,
// аргументы 38, session/owner guards 18 и заключительный status 1.
// Это не квалификация успешных preview/run, игрового RNG или native effects.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "debug_api.h"
#include "runtime_status.h"

#if !defined(_M_IX86)
#error Этот ABI-тест должен собираться MSVC Win32.
#endif

static_assert(sizeof(void*) == 4, "x86 caller");
static_assert(std::is_standard_layout<V2GpcDebugRequest>::value &&
              std::is_trivially_copyable<V2GpcDebugRequest>::value, "request POD");
static_assert(std::is_standard_layout<V2GpcDebugResult>::value &&
              std::is_trivially_copyable<V2GpcDebugResult>::value, "result POD");
static_assert(std::is_standard_layout<V2GpcRuntimeStatus>::value &&
              std::is_trivially_copyable<V2GpcRuntimeStatus>::value, "status POD");
static_assert(sizeof(V2GpcDebugRequest) == 76 && alignof(V2GpcDebugRequest) == 4, "request v1");
static_assert(sizeof(V2GpcDebugResult) == 652 && alignof(V2GpcDebugResult) == 4, "result v1");
static_assert(sizeof(V2GpcRuntimeStatus) == 48 && alignof(V2GpcRuntimeStatus) == 4, "status v1");
static_assert(offsetof(V2GpcDebugRequest, name) == 12, "request name offset");
static_assert(offsetof(V2GpcDebugResult, path) == 44 &&
              offsetof(V2GpcDebugResult, error) == 556, "result strings offsets");
static_assert(offsetof(V2GpcRuntimeStatus, rng) == 36 &&
              offsetof(V2GpcRuntimeStatus, state_digest) == 44, "status state offsets");

namespace {

V2GpcDebugFn debugCall = nullptr;
V2GpcGetRuntimeStatusFn statusCall = nullptr;
unsigned checks = 0, cases = 0;
std::string currentCase = "initialization";
constexpr unsigned char kCanary = 0xa5;

void Check(bool condition, const char* explanation) {
    ++checks;
    if (!condition) throw std::runtime_error(explanation);
}

bool Filled(const void* memory, size_t size, unsigned char byte) {
    const auto* first = static_cast<const unsigned char*>(memory);
    for (size_t i = 0; i < size; ++i) if (first[i] != byte) return false;
    return true;
}

template<class T> struct Framed {
    unsigned char before[16];
    T value;
    unsigned char after[16];
    Framed() { std::memset(this, kCanary, sizeof(*this)); }
    void CheckEdges() const {
        Check(Filled(before, sizeof(before), kCanary), "Запись перед caller buffer");
        Check(Filled(after, sizeof(after), kCanary), "Запись за пределами DTO");
    }
};

V2GpcRuntimeStatus EmptyStatus() {
    V2GpcRuntimeStatus result = {};
    result.size = 48;
    result.version = 1;
    return result;
}

void CheckStatus() {
    Framed<V2GpcRuntimeStatus> output;
    Check(statusCall(&output.value, 48) == 1, "Status getter вернул отказ");
    output.CheckEdges();
    const auto expected = EmptyStatus();
    Check(!std::memcmp(&output.value, &expected, sizeof(expected)),
          "До init status должен оставаться нулевым, включая RNG/pass/failed/busy");
}

template<class Fn> void Case(const std::string& name, Fn test) {
    currentCase = name;
    test();
    CheckStatus();
    ++cases;
}

V2GpcDebugRequest Request(uint32_t operation = V2_GPC_DEBUG_PREVIEW) {
    V2GpcDebugRequest request = {};
    request.size = 76;
    request.version = 1;
    request.operation = operation;
    std::memcpy(request.name, "unit", 5);
    return request;
}

void CheckError(const V2GpcDebugResult& actual, uint32_t code, const char* error) {
    Check(actual.size == 652 && actual.version == 1, "Размер/версия result v1");
    Check(actual.code == code, "Неверный численный код отказа");
    Check(actual.committed == 0 && actual.pairs == 0 && actual.added == 0 &&
          actual.raw_date == 0 && actual.pass == 0 && actual.rng == 0 &&
          actual.state_digest == 0 && actual.core_digest == 0,
          "Отказ до Guard не должен публиковать commit или данные мира");
    Check(std::memchr(actual.path, '\0', sizeof(actual.path)) == actual.path,
          "До init path должен быть пустым и NUL-завершённым");
    const auto* terminator = static_cast<const char*>(
        std::memchr(actual.error, '\0', sizeof(actual.error)));
    Check(terminator && static_cast<size_t>(terminator - actual.error) == std::strlen(error),
          "Неверная длина/NUL error");
    Check(!std::memcmp(actual.error, error, std::strlen(error)), "Неверная ASCII-строка error");
    V2GpcDebugResult expected = {};
    expected.size = 652;
    expected.version = 1;
    expected.code = code;
    std::memcpy(expected.error, error, std::strlen(error) + 1);
    Check(!std::memcmp(&actual, &expected, sizeof(expected)),
          "Result содержит лишние байты или неинициализированный хвост строки");
}

void ExpectError(const V2GpcDebugRequest& request, uint32_t code, const char* error,
                 uint32_t requestSize = 76, uint32_t outputSize = 652) {
    Framed<V2GpcDebugRequest> input;
    input.value = request;
    Framed<V2GpcDebugResult> output;
    Check(debugCall(&input.value, requestSize, &output.value, outputSize) == 1,
          "Корректный caller buffer должен получить result даже при отказе");
    input.CheckEdges();
    output.CheckEdges();
    Check(!std::memcmp(&input.value, &request, sizeof(request)), "Request изменён экспортом");
    CheckError(output.value, code, error);
}

void StatusBuffers() {
    for (const uint32_t size : {0u, 48u, UINT32_MAX})
        Case("status null size=" + std::to_string(size), [=] {
            Check(statusCall(nullptr, size) == 0, "Null status buffer должен вернуть 0");
        });
    for (const uint32_t size : {0u, 1u, 47u})
        Case("status short size=" + std::to_string(size), [=] {
            Framed<V2GpcRuntimeStatus> output;
            Check(statusCall(&output.value, size) == 0, "Малый status buffer должен вернуть 0");
            Check(Filled(&output, sizeof(output), kCanary), "Малый status buffer изменён");
        });
    for (const uint32_t size : {48u, 64u, UINT32_MAX})
        Case("status accepted size=" + std::to_string(size), [=] {
            Framed<V2GpcRuntimeStatus> output;
            Check(statusCall(&output.value, size) == 1, "Большой status buffer должен принимать v1");
            output.CheckEdges();
            const auto expected = EmptyStatus();
            Check(!std::memcmp(&output.value, &expected, sizeof(expected)), "Неверный начальный status");
        });
}

void DebugBuffers() {
    const auto request = Request();
    Case("debug null request", [&] {
        Framed<V2GpcDebugResult> output;
        Check(debugCall(nullptr, 76, &output.value, 652) == 0, "Null request должен вернуть 0");
        Check(Filled(&output, sizeof(output), kCanary), "Выход изменён при null request");
    });
    Case("debug null output", [&] {
        Check(debugCall(&request, 76, nullptr, 652) == 0, "Null output должен вернуть 0");
    });
    Case("debug both null", [&] {
        Check(debugCall(nullptr, 76, nullptr, 652) == 0, "Null buffers должны вернуть 0");
    });
    for (const uint32_t size : {0u, 1u, 75u})
        Case("debug short request size=" + std::to_string(size), [&request, size] {
            Framed<V2GpcDebugResult> output;
            Check(debugCall(&request, size, &output.value, 652) == 0, "Малый request должен вернуть 0");
            Check(Filled(&output, sizeof(output), kCanary), "Выход изменён при малом request");
        });
    for (const uint32_t size : {0u, 1u, 651u})
        Case("debug short output size=" + std::to_string(size), [&request, size] {
            Framed<V2GpcDebugResult> output;
            Check(debugCall(&request, 76, &output.value, size) == 0, "Малый output должен вернуть 0");
            Check(Filled(&output, sizeof(output), kCanary), "Малый output изменён");
        });
    Case("debug larger caller buffers", [&] {
        // Внешние размеры могут быть больше DTO; внутренний request.size строго 76.
        ExpectError(request, 2, "E_GPC_DEBUG_TEST_SESSION", 92, 668);
    });
}

class NoAccessPage {
public:
    void* memory = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    NoAccessPage() { Check(memory != nullptr, "VirtualAlloc PAGE_NOACCESS не выполнен"); }
    ~NoAccessPage() { if (memory) VirtualFree(memory, 0, MEM_RELEASE); }
    NoAccessPage(const NoAccessPage&) = delete;
    NoAccessPage& operator=(const NoAccessPage&) = delete;
};

void InaccessibleBuffers() {
    NoAccessPage page;
    const auto* badRequest = static_cast<const V2GpcDebugRequest*>(page.memory);
    auto* badOutput = static_cast<V2GpcDebugResult*>(page.memory);
    const auto request = Request();
    // Harness не перехватывает SEH: незащищённое исключение из DLL провалит процесс.
    Case("NOACCESS request", [&] {
        Framed<V2GpcDebugResult> output;
        Check(debugCall(badRequest, 76, &output.value, 652) == 0, "SEH чтения должен вернуть 0");
        Check(Filled(&output, sizeof(output), kCanary), "Выход изменён после отказа чтения request");
    });
    Case("NOACCESS output", [&] {
        Check(debugCall(&request, 76, badOutput, 652) == 0, "SEH записи должен вернуть 0");
    });
    Case("NOACCESS both", [&] {
        Check(debugCall(badRequest, 76, badOutput, 652) == 0, "SEH обоих buffers должен вернуть 0");
    });
    Case("NOACCESS status", [&] {
        Check(statusCall(static_cast<V2GpcRuntimeStatus*>(page.memory), 48) == 0,
              "SEH status должен вернуть 0");
    });
    Case("NOACCESS undersized request", [&] {
        Framed<V2GpcDebugResult> output;
        Check(debugCall(badRequest, 75, &output.value, 652) == 0, "Размер проверяется до чтения request");
        Check(Filled(&output, sizeof(output), kCanary), "Выход изменён при NOACCESS/short request");
    });
}

void Arguments(const std::string& prefix) {
    for (const uint32_t size : {0u, 75u, 77u})
        Case(prefix + "wrong request.size=" + std::to_string(size), [=] {
            auto request = Request(); request.size = size;
            ExpectError(request, 1, "E_GPC_DEBUG_ARGUMENT");
        });
    for (const uint32_t version : {0u, 2u, UINT32_MAX})
        Case(prefix + "wrong version=" + std::to_string(version), [=] {
            auto request = Request(); request.version = version;
            ExpectError(request, 1, "E_GPC_DEBUG_ARGUMENT");
        });
    for (const uint32_t operation : {0u, 4u, UINT32_MAX})
        Case(prefix + "wrong operation=" + std::to_string(operation), [=] {
            ExpectError(Request(operation), 1, "E_GPC_DEBUG_ARGUMENT");
        });
    Case(prefix + "empty name", [] {
        auto request = Request(); request.name[0] = 0;
        ExpectError(request, 1, "E_GPC_DEBUG_NAME");
    });
    Case(prefix + "49-char name", [] {
        auto request = Request(); std::memset(request.name, 'A', 49); request.name[49] = 0;
        ExpectError(request, 1, "E_GPC_DEBUG_NAME");
    });
    Case(prefix + "non-NUL name", [] {
        auto request = Request(); std::memset(request.name, 'A', sizeof(request.name));
        ExpectError(request, 1, "E_GPC_DEBUG_NAME");
    });
    const unsigned char badBytes[] = {'/', '\\', '.', ' ', ':', '\n', 0x80};
    for (const unsigned char bad : badBytes)
        Case(prefix + "name byte=" + std::to_string(bad), [=] {
            auto request = Request(); request.name[1] = static_cast<char>(bad);
            ExpectError(request, 1, "E_GPC_DEBUG_NAME");
        });
}

void Guards(const std::string& prefix, const char* error) {
    const char* names[] = {"x", "Mixed_case-09", "0123456789abcdef0123456789abcdef0123456789abcdef"};
    for (const uint32_t operation : {1u, 2u, 3u})
        for (const char* name : names)
            Case(prefix + "operation=" + std::to_string(operation) + " name=" + name, [=] {
                auto request = Request(operation);
                std::memset(request.name, 0, sizeof(request.name));
                std::memcpy(request.name, name, std::strlen(name));
                ExpectError(request, 2, error);
            });
}

template<class Fn> Fn Export(HMODULE module, const char* name) {
    const FARPROC symbol = GetProcAddress(module, name);
    Check(symbol != nullptr, name);
    static_assert(sizeof(Fn) == sizeof(symbol), "x86 function-pointer ABI");
    Fn function = nullptr;
    std::memcpy(&function, &symbol, sizeof(function));
    return function;
}

} // namespace

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 2) {
        std::fprintf(stderr, "Использование: debug_abi_test.exe <готовая-GPC-DLL>\n");
        return 2;
    }
    HMODULE module = nullptr;
    try {
        // Меняется окружение только этого тестового процесса, никогда shell/игры.
        Check(SetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr) != 0, "Не удалось удалить session");
        module = LoadLibraryA(argv[1]);
        Check(module != nullptr, "LoadLibraryA не загрузил указанный x86-артефакт");
        statusCall = Export<V2GpcGetRuntimeStatusFn>(module, V2_GPC_RUNTIME_STATUS_EXPORT);
        debugCall = Export<V2GpcDebugFn>(module, V2_GPC_DEBUG_EXPORT);
        Case("initial uninitialized status", [] { CheckStatus(); });
        StatusBuffers();
        DebugBuffers();
        InaccessibleBuffers();
        Arguments("no-session ");
        Guards("no-session ", "E_GPC_DEBUG_TEST_SESSION");
        Check(SetEnvironmentVariableA("VIC2_TEST_SESSION", "unit") != 0, "Не удалось установить session");
        Arguments("no-owner ");
        Guards("no-owner ", "E_GPC_DEBUG_OWNER_THREAD");
        Check(SetEnvironmentVariableA("VIC2_TEST_SESSION", nullptr) != 0, "Не удалось очистить session");
        Case("final unchanged status", [] { CheckStatus(); });
        Check(cases == 82, "Изменилось ожидаемое число ABI-сценариев");
        FreeLibrary(module);
        std::printf("GPC debug ABI x86: %u сценария, %u проверок; успешно. Движок не инициализировался.\n", cases, checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GPC debug ABI: ОШИБКА [%s]: %s (%u завершённых сценариев)\n",
                     currentCase.c_str(), error.what(), cases);
        if (module) FreeLibrary(module);
        return 1;
    }
}
