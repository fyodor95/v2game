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
static void* originalBackendUpdate;
static bool backendUpdateSafePoint;
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
#include "sp_frontend.h"
static vic2_sp_frontend::Ticket spReturnTicket;
static unsigned spReturnStage;
static bool DispatchSpReturn(const char* command) {
    if (strncmp(command, "sp-return-", 10)) return false;
    bool acted = false;
    try {
        if (strcmp(command, "sp-return-state") && GetEnvironmentVariableA("VIC2_TEST_READ_ONLY", nullptr, 0)) {
            Error("E_READ_ONLY"); return true;
        }
        if (!strcmp(command, "sp-return-resign")) {
            if (spReturnStage || advanceTarget || requestedEventId) { Error("E_PENDING"); return true; }
            spReturnTicket = vic2_sp_frontend::RequestResign(baseAddress);
            spReturnStage = 1;
            acted = true;
        } else if (!strcmp(command, "sp-return-confirm")) {
            if (spReturnStage != 1) { Error("E_SP_TRANSITION_STAGE"); return true; }
            vic2_sp_frontend::ConfirmResign(baseAddress, spReturnTicket);
            spReturnStage = 2;
            acted = true;
        } else if (!strcmp(command, "sp-return-exit")) {
            if (spReturnStage != 2) { Error("E_SP_TRANSITION_STAGE"); return true; }
            vic2_sp_frontend::ExitSummary(baseAddress, spReturnTicket, backendUpdateSafePoint);
            spReturnStage = 3;
            acted = true;
        } else if (strcmp(command, "sp-return-state")) { Error("E_ARGUMENT"); return true; }
        if (!spReturnStage) { Error("E_SP_NO_TRANSITION"); return true; }
        const auto current = vic2_sp_frontend::Inspect(baseAddress, spReturnTicket, mainMenu);
        if (current.main_menu) {
            advanceTarget = 0; requestedEventId = 0; requestedProvince = 0; requestedEventTag[0] = 0;
            eventPopup = 0; popupCount = 0; autoEventsEnabled = false; autoEventsSelected = 0;
            consoleManager = 0;
            spReturnTicket = {}; spReturnStage = 0;
        }
        sprintf_s(response, "{\"ok\":true,\"campaign\":%s,\"confirmation\":%s,\"summary\":%s,\"main_menu\":%s,\"resign_queued\":%s,\"pending\":%s}\n",
            current.campaign ? "true" : "false", current.confirmation ? "true" : "false", current.summary ? "true" : "false",
            current.main_menu ? "true" : "false", current.resign_queued ? "true" : "false", spReturnStage ? "true" : "false");
    } catch (const std::exception& error) {
        Log("vic2test SP return: %s", error.what());
        if (acted) { InterlockedExchange(&poisoned, 1); Error("E_SP_FRONTEND_OUTCOME_UNKNOWN"); return true; }
        if (strstr(error.what(), "OUTCOME_UNKNOWN") || strstr(error.what(), "NATIVE_EXCEPTION")) InterlockedExchange(&poisoned, 1);
        char code[64] = {}; unsigned length = 0;
        while (length < sizeof(code) - 1 && (error.what()[length] == '_' ||
               (error.what()[length] >= 'A' && error.what()[length] <= 'Z'))) { code[length] = error.what()[length]; ++length; }
        Error(length > 2 ? code : "E_SP_TRANSITION");
    }
    return true;
}
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
                sprintf_s(response, "{\"ok\":true,\"queued\":%s,\"event_id\":%u,\"option\":%u,\"country\":\"%s\",\"scope\":\"%s\",\"province\":%u}\n",
                    result.queued ? "true" : "false", eventId, option, result.before.tag,
                    result.before.province ? "province" : "country", result.before.province);
            } else {
                strcpy_s(response, "{\"ok\":true,\"events\":[");
                for (size_t i = 0; i < popups.size(); ++i) {
                    char item[192]; sprintf_s(item, "%s{\"event_id\":%u,\"country\":\"%s\",\"options\":%u,\"scope\":\"%s\",\"province\":%u}",
                        i ? "," : "", popups[i].event_id, popups[i].tag, popups[i].option_count,
                        popups[i].province ? "province" : "country", popups[i].province);
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
static bool SpSaveReady(DWORD* playButton = nullptr) {
    using namespace vic2_mp_frontend::detail;
    using vic2_mp_lobby::Field;
    if (!frontend || !mainMenu || Field(frontend) != baseAddress + 0xA14ED0 ||
        Field(mainMenu) != baseAddress + 0xA13DBC || Field(frontend + 4) != mainMenu ||
        Field(baseAddress + 0xE589AC) != 0) throw std::runtime_error("E_NOT_READY");
    if (Flag(frontend + 0x5BC) || !Flag(frontend + 0x5BD)) return false;
    DWORD button = vic2_mp_lobby::Control(baseAddress, Field(frontend + 0x278), 0x34, "play_button");
    DWORD status = 0;
    if (!TryGet(vic2_mp_lobby::Method(baseAddress, button, 0xB0), button, &status) || status > 3)
        throw std::runtime_error("E_NATIVE_LAYOUT");
    if (playButton) *playButton = button;
    // Native getter состояния кнопки:3 — недоступна; тот же контракт, что MP Play.
    return status != 3;
}
static void SpSelectSave(const char* basename) {
    using namespace vic2_mp_frontend::detail;
    using vic2_mp_lobby::Field;
    using vic2_mp_lobby::Control;
    using vic2_mp_lobby::Method;
    if (!frontend || !mainMenu || Field(frontend) != baseAddress + 0xA14ED0 ||
        Field(mainMenu) != baseAddress + 0xA13DBC || Field(frontend + 4) != mainMenu ||
        Field(baseAddress + 0xE589AC) != 0) throw std::runtime_error("E_NOT_READY");
    if (Flag(frontend + 0x5BC) || Flag(frontend + 0x5BD)) throw std::runtime_error("E_ALREADY_SELECTED");
    const unsigned char callback[] = { 0x55,0x8B,0xEC,0x6A,0xFF,0x68 };
    const unsigned char listContract[] = { 0x8B,0x10,0x8B,0xC8,0x8B,0x42,0x2C,0xFF,0xD0 };
    const unsigned char rowContract[] = { 0x8B,0x16,0x8B,0x4A,0x20,0x8B,0x01,0x8B,0x40,0x48 };
    Signature(baseAddress, 0x377DB0, callback);
    Signature(baseAddress, 0x377E8D, listContract);
    Signature(baseAddress, 0x377F75, rowContract);
    DWORD window = Control(baseAddress, Field(frontend + 0x278), 0x6C, "bookmarks");
    DWORD list = Control(baseAddress, window, 0x7C, "bookmarks_list"), header = 0;
    if (!TryGet(Method(baseAddress, list, 0x2C), list, &header)) throw std::runtime_error("E_NATIVE_LAYOUT");
    DWORD checkbox = 0;
    std::set<DWORD> seen;
    for (DWORD node = Field(header); node; node = Field(node + 8)) {
        if (seen.size() >= 8192 || !seen.insert(node).second) throw std::runtime_error("E_NATIVE_LAYOUT");
        DWORD row = Field(node);
        if (Field(row) != baseAddress + 0xA15190 || Text(row + 0x28) != basename) continue;
        if (checkbox) throw std::runtime_error("E_AMBIGUOUS_SAVE");
        checkbox = Control(baseAddress, Field(row + 0x20), 0x48, "save_game");
    }
    if (!checkbox) throw std::runtime_error("E_SAVE_NOT_LISTED");
    // 00777DB0 ставит CClearBookmark в очередь и запускает штатный выбор/загрузку.
    // Прямое присваивание filename/5BC обходило эту часть фронтенда.
    if (!TrySave(baseAddress + 0x377DB0, frontend, checkbox)) {
        InterlockedExchange(&poisoned, 1);
        throw std::runtime_error("E_NATIVE_OUTCOME_UNKNOWN");
    }
}
static void SpLoadResponse(const char* command) {
    try {
        if (!strcmp(command, "frontend-status")) {
            bool ready = SpSaveReady();
            sprintf_s(response, "{\"ok\":true,\"load_pending\":%s,\"save_loaded\":%s}\n",
                *(BYTE*)(frontend + 0x5BC) ? "true" : "false", ready ? "true" : "false");
        } else if (!strcmp(command, "play")) {
            DWORD button = 0;
            if (!SpSaveReady(&button)) { Error("E_NOT_READY"); return; }
            if (!Signal(button)) { Error("E_NATIVE_SIGNAL"); return; }
            strcpy_s(response, "{\"ok\":true,\"queued\":true,\"completed\":false}\n");
        } else {
            const char* basename = request + strlen("load-save");
            while (*basename == ' ') ++basename;
            size_t length = strcspn(basename, "\r\n");
            char filename[260] = {};
            if (length < 4 || length >= sizeof(filename)) { Error("E_ARGUMENT"); return; }
            memcpy(filename, basename, length);
            if (strpbrk(filename, "\\/:") ||
                strcmp(filename + length - 3, ".v2")) { Error("E_ARGUMENT"); return; }
            SpSelectSave(filename);
            strcpy_s(response, "{\"ok\":true,\"queued\":true,\"completed\":false}\n");
        }
    } catch (const std::exception& error) {
        Log("vic2test SP load: %s", error.what());
        // Общие qualified getter helpers используют MP-коды для ошибок layout.
        Error(!strncmp(error.what(), "E_MP_", 5) ? "E_NATIVE_LAYOUT" : error.what());
    }
}
static bool EventCheckReadable(DWORD address, DWORD size) {
    if (!address || address + size < address) return false;
    const DWORD end = address + size;
    while (address < end) {
        MEMORY_BASIC_INFORMATION memory = {};
        if (!VirtualQuery((void*)address, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        DWORD next = (DWORD)memory.BaseAddress + (DWORD)memory.RegionSize;
        if (next <= address) return false;
        address = next;
    }
    return true;
}
struct EventCheckVector { DWORD begin, count; bool contains; };
static bool EventCheckReadVector(DWORD address, DWORD target, DWORD limit, EventCheckVector& result) {
    if (!EventCheckReadable(address, 8)) return false;
    DWORD begin = Read(address), end = Read(address + 4);
    if (end < begin || ((end - begin) & 3) || ((begin | end) & 3) || (end - begin) / 4 > limit ||
        (begin != end && !EventCheckReadable(begin, end - begin))) return false;
    result = { begin, (end - begin) / 4, false };
    for (DWORD index = 0; target && index < result.count; ++index)
        if (Read(begin + index * 4) == target) result.contains = true;
    return true;
}
struct EventCheckTriggerType { DWORD vtable, evaluate; const char* name; bool children; bool core = false; };
static const EventCheckTriggerType eventCheckTypes[] = {
    { 0x9F9994, 0x4B8260, "and", true },
    { 0xA2FB70, 0x4B88A0, "or", true },
    { 0xA2FBA8, 0x4B9240, "not", true },
    { 0xA2FBE0, 0x4B94B0, "context", true },
    { 0xA30A88, 0x4D1750, "any_core", true },
    { 0xA2FE80, 0x4BB4F0, "always", false },
    { 0xA30E78, 0x4C9D80, "controlled_by_rebels", false },
    { 0xA31578, 0x4E21C0, "culture", false },
    { 0xA315E8, 0x4E2A70, "primary_culture", false },
    { 0xA30628, 0x4C3A80, "exists", false },
    { 0xA301C8, 0x4BDE30, "is_core", false },
    { 0xA30C80, 0x4D4000, "tag", false },
    { 0xA31C40, 0x4ECA90, "civilized", false },
    { 0xA31F88, 0x4F1320, "pop_majority_religion", false },
    { 0xE3B3CC, 0x033BC0, "country_religion", false, true }
};
static const BYTE countryReligionEvaluateBytes[] = {
    0x55,0x8B,0xEC,0x6A,0xFE,0x68,0x90,0x77,0x07,0x10,0x68,0xF0,0xF2,0x03,0x10,0x64,
    0xA1,0x00,0x00,0x00,0x00,0x50,0x83,0xEC,0x0C,0x53,0x56,0x57,0xA1,0x40,0xA0,0x07,
    0x10,0x31,0x45,0xF8,0x33,0xC5,0x50,0x8D,0x45,0xF0,0x64,0xA3,0x00,0x00,0x00,0x00,
    0x89,0x65,0xE8,0x8B,0xF1,0x33,0xDB,0x89,0x5D,0xFC,0x8B,0x4D,0x08,0x85,0xC9,0x74,
    0x2E,0x8B,0x49,0x0C,0xE8,0x97,0xFE,0xFF,0xFF,0x8B,0xC8,0x8B,0x46,0x1C,0x85,0xC9,
    0x74,0x1D,0x85,0xC0,0x74,0x19,0x39,0x81,0x68,0x0E,0x00,0x00,0x0F,0x94,0xC3,0xEB,
    0x0B,0xB8,0x01,0x00,0x00,0x00,0xC3,0x8B,0x65,0xE8,0x33,0xDB,0x89,0x5D,0xE4,0xC7,
    0x45,0xFC,0xFE,0xFF,0xFF,0xFF,0x8B,0xC3,0x8B,0x4D,0xF0,0x64,0x89,0x0D,0x00,0x00,
    0x00,0x00,0x59,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00,
};
static const unsigned countryReligionEvaluateRelocations[] = { 6, 11, 29 };
static const BYTE countryReligionLookupBytes[] = {
    0xA1,0x10,0xC4,0x07,0x10,0x8B,0x90,0xE4,0x87,0xE5,0x00,0x85,0xD2,0x74,0x23,0x8B,
    0x42,0x04,0x8B,0x52,0x08,0x85,0xC0,0x74,0x19,0x85,0xD2,0x74,0x15,0x85,0xC9,0x74,
    0x11,0x3B,0xC2,0x77,0x0D,0x2B,0xD0,0xC1,0xFA,0x02,0x3B,0xCA,0x73,0x04,0x8B,0x04,
    0x88,0xC3,0x33,0xC0,0xC3,
};
static const unsigned countryReligionLookupRelocations[] = { 1 };
// Полные тела квалифицированной сборки ядра; HIGHLOW исправляются по ASLR, а не игнорируются.
// Другая компиляция отклоняется до повторного ревью этих двух функций чтения.
template<size_t N, size_t M>
static bool EventCheckCoreBytes(DWORD core, DWORD rva, const BYTE (&bytes)[N], const unsigned (&fixups)[M]) {
    BYTE expected[N]; memcpy(expected, bytes, N);
    for (unsigned at : fixups) {
        if (at > N || N - at < sizeof(DWORD)) return false;
        DWORD address = 0; memcpy(&address, expected + at, sizeof(address));
        if (address < 0x10000000) return false;
        address = core + (address - 0x10000000);
        memcpy(expected + at, &address, sizeof(address));
    }
    return EventCheckReadable(core + rva, N) && !memcmp((void*)(core + rva), expected, N);
}
static bool EventCheckCountryReligion(DWORD core, DWORD vtable) {
    if (!core || !EventCheckReadable(vtable - 4, 17 * sizeof(DWORD)) ||
        !EventCheckReadable(core + 0x7C410, 4) || Read(core + 0x7C410) != baseAddress ||
        Read(vtable + 0x28) != core + 0x34270 ||
        !EventCheckCoreBytes(core, 0x33BC0, countryReligionEvaluateBytes, countryReligionEvaluateRelocations) ||
        !EventCheckCoreBytes(core, 0x33AA0, countryReligionLookupBytes, countryReligionLookupRelocations)) return false;
    // CCore_ProvinceFlags клонирует CPopMajorityReligionTrigger, меняя только Evaluate/Tooltip.
    DWORD original = baseAddress + 0xA31F88;
    if (Read(vtable - 4) != Read(original - 4)) return false;
    for (unsigned slot = 0; slot < 16; ++slot)
        if (slot != 6 && slot != 10 && Read(vtable + slot * 4) != Read(original + slot * 4)) return false;
    return true;
}
static bool EventCheckReligionRegistry(DWORD trigger) {
    // 00969210 может лениво создать manager/default; заполненный справочник исключает обе ветки.
    DWORD count = Read(baseAddress + 0xE58A1C), manager = Read(baseAddress + 0xE5EA24);
    if (!count || count > 1024 || !EventCheckReadable(manager, 0x20)) return false;
    DWORD table = Read(manager + 0x1C), wanted = Read(trigger + 0x1C);
    if (!wanted || !EventCheckReadable(table, count * 4)) return false;
    bool found = false;
    for (DWORD index = 0; index < count; ++index) {
        DWORD religion = Read(table + index * 4);
        if (!religion) return false;
        if (religion == wanted) found = true;
    }
    return found;
}
static const EventCheckTriggerType* EventCheckType(DWORD trigger) {
    if (!EventCheckReadable(trigger, 0x20)) return nullptr;
    DWORD vtable = Read(trigger);
    for (const auto& type : eventCheckTypes) {
        DWORD image = type.core ? (DWORD)GetModuleHandleW(L"creatinecore.dll") : baseAddress;
        if (image && vtable == image + type.vtable && EventCheckReadable(vtable, 0x1C) &&
            Read(vtable + 0x18) == image + type.evaluate) return &type;
    }
    return nullptr;
}
static bool EventCheckQualifyTrigger(DWORD trigger, unsigned depth, unsigned& remaining, bool countryScope = false) {
    if (depth > 16 || !remaining) return false;
    --remaining;
    const EventCheckTriggerType* type = EventCheckType(trigger);
    if (!type) return false;
    if (type->core && (!countryScope || !EventCheckCountryReligion(
        (DWORD)GetModuleHandleW(L"creatinecore.dll"), Read(trigger)))) return false;
    // Квалифицирована только страновая ветка civilized, без запасного виртуального метода POP.
    if (type->vtable == 0xA31C40 && (!countryScope || *(BYTE*)(trigger + 0x1C) > 1)) return false;
    if (type->vtable == 0xA31F88 && !EventCheckReligionRegistry(trigger)) return false;
    bool childCountryScope = countryScope;
    if (type->vtable == 0xA2FBE0) {
        // 00876240: только явная страна или owner (token295), без случайных scopes.
        DWORD context = Read(trigger + 0x1C);
        if (!EventCheckReadable(context, 0x110)) return false;
        DWORD countryIndex = Read(context + 0x10C);
        if (countryIndex) {
            DWORD countries = Read(baseAddress + 0xE587E4);
            if (!countries || countryIndex >= Read(countries) || Read(countries) > 10000) return false;
            DWORD country = Read(Read(countries + 4) + countryIndex * 4);
            if (!EventCheckReadable(country, 0x24) || Read(country + 0x20) != countryIndex ||
                memcmp((void*)(country + 0x1C), (void*)(context + 0x108), 3)) return false;
        } else if (Read(context) != 0x295) return false;
        childCountryScope = true;
    }
    if (!type->children) return true;
    EventCheckVector children = {};
    if (!EventCheckReadVector(trigger + 8, 0, 32, children)) return false;
    // Первая корка использует any_core={always=yes}; другие обходы здесь не квалифицированы.
    if (type->vtable == 0xA30A88 && (children.count != 1 ||
        !EventCheckReadable(Read(children.begin), 4) || Read(Read(children.begin)) != baseAddress + 0xA2FE80)) return false;
    if (type->vtable == 0xA30A88) childCountryScope = false;
    for (DWORD index = 0; index < children.count; ++index)
        if (!EventCheckQualifyTrigger(Read(children.begin + index * 4), depth + 1, remaining, childCountryScope)) return false;
    return true;
}
static void EventCheckScope(unsigned char* scope, unsigned province) {
    // Естественный province scan оставляет country=---/0, province находится в +10.
    ((void* (__fastcall*)(void*, void*, void*))(baseAddress + 0x4A8650))(scope, 0, 0);
    *(DWORD*)(scope + 0x10) = province;
}
static bool EventCheckEvaluate(DWORD trigger, unsigned province) {
    unsigned char scope[0x60] = {};
    EventCheckScope(scope, province);
    return ((char (__fastcall*)(void*, void*, void*))Read(Read(trigger) + 0x18))((void*)trigger, 0, scope) != 0;
}
static void EventCheckProvince(DWORD world) {
    unsigned provinceId = 0, eventId = 0; char extra[2] = {};
    if (sscanf_s(request + strlen("event-check-province"), "%u %u %1s", &provinceId, &eventId,
        extra, (unsigned)sizeof(extra)) != 2 || !provinceId || provinceId > 99999 || !eventId || eventId > 2147483647u) {
        Error("E_ARGUMENT"); return;
    }
    if (!InCampaign(world)) { Error("E_NOT_READY"); return; }
    if (Multiplayer(world)) { Error("E_MULTIPLAYER_READ_ONLY"); return; }
    DWORD idler = Read(world + 0xB24);
    if (!idler || *(BYTE*)(idler + 0x1538) != 1 || advanceTarget) { Error("E_PAUSE_REQUIRED"); return; }
    if (*(BYTE*)(world + 0xB20)) { Error("E_DAY_BUSY"); return; }
    EventCheckVector provinces = {};
    if (!EventCheckReadVector(world + 0xACC, 0, 100000, provinces)) { Error("E_NATIVE_LAYOUT"); return; }
    if (provinceId >= provinces.count) { Error("E_NOT_FOUND"); return; }
    DWORD province = Read(provinces.begin + provinceId * 4);
    if (!EventCheckReadable(province, 0x138)) { Error("E_NATIVE_LAYOUT"); return; }
    if (!Read(world + 0xB60) || Read(province + 0x12C) != Read(world + 0xB60) ||
        memcmp((void*)(province + 0x128), (void*)(world + 0xB5C), 3)) { Error("E_PLAYER_ONLY"); return; }
    DWORD event = EventObject(eventId), registry = Read(baseAddress + 0xF19B80);
    if (!event || !registry) { Error("E_NOT_FOUND"); return; }
    if (!EventCheckReadable(event, 0x1A4) || Read(event) != baseAddress + 0xA2C834) { Error("E_NATIVE_LAYOUT"); return; }
    EventCheckVector all = {}, random = {}, cached = {}, options = {}, modifiers = {}, children = {};
    if (!EventCheckReadVector(registry + 0x20, event, 1000000, all) ||
        !EventCheckReadVector(registry + 0x30, event, 1000000, random) ||
        !EventCheckReadVector(province + 0x14, event, 1000000, cached) ||
        !EventCheckReadVector(event + 0xB0, 0, 100, options) ||
        !EventCheckReadVector(event + 0xCC, 0, 1000, modifiers) ||
        !EventCheckReadVector(event + 0x38, 0, 16, children)) { Error("E_NATIVE_LAYOUT"); return; }
    if (!all.contains || EventKind(eventId) != 1) { Error("E_EVENT_SCOPE"); return; }
    const BYTE scopeSignature[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
    const BYTE mtthSignature[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x8B, 0x45, 0x08, 0x53, 0x8B, 0x58, 0x08 };
    const BYTE andSignature[] = { 0x55, 0x8B, 0xEC, 0x53, 0x56, 0x8B, 0xD9, 0x8B, 0x73, 0x0C };
    unsigned remaining = 256;
    if (memcmp((void*)(baseAddress + 0x4A8650), scopeSignature, sizeof(scopeSignature)) ||
        Read(baseAddress + 0x4A8656) != baseAddress + 0x79F88B ||
        memcmp((void*)(baseAddress + 0x4AE380), mtthSignature, sizeof(mtthSignature)) ||
        memcmp((void*)(baseAddress + 0x4B8260), andSignature, sizeof(andSignature)) ||
        Read(event + 0x30) != baseAddress + 0x9F9994 || modifiers.count || !Read(baseAddress + 0xE58A10) ||
        !EventCheckQualifyTrigger(event + 0x30, 0, remaining)) { Error("E_NATIVE_LAYOUT"); return; }
    // Все отказы по layout выше: далее только детерминированные predicates, без Run/RNG/refresh.
    bool trigger = EventCheckEvaluate(event + 0x30, provinceId);
    unsigned char scope[0x60] = {};
    EventCheckScope(scope, provinceId);
    int chance = ((int (__stdcall*)(void*, void*, int))(baseAddress + 0x4AE380))((void*)(event + 0xC0), scope, 2);
    sprintf_s(response, "{\"ok\":true,\"read_only\":true,\"event_id\":%u,\"province\":%u,\"date_raw\":%lu,"
        "\"tutorial\":%u,\"day_busy\":0,\"triggered_only\":%u,\"fire_only_once\":%u,\"options\":%lu,"
        "\"all\":{\"count\":%lu,\"contains\":true},\"random\":{\"count\":%lu,\"contains\":%s},"
        "\"cached\":{\"count\":%lu,\"contains\":%s},\"mtth\":{\"days_fixed\":%ld,\"modifiers\":0,\"multiplier\":2,\"threshold\":%d},"
        "\"trigger\":%s,\"children\":[", eventId, provinceId, Read(world + 0xB0C),
        (unsigned)*(BYTE*)(world + 0xD11), (unsigned)*(BYTE*)(event + 0x1A0), (unsigned)*(BYTE*)(event + 0x1A3), options.count,
        all.count, random.count, random.contains ? "true" : "false", cached.count, cached.contains ? "true" : "false",
        (LONG)Read(event + 0xC8), chance, trigger ? "true" : "false");
    for (DWORD index = 0; index < children.count; ++index) {
        DWORD child = Read(children.begin + index * 4);
        const EventCheckTriggerType* type = EventCheckType(child);
        char item[160];
        sprintf_s(item, "%s{\"index\":%lu,\"type\":\"%s\",\"vtable_rva\":%lu,\"result\":%s}",
            index ? "," : "", index, type->name, type->vtable, EventCheckEvaluate(child, provinceId) ? "true" : "false");
        strcat_s(response, item);
    }
    strcat_s(response, "]}\n");
}
static void Dispatch() {
    char command[64] = {}, tag[8] = {}, name[512] = {};
    sscanf_s(request, "%63s %7s %511s", command, (unsigned)sizeof(command),
        tag, (unsigned)sizeof(tag), name, (unsigned)sizeof(name));
    DWORD world = Read(baseAddress + 0xE588E8);
    if (DispatchSpReturn(command)) return;
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
    // Диагностика разрешена в read-only SP, но сама отвергает MP и непоставленную на паузу кампанию.
    if (!strcmp(command, "event-check-province")) { EventCheckProvince(world); return; }
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
        SpLoadResponse(command); return;
    }
    if (strcmp(command, "status") == 0) {
        const bool inCampaign = InCampaign(world);
        // В меню текущий idler имеет другой класс и не содержит поля паузы CInGameIdler.
        DWORD idler = inCampaign ? Read(world + 0xB24) : 0;
        char allCount[16] = "null", randomCount[16] = "null";
        if (inCampaign) {
            DWORD registry = Read(baseAddress + 0xF19B80);
            EventCheckVector all = {}, random = {};
            if (registry && EventCheckReadVector(registry + 0x20, 0, 1000000, all) &&
                EventCheckReadVector(registry + 0x30, 0, 1000000, random)) {
                sprintf_s(allCount, "%lu", all.count); sprintf_s(randomCount, "%lu", random.count);
            }
        }
        sprintf_s(response, "{\"ok\":true,\"protocol\":1,\"campaign_loaded\":%s,\"date_raw\":%lu,\"pid\":%lu,\"owner_thread\":%lu,\"console_captured\":%s,\"advance_target\":%lu,\"paused\":%s,\"event_windows\":%u,\"auto_events_selected\":%u,\"multiplayer\":%s,\"all_province_events\":%s,\"random_province_events\":%s}\n",
            inCampaign ? "true" : "false",
            world ? Read(world + 0xB0C) : 0, GetCurrentProcessId(), ownerThread,
            consoleManager ? "true" : "false", advanceTarget,
            idler && *(BYTE*)(idler + 0x1538) ? "true" : "false", ActivePopupCount(), autoEventsSelected,
            Multiplayer(world) ? "true" : "false", allCount, randomCount);
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
static bool PendingBackendExit() {
    return InterlockedCompareExchange(&state, 0, 0) == 1 &&
        !strncmp(request, "sp-return-exit", 14) &&
        (request[14] == 0 || request[14] == '\r' || request[14] == '\n' || request[14] == ' ');
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
    if (ownerThread != GetCurrentThreadId()) return;
    // Exit создаёт CFrontEnd синхронно и заменяет GUI registry. Его нельзя
    // исполнять из Peek/GetMessage до завершения старого BackEnd Update:
    // тот ещё выполнит lookup controls в уже заменённом GUI. Оставляем запрос
    // в IPC-очереди до wrapper-хвоста Update, перед native pending activation.
    if (!backendUpdateSafePoint && PendingBackendExit()) return;
    if (InterlockedCompareExchange(&state, 2, 1) != 1) return;
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
static void __fastcall BackendUpdate(void* object, void*, unsigned tick) {
    ((void (__thiscall*)(void*, unsigned))originalBackendUpdate)(object, tick);
    if (GetCurrentThreadId() != ownerThread || !PendingBackendExit()) return;
    // Native caller немедленно после этого virtual вызова проверяет pending
    // idler (009DF966). Старый Update уже вернулся и больше не читает GUI.
    backendUpdateSafePoint = true;
    Pump();
    backendUpdateSafePoint = false;
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
    // CBackEndIdler::Update: точные vtable slot, пролог и callee-cleanup ABI.
    const BYTE backendUpdateStart[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x6a,0xff};
    const BYTE backendUpdateTail[] = {0x8b,0xe5,0x5d,0xc2,0x04,0x00};
    const BYTE backendCaller[] = {0x8b,0x8f,0x84,0,0,0,0x85,0xc9,0x74,0x09,
        0x8b,0x11,0x8b,0x42,0x04,0x6a,0x01,0xff,0xd0,0x80,0xbf,0x88,0,0,0,0};
    if (Read(baseAddress + 0xA04CFC + 4) != baseAddress + 0x1F8470 ||
        memcmp((void*)(baseAddress + 0x1F8470), backendUpdateStart, sizeof(backendUpdateStart)) ||
        memcmp((void*)(baseAddress + 0x1F8A9F), backendUpdateTail, sizeof(backendUpdateTail)) ||
        memcmp((void*)(baseAddress + 0x5DF953), backendCaller, sizeof(backendCaller))) return 0;
    if (!api->PatchSlot(0xA04CFC, 1, (void*)&BackendUpdate, &originalBackendUpdate)) return 0;
    api->Log("vic2test: SP return Exit поставлен после CBackEndIdler::Update");
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
