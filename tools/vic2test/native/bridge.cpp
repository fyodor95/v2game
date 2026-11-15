#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct V2ModAPI {
    uint32_t size, apiVersion, gameBase;
    const char* modFolder;
    void (__cdecl* Log)(const char*, ...);
    int (__cdecl* PatchSlot)(uint32_t, int, void*, void**);
    int (__cdecl* StealToTrampoline)(uint32_t, unsigned, unsigned char*, int,
                                    const unsigned char*, void*, void**, const char*);
};
static DWORD baseAddress, ownerThread;
static volatile LONG state;
static volatile LONG poisoned;
static char request[1024], response[4096];
static HANDLE completed;
typedef BOOL (WINAPI* PeekMessageFn)(LPMSG, HWND, UINT, UINT, UINT);
static PeekMessageFn originalPeekMessage;
typedef BOOL (WINAPI* GetMessageFn)(LPMSG, HWND, UINT, UINT);
static GetMessageFn originalGetMessage;
static void (__cdecl* Log)(const char*, ...);
static DWORD consoleManager;
static void* originalConsoleTail;
static unsigned char consoleTrampoline[64];
static DWORD advanceTarget;
static bool autoEventsEnabled;
static unsigned autoEventsSelected;
static DWORD popups[256];
static unsigned popupCount;
static DWORD mainMenu, frontend;
static void* originalMenuCtor;
static void* originalFrontendCtor;
static void* originalMenuDtor;
static void* originalFrontendDtor;
static unsigned char frontendTrampolines[4][64];
static DWORD eventPopup;
static void* originalPopupCtor;
static void* originalPopupDelete;
static unsigned char popupTrampoline[64];
static unsigned requestedEventId;
static unsigned requestedProvince;
static char requestedEventTag[4];
static void __stdcall CapturePopup(DWORD object, DWORD event) {
    // Вложенное событие из immediate не должно заменить ожидаемое окно.
    if (!requestedEventId || (event && *(DWORD*)(event + 0x0C) == requestedEventId)) eventPopup = object;
    if (popupCount < 256) popups[popupCount++] = object;
    else if (Log) Log("vic2test: превышен предел отслеживаемых окон событий");
}
static __declspec(naked) void PopupCtor() {
    __asm {
        pushfd
        pushad
        mov eax, [esp + 0x2C]
        push eax
        mov eax, [esp + 0x2C]
        push eax
        call CapturePopup
        popad
        popfd
        jmp dword ptr [originalPopupCtor]
    }
}
static void* __fastcall PopupDelete(void* object, void*, unsigned flags) {
    if ((DWORD)object == eventPopup) { eventPopup = 0; requestedEventId = 0; requestedProvince = 0; }
    for (unsigned i = 0; i < popupCount; ++i) if (popups[i] == (DWORD)object) {
        popups[i] = popups[--popupCount]; break;
    }
    return ((void* (__thiscall*)(void*, unsigned))originalPopupDelete)(object, flags);
}
static void __stdcall CaptureMenu(DWORD object) {
    mainMenu = object; ownerThread = GetCurrentThreadId();
    if (Log) Log("vic2test: главное меню, поток=%lu объект=%08lX", ownerThread, object);
}
static void __stdcall CaptureFrontend(DWORD object) {
    frontend = object; ownerThread = GetCurrentThreadId();
    if (Log) Log("vic2test: выбор кампании, поток=%lu объект=%08lX", ownerThread, object);
}
static __declspec(naked) void MenuCtor() {
    __asm {
        pushfd
        pushad
        mov eax, [esp + 0x28]
        push eax
        call CaptureMenu
        popad
        popfd
        jmp dword ptr [originalMenuCtor]
    }
}
static __declspec(naked) void FrontendCtor() {
    __asm {
        pushfd
        pushad
        mov eax, [esp + 0x28]
        push eax
        call CaptureFrontend
        popad
        popfd
        jmp dword ptr [originalFrontendCtor]
    }
}
static __declspec(naked) void MenuDtor() {
    __asm {
        pushfd
        cmp ecx, mainMenu
        jne done
        mov mainMenu, 0
    done:
        popfd
        jmp dword ptr [originalMenuDtor]
    }
}
static __declspec(naked) void FrontendDtor() {
    __asm {
        pushfd
        cmp ecx, frontend
        jne done
        mov frontend, 0
    done:
        popfd
        jmp dword ptr [originalFrontendDtor]
    }
}
static DWORD Read(DWORD at);
static bool InCampaign(DWORD world) {
    if (!world) return false;
    DWORD idler = Read(world + 0xB24);
    if (!idler) return false;
    DWORD vtable = Read(idler), locator = Read(vtable - 4);
    if (!locator) return false;
    DWORD type = Read(locator + 0x0C);
    return type && strcmp((char*)(type + 8), ".?AVCInGameIdler@@") == 0;
}
static bool Multiplayer(DWORD world) {
    // Тот же текущий флаг, который штатная консоль проверяет перед No cheats in MP.
    return InCampaign(world) && *(BYTE*)(Read(world + 0xB24) + 0x5C) != 0;
}
static bool Pause(DWORD world, bool pause) {
    DWORD idler = Read(world + 0xB24);
    if (!idler || *(BYTE*)(idler + 0x1538) > 1) return false;
    const BYTE signature[] = { 0x55, 0x8b, 0xec, 0x64, 0xa1, 0, 0, 0, 0 };
    if (memcmp((void*)(baseAddress + 0x26A2C0), signature, sizeof(signature))) return false;
    if ((*(BYTE*)(idler + 0x1538) != 0) != pause)
        ((void (__thiscall*)(void*))(baseAddress + 0x26A2C0))((void*)idler);
    return (*(BYTE*)(idler + 0x1538) != 0) == pause;
}
static bool MaximumSpeed(DWORD world) {
    const BYTE signature[] = { 0x8b, 0x81, 0x28, 0x0b, 0, 0, 0x40, 0x83, 0xf8, 4 };
    if (memcmp((void*)(baseAddress + 0x32EE90 + 6), signature, sizeof(signature))) return false;
    DWORD speed = Read(world + 0xB28);
    if (speed > 4) return false;
    for (unsigned i = 0; i < 4 && speed < 4; ++i) {
        ((void (__cdecl*)())(baseAddress + 0x32EE90))();
        DWORD actual = Read(world + 0xB28);
        if (actual != speed + 1) return false;
        speed = actual;
    }
    return speed == 4;
}
static __declspec(naked) void ConsoleTail() {
    __asm {
        mov dword ptr [consoleManager], esi
        jmp dword ptr [originalConsoleTail]
    }
}
static DWORD Read(DWORD at) { return *(DWORD*)at; }
static bool SelectAmbientPopup(DWORD popup) {
    if (!popup || Read(popup) != baseAddress + 0xA2CDB8 ||
        !*(BYTE*)(popup + 0x58) || *(BYTE*)(popup + 0xCC)) return false;
    DWORD node = Read(popup + 0xB4);
    if (!node) return false;
    const BYTE signature[] = { 0x55, 0x8b, 0xec, 0x64, 0xa1, 0, 0, 0, 0 };
    if (memcmp((void*)(baseAddress + 0x4AE150), signature, sizeof(signature))) return false;
    unsigned count = 0;
    DWORD controls[100];
    for (DWORD at = node; at && count < 100; at = Read(at + 8), ++count) {
        DWORD control = Read(at);
        if (!control) return false;
        controls[count] = control;
    }
    if (count == 100) return false;
    for (unsigned i = 0; i < count; ++i) *(BYTE*)(controls[i] + 0x38) = i == 0;
    DWORD event = Read(popup + 0x0C);
    unsigned id = event ? Read(event + 0x0C) : 0;
    ((void (__thiscall*)(void*))(baseAddress + 0x4AE150))((void*)popup);
    ++autoEventsSelected;
    if (Log) Log("vic2test: автоматический выбор первой опции события id=%u, всего=%u", id, autoEventsSelected);
    return true;
}
static unsigned ActivePopupCount() {
    unsigned active = 0;
    for (unsigned i = 0; i < popupCount; ++i) {
        DWORD popup = popups[i];
        if (popup && Read(popup) == baseAddress + 0xA2CDB8 &&
            *(BYTE*)(popup + 0x58) && !*(BYTE*)(popup + 0xCC)) ++active;
    }
    return active;
}
static DWORD FindControl(DWORD object, DWORD slot, const char* text) {
    if (!object || strlen(text) > 255) return 0;
    unsigned char name[0x1C] = {};
    if (strlen(text) > 15) *(const char**)name = text;
    else strcpy_s((char*)name, 16, text);
    *(DWORD*)(name + 0x10) = (DWORD)strlen(text);
    *(DWORD*)(name + 0x14) = strlen(text) > 15 ? (DWORD)strlen(text) : 15;
    return ((DWORD (__thiscall*)(void*, void*))Read(Read(object) + slot))((void*)object, name);
}
static bool Signal(DWORD object) {
    if (!object) return false;
    DWORD signal = object + 0x54;
    DWORD press = baseAddress + 0x5EE510, release = baseAddress + 0x5EE550;
    const BYTE signature[] = { 0x56, 0x8b, 0x70, 4, 0x85, 0xf6, 0x74, 0x10, 0x8b, 0x0e };
    if (memcmp((void*)press, signature, sizeof(signature)) || memcmp((void*)release, signature, sizeof(signature))) return false;
    __asm {
        mov eax, signal
        call press
        mov eax, signal
        call release
    }
    return true;
}
static void Error(const char* code) {
    sprintf_s(response, "{\"ok\":false,\"error\":\"%s\"}\n", code);
}
static const char* GameString(DWORD at) {
    unsigned length = Read(at + 16), capacity = Read(at + 20);
    return length <= 512 && capacity >= length ?
        (capacity > 15 ? (char*)Read(at) : (char*)at) : "";
}
static DWORD Country(const char* tag) {
    if (strlen(tag) != 3) return 0;
    DWORD manager = Read(baseAddress + 0xE587E4);
    if (!manager || Read(manager) > 10000) return 0;
    for (unsigned i = 0; i < Read(manager); ++i) {
        DWORD country = Read(Read(manager + 4) + i * 4);
        if (country && memcmp((void*)(country + 0x1C), tag, 3) == 0) return country;
    }
    return 0;
}
static DWORD Decision(const char* name) {
    DWORD manager = Read(baseAddress + 0xF19730);
    if (!manager) return 0;
    const DWORD offsets[] = { 0x0C, 0x28 };
    for (DWORD offset : offsets) {
        DWORD begin = Read(manager + offset), end = Read(manager + offset + 4);
        if (end < begin || end - begin > 0x100000 || ((end - begin) & 3)) return 0;
        for (DWORD at = begin; at < end; at += 4) {
            DWORD decision = Read(at);
            if (decision && strcmp(GameString(decision + 8), name) == 0) return decision;
        }
    }
    return 0;
}
static DWORD EventObject(unsigned id) {
    // Нативная команда event: ключ {0x27,id}, hash-chain manager из 0041F31E.
    DWORD manager = Read(baseAddress + 0xE58A98);
    if (!manager) return 0;
    DWORD buckets = Read(manager + 4), table = Read(manager + 8);
    if (!buckets || buckets > 1000000 || !table) return 0;
    DWORD at = Read(table + (((0x27u << 16) + id) % buckets) * 4), event = 0;
    for (unsigned n = 0; at && n < 100000; ++n, at = Read(at + 4)) {
        DWORD object = Read(at);
        if (object && Read(object + 8) == 0x27 && Read(object + 0x0C) == id) { event = object; break; }
    }
    return event;
}
#include "mp_control.h"
#include "mp_lobby.h"
#include "mp_frontend.h"
#include "mp_events.h"
#include "mp_save.h"
#include "../../creatinecore/gpc/runtime_status.h"
#include "../../creatinecore/gpc/debug_api.h"
static void JsonEscape(const char* source, char* output, size_t capacity) {
    size_t used = 0;
    for (size_t i = 0; source[i]; ++i) {
        const unsigned char c = (unsigned char)source[i];
        if (used + 7 >= capacity) throw std::runtime_error("E_GPC_RESPONSE_LIMIT");
        if (c == '\\' || c == '"') { output[used++] = '\\'; output[used++] = (char)c; }
        else if (c < 32) { sprintf_s(output + used, capacity - used, "\\u%04X", (unsigned)c); used += 6; }
        else output[used++] = (char)c;
    }
    output[used] = 0;
}
static void GpcDebugResponse(const char* command) {
    if (!InCampaign(Read(baseAddress + 0xE588E8))) { Error("E_NOT_READY"); return; }
    if (GetEnvironmentVariableA("VIC2_TEST_READ_ONLY", nullptr, 0) && !strcmp(command, "culture-cores-run")) {
        Error("E_READ_ONLY"); return;
    }
    V2GpcDebugRequest input = {};
    input.size = sizeof(input); input.version = V2_GPC_DEBUG_VERSION;
    input.operation = !strcmp(command, "culture-cores-preview") ? V2_GPC_DEBUG_PREVIEW :
                      !strcmp(command, "culture-cores-snapshot") ? V2_GPC_DEBUG_SNAPSHOT : V2_GPC_DEBUG_RUN;
    char extra[2] = {};
    if (sscanf_s(request + strlen(command), "%63s %1s", input.name, (unsigned)sizeof(input.name), extra, (unsigned)sizeof(extra)) != 1) {
        Error("E_ARGUMENT"); return;
    }
    HMODULE module = GetModuleHandleA("ccore_great_power_culture_cores.dll");
    if (!module) { Error("E_GPC_NOT_INSTALLED"); return; }
    const auto method = (V2GpcDebugFn)GetProcAddress(module, V2_GPC_DEBUG_EXPORT);
    if (!method) { Error("E_GPC_DEBUG_API"); return; }
    V2GpcDebugResult result = {};
    if (!method(&input, sizeof(input), &result, sizeof(result)) || result.size != sizeof(result) ||
        result.version != V2_GPC_DEBUG_VERSION) {
        if (input.operation == V2_GPC_DEBUG_RUN) InterlockedExchange(&poisoned, 1);
        Error("E_GPC_OUTCOME_UNKNOWN"); return;
    }
    if (result.code == V2_GPC_DEBUG_NATIVE) InterlockedExchange(&poisoned, 1);
    char path[3072] = {}, error[600] = {};
    JsonEscape(result.path, path, sizeof(path)); JsonEscape(result.error, error, sizeof(error));
    sprintf_s(response, "{\"ok\":%s,\"error\":\"%s\",\"code\":%u,\"committed\":%u,\"pairs\":%u,\"added\":%u,\"date_raw\":%u,\"pass\":%u,\"rng\":%u,\"state_digest\":%u,\"core_digest\":%u,\"path\":\"%s\"}\n",
        result.code == 0 ? "true" : "false", error, result.code, result.committed, result.pairs, result.added,
        result.raw_date, result.pass, result.rng, result.state_digest, result.core_digest, path);
}
static bool GpcStatus(V2GpcRuntimeStatus* output) {
    HMODULE module = GetModuleHandleA("ccore_great_power_culture_cores.dll");
    if (!module) return false;
    const auto getter = (V2GpcGetRuntimeStatusFn)GetProcAddress(module, V2_GPC_RUNTIME_STATUS_EXPORT);
    if (!getter || !getter(output, sizeof(*output)) || output->size != sizeof(*output) ||
        output->version != V2_GPC_RUNTIME_STATUS_VERSION)
        throw std::runtime_error("E_GPC_STATUS_API");
    return true;
}
static void GpcStatusResponse() {
    V2GpcRuntimeStatus snapshot = {};
    if (!GpcStatus(&snapshot)) { strcpy_s(response, "{\"ok\":true,\"installed\":false}\n"); return; }
    sprintf_s(response, "{\"ok\":true,\"installed\":true,\"version\":%u,\"enabled\":%u,\"failed\":%u,\"busy\":%u,\"has_schedule\":%u,\"completed_raw_date\":%u,\"callback_raw_date\":%u,\"pass\":%u,\"rng\":%u,\"core_digest\":%u,\"state_digest\":%u}\n",
        snapshot.version, snapshot.enabled, snapshot.failed, snapshot.busy, snapshot.has_schedule,
        snapshot.last_completed_raw_date, snapshot.last_callback_raw_date, snapshot.pass, snapshot.rng,
        snapshot.core_digest, snapshot.state_digest);
}
static bool mpControlPending;
static bool mpExpectedPaused;
static unsigned mpExpectedSpeed;
static bool mpPendingIsPause;
static void MpStateResponse(const vic2_mp::State& current, bool queued = false) {
    if (mpControlPending && (mpPendingIsPause ? current.paused == mpExpectedPaused : current.speed == mpExpectedSpeed))
        mpControlPending = false;
    const bool canStart = vic2_mp_frontend::CanStartWaiting(baseAddress, current.idler);
    sprintf_s(response, "{\"ok\":true,\"multiplayer\":%s,\"host\":%s,\"network_ready\":%s,\"paused\":%s,\"speed\":%u,\"date_raw\":%lu,\"queued\":%s,\"pending\":%s,\"expected_paused\":%s,\"expected_speed\":%u,\"can_start\":%s,\"executing\":%s}\n",
        current.mode == 1 ? "true" : "false", current.host ? "true" : "false", current.network_ready ? "true" : "false",
        current.paused ? "true" : "false", current.speed, Read(current.world + 0xB0C), queued ? "true" : "false",
        mpControlPending ? "true" : "false", mpExpectedPaused ? "true" : "false", mpExpectedSpeed,
        canStart ? "true" : "false", current.executing ? "true" : "false");
}
static bool DispatchMp(const char* command) {
    if (strncmp(command, "mp-", 3)) return false;
    try {
        if (GetEnvironmentVariableA("VIC2_TEST_READ_ONLY", nullptr, 0) &&
            strcmp(command, "mp-state") && strcmp(command, "mp-lobby") && strcmp(command, "mp-front-state") && strcmp(command, "mp-events")) {
            Error("E_READ_ONLY"); return true;
        }
        if (!strcmp(command, "mp-open") || !strcmp(command, "mp-lobby") ||
            !strcmp(command, "mp-host") || !strcmp(command, "mp-join")) {
            vic2_mp_lobby::State current;
            if (!strcmp(command, "mp-open")) current = vic2_mp_lobby::Open(baseAddress, mainMenu);
            else if (!strcmp(command, "mp-lobby")) current = vic2_mp_lobby::Inspect(baseAddress, mainMenu);
            else {
                char player[64] = {}, value[64] = {}, extra[2] = {};
                if (sscanf_s(request + strlen(command), "%63s %63s %1s", player, (unsigned)sizeof(player), value, (unsigned)sizeof(value), extra, (unsigned)sizeof(extra)) != 2) {
                    Error("E_ARGUMENT"); return true;
                }
                current = !strcmp(command, "mp-host") ? vic2_mp_lobby::Host(baseAddress, mainMenu, player, value)
                                                       : vic2_mp_lobby::Join(baseAddress, mainMenu, player, value);
            }
            sprintf_s(response, "{\"ok\":true,\"ready\":%s,\"active\":%s,\"modal_available\":%s,\"screens\":%u,\"queued\":%s}\n",
                current.ready ? "true" : "false", current.active ? "true" : "false", current.modal_available ? "true" : "false", current.screens,
                strcmp(command, "mp-lobby") ? "true" : "false");
            return true;
        }
        if (!strcmp(command, "mp-front-state")) {
            auto current = vic2_mp_frontend::Inspect(baseAddress, frontend, mainMenu);
            DWORD world = Read(baseAddress + 0xE588E8), tag = Read(world + 0xB5C);
            char country[4] = {}; memcpy(country, &tag, 3);
            sprintf_s(response, "{\"ok\":true,\"host\":%s,\"loading\":%s,\"loaded\":%s,\"date_raw\":%lu,\"country\":\"%s\"}\n",
                current.host ? "true" : "false", current.loading ? "true" : "false", current.loaded ? "true" : "false", Read(world + 0xB0C), country);
            return true;
        }
        if (!strcmp(command, "mp-load") || !strcmp(command, "mp-country") || !strcmp(command, "mp-play") || !strcmp(command, "mp-ready")) {
            vic2_mp_frontend::Result result;
            if (!strcmp(command, "mp-play")) result = vic2_mp_frontend::Play(baseAddress, frontend, mainMenu);
            else if (!strcmp(command, "mp-ready")) result = vic2_mp_frontend::StartWaiting(baseAddress, Read(Read(baseAddress + 0xE588E8) + 0xB24));
            else {
                char argument[128] = {}, extra[2] = {};
                if (sscanf_s(request + strlen(command), "%127s %1s", argument, (unsigned)sizeof(argument), extra, (unsigned)sizeof(extra)) != 1) { Error("E_ARGUMENT"); return true; }
                result = !strcmp(command, "mp-load") ? vic2_mp_frontend::SelectSave(baseAddress, frontend, mainMenu, argument)
                                                      : vic2_mp_frontend::SelectCountry(baseAddress, frontend, mainMenu, argument);
            }
            sprintf_s(response, "{\"ok\":true,\"queued\":%s,\"already\":%s,\"country_index\":%lu}\n",
                result.queued ? "true" : "false", result.already ? "true" : "false", result.country_index);
            return true;
        }
        auto current = vic2_mp::Inspect(baseAddress);
        if (!strcmp(command, "mp-state")) { MpStateResponse(current); return true; }
        if (!strcmp(command, "mp-save-menu") || !strcmp(command, "mp-save")) {
            vic2_mp_save::Result result;
            if (!strcmp(command, "mp-save-menu")) result = vic2_mp_save::OpenSaveMenu(baseAddress);
            else {
                char saveName[16] = {}, extra[2] = {};
                if (sscanf_s(request + strlen(command), "%15s %1s", saveName, (unsigned)sizeof(saveName), extra, (unsigned)sizeof(extra)) != 1) {
                    Error("E_ARGUMENT"); return true;
                }
                result = vic2_mp_save::Save(baseAddress, saveName);
            }
            sprintf_s(response, "{\"ok\":true,\"completed\":%s,\"menu_open\":%s,\"already\":%s,\"date_raw\":%lu,\"bytes\":%llu}\n",
                result.completed ? "true" : "false", result.menu_open ? "true" : "false", result.already ? "true" : "false", result.date, result.bytes);
            return true;
        }
        if (!strcmp(command, "mp-events") || !strcmp(command, "mp-option")) {
            const auto popups = vic2_mp_events::List(baseAddress, Read(current.world + 0xB60));
            if (!strcmp(command, "mp-option")) {
                unsigned eventId = 0, option = 0; char extra[2] = {};
                if (sscanf_s(request + strlen(command), "%u %u %1s", &eventId, &option, extra, (unsigned)sizeof(extra)) != 2) {
                    Error("E_ARGUMENT"); return true;
                }
                const vic2_mp_events::PopupInfo* selected = nullptr;
                for (const auto& popup : popups) if (popup.event_id == eventId) {
                    if (selected) { Error("E_MP_EVENT_AMBIGUOUS"); return true; }
                    selected = &popup;
                }
                if (!selected) { Error("E_MP_EVENT_MISSING"); return true; }
                const auto result = vic2_mp_events::Pick(baseAddress, selected->popup, option, Read(current.world + 0xB60));
                sprintf_s(response, "{\"ok\":true,\"queued\":%s,\"event_id\":%u,\"option\":%u}\n",
                    result.queued ? "true" : "false", eventId, option);
            } else {
                strcpy_s(response, "{\"ok\":true,\"events\":[");
                for (size_t i = 0; i < popups.size(); ++i) {
                    char item[160]; sprintf_s(item, "%s{\"event_id\":%u,\"country\":\"%s\",\"options\":%u}",
                        i ? "," : "", popups[i].event_id, popups[i].tag, popups[i].option_count);
                    if (strlen(response) + strlen(item) + 5 >= sizeof(response)) { Error("E_MP_RESPONSE_LIMIT"); return true; }
                    strcat_s(response, item);
                }
                strcat_s(response, "]}\n");
            }
            return true;
        }
        if (mpControlPending && (mpPendingIsPause ? current.paused == mpExpectedPaused : current.speed == mpExpectedSpeed)) mpControlPending = false;
        if (mpControlPending) { Error("E_MP_PENDING"); return true; }
        unsigned target = 0; char extra[2] = {};
        if (sscanf_s(request + strlen(command), "%u %1s", &target, extra, (unsigned)sizeof(extra)) != 1) {
            Error("E_ARGUMENT"); return true;
        }
        vic2_mp::RequestResult result;
        if (!strcmp(command, "mp-pause") && target <= 1) result = vic2_mp::RequestPause(baseAddress, target != 0);
        else if (!strcmp(command, "mp-speed") && target <= 4) result = vic2_mp::RequestSpeedStep(baseAddress, target);
        else { Error("E_ARGUMENT"); return true; }
        mpExpectedPaused = result.expected_paused; mpExpectedSpeed = result.expected_speed; mpControlPending = result.queued;
        mpPendingIsPause = !strcmp(command, "mp-pause");
        MpStateResponse(result.before, result.queued);
    } catch (const std::exception& error) {
        Log("vic2test mp: %s", error.what());
        if (strstr(error.what(), "NATIVE_CALL") || strstr(error.what(), "NATIVE_EXCEPTION") || strstr(error.what(), "OUTCOME_UNKNOWN"))
            InterlockedExchange(&poisoned, 1);
        char code[64] = {};
        unsigned length = 0;
        while (length < sizeof(code) - 1 && (error.what()[length] == '_' ||
               (error.what()[length] >= 'A' && error.what()[length] <= 'Z'))) { code[length] = error.what()[length]; ++length; }
        Error(length > 2 && code[0] == 'E' && code[1] == '_' ? code : "E_MP_CONTROL");
    }
    return true;
}
static int EventKind(unsigned id) {
    DWORD event = EventObject(id), registry = Read(baseAddress + 0xF19B80);
    if (!event || !registry) return -1;
    DWORD begin = Read(registry + 0x20), end = Read(registry + 0x24);
    if (end < begin || end - begin > 4000000 || ((end - begin) & 3)) return -1;
    for (DWORD item = begin; item < end; item += 4) if (Read(item) == event) return 1;
    return 0;
}
static void Dispatch() {
    char command[64] = {}, tag[8] = {}, name[512] = {};
    sscanf_s(request, "%63s %7s %511s", command, (unsigned)sizeof(command),
        tag, (unsigned)sizeof(tag), name, (unsigned)sizeof(name));
    DWORD world = Read(baseAddress + 0xE588E8);
    if (!strcmp(command, "culture-cores-preview") || !strcmp(command, "culture-cores-snapshot") || !strcmp(command, "culture-cores-run")) {
        try { GpcDebugResponse(command); }
        catch (const std::exception& error) { Log("vic2test GPC debug: %s", error.what()); Error("E_GPC_DEBUG_API"); }
        return;
    }
    if (!strcmp(command, "culture-cores-status")) {
        try { GpcStatusResponse(); }
        catch (const std::exception&) { Error("E_GPC_STATUS_API"); }
        return;
    }
    if (DispatchMp(command)) return;
    if ((Multiplayer(world) || GetEnvironmentVariableA("VIC2_TEST_READ_ONLY", nullptr, 0)) &&
        strcmp(command, "status") && strcmp(command, "snapshot") && strcmp(command, "frontend-status")) {
        Error("E_MULTIPLAYER_READ_ONLY"); return;
    }
    if (strcmp(command, "save-native") == 0) {
        if (!InCampaign(world)) { Error("E_NOT_READY"); return; }
        const char* argument = request + strlen("save-native ");
        size_t length = strcspn(argument, "\r\n");
        if (!length || length > 80 || strspn(argument, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != length) {
            Error("E_ARGUMENT"); return;
        }
        char executable[MAX_PATH], filename[MAX_PATH], virtualName[128];
        if (!GetModuleFileNameA(0, executable, sizeof(executable)) || !strstr(executable, "\\.work\\")) { Error("E_PATH"); return; }
        char* slash = strrchr(executable, '\\');
        if (!slash) { Error("E_PATH"); return; }
        *slash = 0; slash = strrchr(executable, '\\');
        if (!slash || strcmp(slash + 1, "game")) { Error("E_PATH"); return; }
        *slash = 0;
        if (sprintf_s(filename, "%s\\userdata\\save games\\%.*s.v2", executable, (int)length, argument) < 0) { Error("E_PATH"); return; }
        sprintf_s(virtualName, "save games/%.*s.v2", (int)length, argument);
        if (GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES) { Error("E_EXISTS"); return; }
        const BYTE signature[] = { 0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68 };
        // Указатель обработчика SEH после push релокируется вместе с образом.
        if (memcmp((void*)(baseAddress + 0x275A80), signature, sizeof(signature)) ||
            Read(baseAddress + 0x275A80 + 6) != baseAddress + 0x79E3AC ||
            *(WORD*)(baseAddress + 0x275A80 + 10) != 0xA164) { Error("E_NATIVE_LAYOUT"); return; }
        unsigned char nativeName[0x1C] = {};
        *(DWORD*)(nativeName + 0x14) = 15;
        // CFile принимает путь VFS, а не абсолютный путь Windows.
        ((void* (__thiscall*)(void*, const char*, unsigned))(baseAddress + 0x9350))(nativeName, virtualName, (unsigned)strlen(virtualName));
        ((void (__thiscall*)(void*, void*))(baseAddress + 0x275A80))(nativeName, (void*)Read(world + 0xB24));
        if (*(DWORD*)(nativeName + 0x14) > 15)
            ((void (__cdecl*)(void*))(baseAddress + 0x6AE91B))((void*)Read((DWORD)nativeName));
        if (GetFileAttributesA(filename) == INVALID_FILE_ATTRIBUTES) { Error("E_SAVE_MISSING"); return; }
        strcpy_s(response, "{\"ok\":true,\"completed\":true}\n"); return;
    }
    if (strcmp(command, "event-select") == 0 || strcmp(command, "event-select-province") == 0) {
        if (!InCampaign(world) || !eventPopup || Read(eventPopup) != baseAddress + 0xA2CDB8) { Error("E_NOT_READY"); return; }
        DWORD event = Read(eventPopup + 0x0C);
        bool provinceSelection = strcmp(command, "event-select-province") == 0;
        if (!event || !requestedEventId || Read(event + 0x0C) != requestedEventId) { Error("E_EVENT_MISMATCH"); return; }
        if (provinceSelection) {
            if (!requestedProvince || strtoul(tag, 0, 10) != requestedProvince ||
                Read(eventPopup + 0x20) != requestedProvince || Read(eventPopup + 0x1C)) {
                Error("E_EVENT_MISMATCH"); return;
            }
        } else if (requestedProvince || memcmp(tag, requestedEventTag, 3) || memcmp((void*)(eventPopup + 0x18), tag, 3)) {
            Error("E_EVENT_MISMATCH"); return;
        }
        char* end = 0; unsigned long choice = strtoul(name, &end, 10);
        if (!end || *end || !name[0] || choice > 99) { Error("E_ARGUMENT"); return; }
        DWORD optionBegin = Read(event + 0xB0), optionEnd = Read(event + 0xB4);
        if (optionEnd < optionBegin || optionEnd - optionBegin > 400 || choice >= (optionEnd - optionBegin) / 4) { Error("E_ARGUMENT"); return; }
        if (!*(BYTE*)(eventPopup + 0x58) || *(BYTE*)(eventPopup + 0xCC)) { Error("E_NOT_READY"); return; }
        DWORD node = Read(eventPopup + 0xB4), selected = 0;
        unsigned count = 0;
        for (; node && count < 100; node = Read(node + 8), ++count) {
            DWORD control = Read(node);
            if (!control) { Error("E_NATIVE_LAYOUT"); return; }
            if (count == choice) selected = control;
        }
        if (!selected || node) { Error("E_NOT_READY"); return; }
        node = Read(eventPopup + 0xB4);
        for (unsigned i = 0; node && i < count; node = Read(node + 8), ++i)
            *(BYTE*)(Read(node) + 0x38) = i == choice;
        const BYTE signature[] = { 0x55, 0x8b, 0xec, 0x64, 0xa1, 0, 0, 0, 0 };
        if (memcmp((void*)(baseAddress + 0x4AE150), signature, sizeof(signature))) { Error("E_NATIVE_LAYOUT"); return; }
        ((void (__thiscall*)(void*))(baseAddress + 0x4AE150))((void*)eventPopup);
        requestedEventId = 0; requestedProvince = 0;
        strcpy_s(response, "{\"ok\":true,\"queued\":true,\"completed\":false}\n"); return;
    }
    if (strcmp(command, "stop") == 0) {
        if (InCampaign(world)) {
            const BYTE signature[] = { 0xc6, 0x81, 0x20, 0x1d, 0, 0, 1, 0xc3 };
            if (memcmp((void*)(baseAddress + 0x24EDB0), signature, sizeof(signature))) { Error("E_NATIVE_LAYOUT"); return; }
            ((void (__thiscall*)(void*))(baseAddress + 0x24EDB0))((void*)Read(world + 0xB24));
        } else PostQuitMessage(0);
        strcpy_s(response, "{\"ok\":true,\"quit_requested\":true}\n"); return;
    }
    if (strcmp(command, "open-single-player") == 0) {
        if (!mainMenu || Read(mainMenu) != baseAddress + 0xA13DBC) { Error("E_NOT_READY"); return; }
        DWORD panel = FindControl(Read(mainMenu + 0x704), 0x6C, "mainmenu_panel");
        DWORD button = FindControl(panel, 0x34, "single_player_button");
        if (!panel || !button) { Error("E_NOT_READY"); return; }
        if (!Signal(button)) { Error("E_NATIVE_SIGNAL"); return; }
        strcpy_s(response, "{\"ok\":true}\n"); return;
    }
    if (strcmp(command, "load-save") == 0 || strcmp(command, "play") == 0 || strcmp(command, "frontend-status") == 0) {
        if (!frontend || Read(frontend) != baseAddress + 0xA14ED0) { Error("E_NOT_READY"); return; }
        if (strcmp(command, "frontend-status") == 0) {
            sprintf_s(response, "{\"ok\":true,\"load_pending\":%s,\"save_loaded\":%s}\n",
                *(BYTE*)(frontend + 0x5BC) ? "true" : "false", *(BYTE*)(frontend + 0x5BD) ? "true" : "false"); return;
        }
        if (strcmp(command, "play") == 0) {
            bool ok = *(BYTE*)(frontend + 0x5BD) && Signal(FindControl(Read(frontend + 0x278), 0x34, "play_button"));
            sprintf_s(response, "{\"ok\":%s}\n", ok ? "true" : "false"); return;
        }
        const char* basename = request + strlen("load-save ");
        char filename[260]; strcpy_s(filename, basename);
        filename[strcspn(filename, "\r\n")] = 0;
        if (!filename[0] || strpbrk(filename, "\\/:")) { Error("E_ARGUMENT"); return; }
        if (*(BYTE*)(frontend + 0x5BC) || *(BYTE*)(frontend + 0x5BD)) { Error("E_ALREADY_SELECTED"); return; }
        ((void* (__thiscall*)(void*, const char*, unsigned))(baseAddress + 0x9350))((void*)(frontend + 0x590), filename, (unsigned)strlen(filename));
        *(BYTE*)(frontend + 0x5BC) = 1;
        strcpy_s(response, "{\"ok\":true,\"queued\":true,\"completed\":false}\n"); return;
    }
    if (strcmp(command, "status") == 0) {
        DWORD idler = world ? Read(world + 0xB24) : 0;
        sprintf_s(response, "{\"ok\":true,\"protocol\":1,\"campaign_loaded\":%s,\"date_raw\":%lu,\"pid\":%lu,\"owner_thread\":%lu,\"console_captured\":%s,\"advance_target\":%lu,\"paused\":%s,\"event_windows\":%u,\"auto_events_selected\":%u,\"multiplayer\":%s}\n",
            InCampaign(world) ? "true" : "false",
            world ? Read(world + 0xB0C) : 0, GetCurrentProcessId(), ownerThread,
            consoleManager ? "true" : "false", advanceTarget,
            idler && *(BYTE*)(idler + 0x1538) ? "true" : "false", ActivePopupCount(), autoEventsSelected,
            Multiplayer(world) ? "true" : "false");
        return;
    }
    if (strcmp(command, "snapshot") == 0) {
        if (!InCampaign(world)) { Error("E_NOT_READY"); return; }
        DWORD country = Country(tag);
        if (!country) { Error("E_NOT_FOUND"); return; }
        sprintf_s(response, "{\"ok\":true,\"treasury_raw\":%lld,\"treasury\":%.8f}\n",
            *(long long*)(country + 0xE78), *(long long*)(country + 0xE78) / 32768.0);
        return;
    }
    if (strcmp(command, "pause") == 0 || strcmp(command, "advance") == 0 || strcmp(command, "advance-auto") == 0) {
        if (!InCampaign(world)) { Error("E_NOT_READY"); return; }
        if (strcmp(command, "pause") == 0) {
            bool ok = Pause(world, true); advanceTarget = 0; autoEventsEnabled = false;
            sprintf_s(response, "{\"ok\":%s}\n", ok ? "true" : "false"); return;
        }
        char* end = 0;
        if (requestedEventId) { Error("E_EVENT_PENDING"); return; }
        unsigned long days = strtoul(tag, &end, 10);
        if (!end || *end || !days || days > 10000 || advanceTarget) { Error("E_ARGUMENT"); return; }
        DWORD oldDate = Read(world + 0xB0C);
        advanceTarget = oldDate + days * 24;
        autoEventsEnabled = strcmp(command, "advance-auto") == 0;
        autoEventsSelected = 0;
        if (!MaximumSpeed(world) || !Pause(world, false)) { advanceTarget = 0; Error("E_NATIVE_LAYOUT"); return; }
        sprintf_s(response, "{\"ok\":true,\"target_date_raw\":%lu,\"completed\":false}\n", advanceTarget);
        return;
    }
    if (strcmp(command, "event-fire") == 0 || strcmp(command, "event-fire-chosen") == 0 ||
        strcmp(command, "event-fire-province") == 0 || strcmp(command, "event-fire-province-chosen") == 0 || strcmp(command, "save") == 0) {
        if (!InCampaign(world) || !consoleManager) { Error("E_NOT_READY"); return; }
        const bool saving = strcmp(command, "save") == 0;
        const bool provinceEvent = strncmp(command, "event-fire-province", 19) == 0;
        unsigned provinceId = 0;
        char actorTag[4] = {};
        if (provinceEvent) {
            char* end = 0; provinceId = (unsigned)strtoul(tag, &end, 10);
            DWORD begin = Read(world + 0xACC), finish = Read(world + 0xAD0);
            if (!end || *end || !provinceId || finish < begin || finish - begin > 400000 ||
                ((finish - begin) & 3) || provinceId >= (finish - begin) / 4) { Error("E_ARGUMENT"); return; }
            DWORD province = Read(begin + provinceId * 4);
            if (!province || Read(province + 0x12C) != Read(world + 0xB60) ||
                memcmp((void*)(province + 0x128), (void*)(world + 0xB5C), 3)) { Error("E_PLAYER_ONLY"); return; }
            memcpy(actorTag, (void*)(province + 0x128), 3);
        }
        if (!saving && requestedEventId) { Error("E_EVENT_PENDING"); return; }
        if (strcmp(command, "event-fire-chosen") == 0 && memcmp((void*)(world + 0xB5C), tag, 3)) {
            Error("E_PLAYER_ONLY"); return;
        }
        char saveName[32] = {};
        if (saving) {
            const char* argument = request + strlen("save ");
            size_t length = strcspn(argument, "\r\n");
            if (!length || length > 15) { Error("E_ARGUMENT"); return; }
            memcpy(saveName, argument, length);
            if (strspn(saveName, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != length) { Error("E_ARGUMENT"); return; }
        }
        if (!saving && ((!provinceEvent && !Country(tag)) || strlen(name) > 15 || !strspn(name, "0123456789") || strspn(name, "0123456789") != strlen(name))) {
            Error("E_NOT_FOUND"); return;
        }
        if (!saving) {
            int kind = EventKind((unsigned)strtoul(name, 0, 10));
            if (kind < 0) { Error("E_NOT_FOUND"); return; }
            if ((kind == 1) != provinceEvent) { Error("E_EVENT_SCOPE"); return; }
        }
        if (provinceEvent) {
            // Тот же native CEvent::run, что в ежедневном province scan
            // (RVA4A80D5). Console event ошибочно оставляет country scope.
            DWORD event = EventObject((unsigned)strtoul(name, 0, 10));
            const BYTE signature[] = { 0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8 };
            BYTE first = *(BYTE*)(baseAddress + 0x4A8120);
            // DEBUG_LOG ядра может уже цеплять этот же проверенный entry.
            if (first != 0xe9 && memcmp((void*)(baseAddress + 0x4A8120), signature, sizeof(signature))) {
                Error("E_NATIVE_LAYOUT"); return;
            }
            unsigned char scope[0x60] = {};
            ((void* (__fastcall*)(void*, void*, void*))(baseAddress + 0x4A8650))(scope, 0, 0);
            *(DWORD*)(scope + 0x10) = provinceId;
            requestedEventId = (unsigned)strtoul(name, 0, 10);
            requestedProvince = provinceId;
            memcpy(requestedEventTag, actorTag, 4); eventPopup = 0;
            ((void (__thiscall*)(void*, void*, void*))(baseAddress + 0x4A8120))
                ((void*)event, (void*)Read(baseAddress + 0xF19B80), scope);
            if (!eventPopup) { requestedEventId = requestedProvince = 0; Error("E_NOT_READY"); return; }
            // Копия больше не должна ссылаться на временный stack scope.
            *(DWORD*)(eventPopup + 0x10 + 0x24) = eventPopup + 0x10;
            strcpy_s(response, "{\"ok\":true,\"queued\":true,\"completed\":false}\n");
            return;
        }
        DWORD begin = Read(consoleManager), end = Read(consoleManager + 4), handler = 0;
        if (end < begin || end - begin > 0x10000 || ((end - begin) & 3)) { Error("E_NATIVE_LAYOUT"); return; }
        for (DWORD at = begin; at < end; at += 4) {
            DWORD entry = Read(at);
            if (entry && Read(entry + 4) && strcmp((const char*)Read(entry + 4), saving ? "save" : "event") == 0) {
                handler = Read(entry + 0x1C); break;
            }
        }
        if (!handler) { Error("E_NOT_FOUND"); return; }
        if (!saving && (provinceEvent || memcmp((void*)(world + 0xB5C), tag, 3) == 0)) {
            requestedEventId = (unsigned)strtoul(name, 0, 10);
            requestedProvince = provinceId;
            memcpy(requestedEventTag, provinceEvent ? actorTag : tag, 3); requestedEventTag[3] = 0;
            eventPopup = 0;
        }
        // Console ABI: vector элементов 0x1C, SSO length/capacity +0x10/+0x14;
        // нативный caller передаёт скрытый result первым аргументом __cdecl.
        unsigned char arguments[2][0x1C] = {}, result[0x20] = {};
        strcpy_s((char*)arguments[0], 16, saving ? saveName : name); strcpy_s((char*)arguments[1], 16, tag);
        *(DWORD*)(arguments[0] + 0x10) = (DWORD)strlen(saving ? saveName : name);
        *(DWORD*)(arguments[1] + 0x10) = (DWORD)strlen(tag);
        *(DWORD*)(arguments[0] + 0x14) = *(DWORD*)(arguments[1] + 0x14) = 15;
        DWORD endArguments = (DWORD)arguments + (saving ? 0x1C : sizeof(arguments));
        DWORD vector[4] = { (DWORD)arguments, endArguments, endArguments, 0 };
        ((unsigned char* (__cdecl*)(unsigned char*, const void*))handler)(result, vector);
        if (!saving && !result[0]) requestedEventId = 0;
        // Сообщение может владеть памятью движка. Освобождаем его штатным free.
        if (*(DWORD*)(result + 4 + 0x14) > 15)
            ((void (__cdecl*)(void*))(baseAddress + 0x6AE91B))((void*)Read((DWORD)result + 4));
        sprintf_s(response, "{\"ok\":%s,\"queued\":%s,\"completed\":false}\n", result[0] ? "true" : "false", result[0] ? "true" : "false");
        return;
    }
    if (strcmp(command, "decision-check") != 0 && strcmp(command, "decision-execute") != 0) {
        Error("E_UNVERIFIED_OPERATION"); return;
    }
    if (!InCampaign(world)) { Error("E_NOT_READY"); return; }
    DWORD country = Country(tag), decision = Decision(name);
    if (!country || !decision) { Error("E_NOT_FOUND"); return; }
    // ABI и layout взяты из DecisionCull закреплённого CreatineCore.
    unsigned char scope[0x60] = {};
    typedef void* (__fastcall* ScopeCtor)(void*, void*, void*);
    ((ScopeCtor)(baseAddress + 0x4A8650))(scope, 0, 0);
    memcpy(scope + 8, (void*)(country + 0x1C), 8);
    typedef char (__fastcall* Evaluate)(void*, void*, void*);
    bool potential = ((Evaluate)Read(Read(decision + 0x24) + 0x18))((void*)(decision + 0x24), 0, scope) != 0;
    bool allow = ((Evaluate)Read(Read(decision + 0x40) + 0x18))((void*)(decision + 0x40), 0, scope) != 0;
    if (strcmp(command, "decision-check") == 0) {
        sprintf_s(response, "{\"ok\":true,\"potential\":%s,\"allow\":%s}\n", potential ? "true" : "false", allow ? "true" : "false");
        return;
    }
    if (!potential || !allow) { Error("E_CONDITIONS"); return; }
    if (memcmp((void*)(world + 0xB5C), tag, 3)) { Error("E_PLAYER_ONLY"); return; }
    // FireDecision ядра вызывает этот же штатный обработчик __stdcall.
    // Возвращение означает постановку команды, а не завершение эффектов.
    unsigned char element[64] = {};
    char label[600];
    sprintf_s(label, "POLITICSVIEW_DECISION%s", name);
    *(char**)(element + 0x14) = label;
    *(unsigned*)(element + 0x28) = sizeof(label) - 1;
    ((void (__stdcall*)(void*))(baseAddress + 0x2DCE10))(element);
    strcpy_s(response, "{\"ok\":true,\"queued\":true,\"completed\":false}\n");
}
static void __cdecl PumpImpl() {
    if (!ownerThread) ownerThread = GetCurrentThreadId();
    if (ownerThread == GetCurrentThreadId() && advanceTarget) {
        __try {
            DWORD world = Read(baseAddress + 0xE588E8);
            if (InCampaign(world)) {
                if (autoEventsEnabled && !requestedEventId) {
                    for (unsigned i = 0; i < popupCount; ++i)
                        if (SelectAmbientPopup(popups[i])) break;
                }
                if (Read(world + 0xB0C) >= advanceTarget && Pause(world, true)) advanceTarget = 0;
                else Pause(world, false);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            advanceTarget = 0; autoEventsEnabled = false;
            InterlockedExchange(&poisoned, 1);
            if (Log) Log("vic2test: ошибка автоматического прогона, сессия повреждена");
        }
    }
    if (ownerThread != GetCurrentThreadId() || InterlockedCompareExchange(&state, 2, 1) != 1) return;
    if (Log) Log("vic2test: выполнение, поток=%lu запрос=%s", GetCurrentThreadId(), request);
    if (InterlockedCompareExchange(&poisoned, 0, 0)) Error("E_SESSION_POISONED");
    else {
        __try { Dispatch(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&poisoned, 1); advanceTarget = 0;
            Error("E_NATIVE_EXCEPTION");
        }
    }
    InterlockedExchange(&state, 3); SetEvent(completed);
    if (Log) Log("vic2test: ответ=%s", response);
}
static volatile LONG pumping;
static void __cdecl Pump() {
    if (InterlockedCompareExchange(&pumping, 1, 0)) return;
    PumpImpl();
    InterlockedExchange(&pumping, 0);
}
static BOOL WINAPI PollMessages(LPMSG message, HWND window, UINT first, UINT last, UINT remove) {
    BOOL result = originalPeekMessage(message, window, first, last, remove);
    DWORD error = GetLastError();
    if (GetCurrentThreadId() == ownerThread) Pump();
    SetLastError(error);
    return result;
}
static BOOL WINAPI GetMessages(LPMSG message, HWND window, UINT first, UINT last) {
    BOOL result = originalGetMessage(message, window, first, last);
    DWORD error = GetLastError();
    if (GetCurrentThreadId() == ownerThread) Pump();
    SetLastError(error);
    return result;
}
static bool InstallMessagePoll() {
    BYTE* image = (BYTE*)baseAddress;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)image;
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(image + dos->e_lfanew);
    DWORD directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!directory) return false;
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(image + directory); d->Name; ++d) {
        if (_stricmp((char*)(image + d->Name), "user32.dll")) continue;
        if (!d->OriginalFirstThunk) return false;
        IMAGE_THUNK_DATA32* names = (IMAGE_THUNK_DATA32*)(image + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA32* slots = (IMAGE_THUNK_DATA32*)(image + d->FirstThunk);
        unsigned installed = 0;
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME* name = (IMAGE_IMPORT_BY_NAME*)(image + names->u1.AddressOfData);
            bool peek = strcmp((char*)name->Name, "PeekMessageA") == 0;
            bool get = strcmp((char*)name->Name, "GetMessageA") == 0;
            if (!peek && !get) continue;
            DWORD protection;
            if (!VirtualProtect(&slots->u1.Function, 4, PAGE_READWRITE, &protection)) return false;
            if (peek) originalPeekMessage = (PeekMessageFn)InterlockedExchange((volatile LONG*)&slots->u1.Function, (LONG)&PollMessages);
            else originalGetMessage = (GetMessageFn)InterlockedExchange((volatile LONG*)&slots->u1.Function, (LONG)&GetMessages);
            VirtualProtect(&slots->u1.Function, 4, protection, &protection);
            ++installed;
        }
        return installed == 2;
    }
    return false;
}
static DWORD WINAPI Serve(void*) {
    char session[80] = {}, path[160];
    DWORD count = GetEnvironmentVariableA("VIC2_TEST_SESSION", session, sizeof(session));
    if (!count || count >= sizeof(session)) return 1;
    for (DWORD i = 0; i < count; ++i) if (!((session[i] >= 'a' && session[i] <= 'z') ||
        (session[i] >= '0' && session[i] <= '9') || session[i] == '-')) return 1;
    sprintf_s(path, "\\\\.\\pipe\\vic2test-%s", session);
    for (;;) {
        HANDLE pipe = CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, sizeof(response), sizeof(request), 0, 0);
        if (pipe == INVALID_HANDLE_VALUE) return 2;
        if (ConnectNamedPipe(pipe, 0) || GetLastError() == ERROR_PIPE_CONNECTED) {
            DWORD got = 0, sent = 0;
            if (ReadFile(pipe, request, sizeof(request) - 1, &got, 0) && got) {
                if (InterlockedCompareExchange(&poisoned, 0, 0)) {
                    request[got] = 0;
                    if (strncmp(request, "stop", 4) == 0 && (request[4] == '\n' || request[4] == '\r' || request[4] == 0)) {
                        const char stopped[] = "{\"ok\":true,\"quit_requested\":true}\n";
                        PostThreadMessageA(ownerThread, WM_QUIT, 0, 0);
                        WriteFile(pipe, stopped, sizeof(stopped) - 1, &sent, 0);
                        DisconnectNamedPipe(pipe); CloseHandle(pipe); return 0;
                    }
                    const char error[] = "{\"ok\":false,\"error\":\"E_SESSION_POISONED\"}\n";
                    WriteFile(pipe, error, sizeof(error) - 1, &sent, 0);
                    DisconnectNamedPipe(pipe); CloseHandle(pipe); continue;
                }
                request[got] = 0; ResetEvent(completed); InterlockedExchange(&state, 1);
                if (Log) Log("vic2test: получен запрос, поток=%lu запрос=%s", ownerThread, request);
                PostThreadMessageA(ownerThread, WM_NULL, 0, 0);
                if (WaitForSingleObject(completed, 180000) != WAIT_OBJECT_0) {
                    InterlockedExchange(&poisoned, 1); advanceTarget = 0;
                    const char timeout[] = "{\"ok\":false,\"error\":\"E_TIMEOUT\"}\n";
                    WriteFile(pipe, timeout, sizeof(timeout) - 1, &sent, 0);
                    DisconnectNamedPipe(pipe); CloseHandle(pipe); return 3;
                }
                if (!WriteFile(pipe, response, (DWORD)strlen(response), &sent, 0)) InterlockedExchange(&poisoned, 1);
                InterlockedExchange(&state, 0);
            }
        }
        FlushFileBuffers(pipe); DisconnectNamedPipe(pipe); CloseHandle(pipe);
    }
}
extern "C" __declspec(dllexport) int __cdecl V2Mod_Init(const V2ModAPI* api) {
    char session[80];
    if (!GetEnvironmentVariableA("VIC2_TEST_SESSION", session, sizeof(session))) return 0;
    if (!api || api->size < sizeof(V2ModAPI) || api->apiVersion != 1) return 0;
    baseAddress = api->gameBase;
    Log = api->Log;
    ownerThread = GetCurrentThreadId();
    DWORD protect;
    completed = CreateEventA(0, TRUE, FALSE, 0);
    if (!completed) return 0;
    if (!InstallMessagePoll()) return 0;
    const BYTE ctor[] = { 0x55, 0x8b, 0xec, 0x6a, 0xff };
    const BYTE dtor[] = { 0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1 };
    if (!api->StealToTrampoline(0x4A8F30, 5, popupTrampoline, 64, ctor, (void*)&PopupCtor, &originalPopupCtor, "vic2test event popup")) return 0;
    if (!api->PatchSlot(0xA2CDB8, 0, (void*)&PopupDelete, &originalPopupDelete)) return 0;
    if (!api->StealToTrampoline(0x354A00, 5, frontendTrampolines[0], 64, ctor, (void*)&MenuCtor, &originalMenuCtor, "vic2test main menu")) return 0;
    if (!api->StealToTrampoline(0x36A2F0, 5, frontendTrampolines[1], 64, ctor, (void*)&FrontendCtor, &originalFrontendCtor, "vic2test frontend")) return 0;
    if (!api->StealToTrampoline(0x354DF0, 6, frontendTrampolines[2], 64, dtor, (void*)&MenuDtor, &originalMenuDtor, "vic2test main menu release")) return 0;
    if (!api->StealToTrampoline(0x36B030, 6, frontendTrampolines[3], 64, dtor, (void*)&FrontendDtor, &originalFrontendDtor, "vic2test frontend release")) return 0;
    if (!VirtualProtect(consoleTrampoline, sizeof(consoleTrampoline), PAGE_EXECUTE_READWRITE, &protect)) return 0;
    const unsigned char consoleExpected[] = { 0x5f, 0x5e, 0x5b, 0x8b, 0xe5 };
    if (!api->StealToTrampoline(0x23A43, sizeof(consoleExpected), consoleTrampoline, sizeof(consoleTrampoline),
        consoleExpected, (void*)&ConsoleTail, &originalConsoleTail, "vic2test console capture")) return 0;
    HANDLE worker = CreateThread(0, 0, Serve, 0, 0, 0);
    if (!worker) return 0;
    CloseHandle(worker);
    api->Log("vic2test: экспериментальный нативный мост подключён");
    return 1;
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
