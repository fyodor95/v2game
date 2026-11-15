#pragma once
// Нативные host/join действия из потока владельца CLI-моста. Нет физических
// событий ввода; все переходы выполняют настоящие подписчики игровых кнопок.
#include <windows.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <set>

namespace vic2_mp_lobby {

static bool Copy(DWORD address, void* output, size_t bytes) {
    if (!address || !output || !bytes || bytes > 1024 * 1024 || address + bytes < address) return false;
    uintptr_t at = address, end = at + bytes;
    while (at < end) {
        MEMORY_BASIC_INFORMATION memory = {};
        if (!VirtualQuery((void*)at, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        DWORD protection = memory.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
            protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
            protection != PAGE_EXECUTE_WRITECOPY) return false;
        uintptr_t next = (uintptr_t)memory.BaseAddress + memory.RegionSize;
        if (next <= at) return false;
        at = next;
    }
    __try { memcpy(output, (void*)address, bytes); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static DWORD Field(DWORD address) {
    DWORD result = 0;
    if (!Copy(address, &result, sizeof(result))) throw std::runtime_error("E_NATIVE_LAYOUT");
    return result;
}

static void NativeCode(DWORD base, DWORD function) {
    DWORD pe = Field(base + 0x3c);
    if (pe > 0x100000 || Field(base + pe) != IMAGE_NT_SIGNATURE)
        throw std::runtime_error("E_NATIVE_LAYOUT");
    DWORD size = Field(base + pe + 0x50);
    MEMORY_BASIC_INFORMATION memory = {};
    if (size < 0x1000 || function < base || function - base >= size ||
        !VirtualQuery((void*)function, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD))) throw std::runtime_error("E_NATIVE_LAYOUT");
    DWORD protection = memory.Protect & 0xff;
    if (protection != PAGE_EXECUTE && protection != PAGE_EXECUTE_READ &&
        protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)
        throw std::runtime_error("E_NATIVE_LAYOUT");
}

static DWORD Method(DWORD base, DWORD object, DWORD slot) {
    DWORD function = Field(Field(object) + slot);
    NativeCode(base, function);
    return function;
}

static DWORD Control(DWORD base, DWORD parent, DWORD slot, const char* name) {
    if (!parent) throw std::runtime_error("E_NOT_READY");
    Method(base, parent, slot);
    DWORD result = ::FindControl(parent, slot, name);
    if (!result) throw std::runtime_error("E_NOT_READY");
    Field(result);
    return result;
}

static void Press(DWORD base, DWORD button) {
    if (!button) throw std::runtime_error("E_NOT_READY");
    DWORD signal = Field(button + 0x54);
    if (!signal) throw std::runtime_error("E_NATIVE_LAYOUT");
    NativeCode(base, Field(signal));
    if (!::Signal(button)) throw std::runtime_error("E_NATIVE_SIGNAL");
}

struct State {
    DWORD view = 0;
    DWORD panel = 0;
    DWORD modal = 0;
    DWORD holder = 0;
    unsigned screens = 0;
    bool ready = false;
    bool active = false;
    bool modal_available = false;
};

static State Inspect(DWORD base, DWORD mainMenu) {
    if (!mainMenu || Field(mainMenu) != base + 0xa13dbc)
        throw std::runtime_error("E_NOT_READY");
    State result;
    // 007599E0 работает с вторичным this=root+0x88. Его +6C4/+6C8
    // соответствуют +74C/+750 у захваченного первичного CFrontEnd.
    DWORD begin = Field(mainMenu + 0x74c), end = Field(mainMenu + 0x750);
    if (end < begin || ((end - begin) & 3) || (end - begin) / 4 > 64 || (begin != end && !begin))
        throw std::runtime_error("E_NATIVE_LAYOUT");
    result.screens = (end - begin) / 4;
    const DWORD active_index = Field(mainMenu + 0x75c);
    std::set<DWORD> views;
    for (DWORD at = begin; at < end; at += 4) {
        // Вектор содержит непосредственно view*, не holder-объекты.
        DWORD view = Field(at);
        if (!view || Field(view) != base + 0xa14844 || !views.insert(view).second) continue;
        if (Field(view + 4) != mainMenu) throw std::runtime_error("E_NATIVE_LAYOUT");
        if (result.view && result.view != view) throw std::runtime_error("E_AMBIGUOUS_MP_VIEW");
        result.view = view;
        result.active = (at - begin) / 4 == active_index;
        result.holder = 0; // Поле оставлено для совместимости диагностики.
        result.panel = Field(view + 0x368);
        result.modal = Field(view + 0x36c);
    }
    result.ready = result.view && result.panel;
    result.modal_available = result.modal != 0;
    return result;
}

static State Open(DWORD base, DWORD mainMenu) {
    State state = Inspect(base, mainMenu);
    if (state.ready && state.active) return state;
    DWORD root = Field(mainMenu + 0x704);
    DWORD panel = Control(base, root, 0x6c, "mainmenu_panel");
    Press(base, Control(base, panel, 0x34, "multi_player_button"));
    state = Inspect(base, mainMenu);
    return state;
}

struct Text {
    union { char inline_text[16]; const char* pointer; } value;
    DWORD length;
    DWORD capacity;
};
static_assert(sizeof(Text) == 24, "Victoria II x86 string ABI required");

static bool SetNativeText(DWORD function, DWORD component, const Text* input) {
    __try {
        ((void (__thiscall*)(void*, const Text*))function)((void*)component, input);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static bool GetNativeText(DWORD function, DWORD component, Text* output, DWORD* returned) {
    __try {
        *returned = (DWORD)((Text* (__thiscall*)(void*, Text*))function)((void*)component, output);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static bool FreeNativeText(DWORD function, Text* text) {
    __try { ((void (__thiscall*)(void*))function)(text); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static std::string ASCII(const char* text) {
    if (!text) throw std::runtime_error("E_ARGUMENT");
    size_t length = strnlen_s(text, 64);
    if (!length || length > 63) throw std::runtime_error("E_ARGUMENT");
    bool visible = false;
    for (size_t n = 0; n < length; ++n) {
        unsigned char c = (unsigned char)text[n];
        if (c < 32 || c > 126) throw std::runtime_error("E_ARGUMENT");
        visible = visible || c != ' ';
    }
    if (!visible) throw std::runtime_error("E_ARGUMENT");
    return std::string(text, length);
}

static void IPv4(const std::string& address) {
    unsigned parts = 0, value = 0, digits = 0;
    for (size_t n = 0; n <= address.size(); ++n) {
        char c = n == address.size() ? '.' : address[n];
        if (c == '.') {
            if (!digits || value > 255) throw std::runtime_error("E_ARGUMENT");
            ++parts; value = 0; digits = 0;
        } else {
            if (c < '0' || c > '9' || ++digits > 3) throw std::runtime_error("E_ARGUMENT");
            value = value * 10 + c - '0';
        }
    }
    if (parts != 4) throw std::runtime_error("E_ARGUMENT");
}

static void SetAndVerifyText(DWORD base, DWORD box, const std::string& source) {
    DWORD component = box + 0x54;
    DWORD setter = Method(base, component, 0x40), getter = Method(base, component, 0x4c);
    // Квалифицированный деструктор старой игровой строки, не free() CRT плагина.
    NativeCode(base, base + 0x8ad0);
    Text input = {};
    input.length = (DWORD)source.size();
    input.capacity = input.length < 16 ? 15 : input.length;
    if (input.length < 16) memcpy(input.value.inline_text, source.c_str(), input.length + 1);
    else input.value.pointer = source.c_str();
    if (!SetNativeText(setter, component, &input)) throw std::runtime_error("E_NATIVE_EXCEPTION");
    // input заимствует буфер source и не передаётся деструктору игры.
    Text output = {};
    output.capacity = 15;
    DWORD returned = 0;
    if (!GetNativeText(getter, component, &output, &returned))
        throw std::runtime_error("E_NATIVE_EXCEPTION");
    if (output.length > output.capacity || output.capacity > 1024 * 1024 ||
        (output.capacity < 16 && (output.capacity != 15 || output.length >= 16)))
        // При неизвестном ABI нельзя освобождать потенциально чужой указатель.
        throw std::runtime_error("E_NATIVE_LAYOUT");
    bool valid = returned == (DWORD)&output && output.length <= output.capacity &&
                 output.length == source.size() && (output.capacity >= 16 || output.length < 16);
    char actual[64] = {};
    DWORD chars = output.capacity < 16 ? (DWORD)output.value.inline_text : (DWORD)output.value.pointer;
    valid = valid && Copy(chars, actual, output.length + 1) && actual[output.length] == 0 &&
            !memcmp(actual, source.data(), source.size());
    // Getter создаёт собственный выходной объект. Он уничтожается даже при
    // несовпадении текста; заимствованный input при этом остаётся нетронутым.
    if (!FreeNativeText(base + 0x8ad0, &output)) throw std::runtime_error("E_NATIVE_EXCEPTION");
    if (!valid) throw std::runtime_error("E_TEXT_VERIFY");
}

static State Submit(DWORD base, DWORD mainMenu, const std::string& player,
                    const std::string& target, const char* mode) {
    State state = Inspect(base, mainMenu);
    if (!state.ready || !state.active) throw std::runtime_error("E_NOT_READY");
    SetAndVerifyText(base, Control(base, state.panel, 0x40, "playername"), player);
    // Обе подтверждающие ветви сами читают playername. У host — 00767E38,
    // у join — 00767378; отдельной записи глобальных настроек здесь нет.
    Press(base, Control(base, state.panel, 0x34, mode));
    state = Inspect(base, mainMenu);
    if (!state.ready || !state.modal_available) throw std::runtime_error("E_NOT_READY");
    SetAndVerifyText(base, Control(base, state.modal, 0x40, "edit"), target);
    Press(base, Control(base, state.modal, 0x34, "ok_host_button"));
    // queued не означает созданную сессию; её состояние проверяет вызывающий
    // контроллер после возвращения. Указатели после Signal не разыменовываем.
    return state;
}

static State Host(DWORD base, DWORD mainMenu, const char* playerName, const char* gameName) {
    const auto player = ASCII(playerName), game = ASCII(gameName);
    return Submit(base, mainMenu, player, game, "host");
}

static State Join(DWORD base, DWORD mainMenu, const char* playerName, const char* address) {
    const auto player = ASCII(playerName), ip = ASCII(address);
    IPv4(ip);
    return Submit(base, mainMenu, player, ip, "join_direct");
}

} // namespace vic2_mp_lobby
