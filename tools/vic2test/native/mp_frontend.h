#pragma once

#if !defined(_MSC_VER) || !defined(_M_IX86)
#error mp_frontend.h requires MSVC x86
#endif

#include "mp_lobby.h"
#include "mp_control.h"

// Включается после bridge.cpp::FindControl/Signal. Вызывающий код обеспечивает
// поток владельца и единственный pending-запрос. queued — отправка, не ACK сети.
namespace vic2_mp_frontend {

using vic2_mp_lobby::Copy;
using vic2_mp_lobby::Field;
using vic2_mp_lobby::Method;
using vic2_mp_lobby::Control;
using vic2_mp_lobby::Press;

struct State {
    DWORD frontend = 0, main_menu = 0, manager = 0, queue = 0, root = 0;
    bool host = false, loading = false, loaded = false;
};

struct Result {
    bool queued = false;
    bool already = false;
    DWORD country_index = 0, country_tag = 0;
};

namespace detail {

template<size_t N>
static void Signature(DWORD base, DWORD rva, const unsigned char (&bytes)[N]) {
    unsigned char actual[N] = {};
    if (!Copy(base + rva, actual, N) || memcmp(actual, bytes, N))
        throw std::runtime_error("E_MP_SIGNATURE");
}

static bool Flag(DWORD at) {
    unsigned char value = 0;
    if (!Copy(at, &value, 1) || value > 1) throw std::runtime_error("E_MP_LAYOUT");
    return value != 0;
}

static void Equal(DWORD at, DWORD expected) {
    if (Field(at) != expected) throw std::runtime_error("E_MP_LAYOUT");
}

static std::string Text(DWORD at) {
    const DWORD length = Field(at + 0x10), capacity = Field(at + 0x14);
    if (length > 512 || capacity < length || capacity > 1024 * 1024 ||
        (capacity < 16 && capacity != 15)) throw std::runtime_error("E_MP_LAYOUT");
    char bytes[513] = {};
    DWORD chars = capacity >= 16 ? Field(at) : at;
    if (!Copy(chars, bytes, length + 1) || bytes[length] || memchr(bytes, 0, length))
        throw std::runtime_error("E_MP_LAYOUT");
    return std::string(bytes, length);
}

static bool TryGet(DWORD function, DWORD object, DWORD* result) {
    __try { *result = ((DWORD (__thiscall*)(void*))function)((void*)object); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static bool TrySave(DWORD function, DWORD object, DWORD checkbox) {
    __try { ((void (__thiscall*)(void*, DWORD))function)((void*)object, checkbox); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static bool TryCountry(DWORD allocate, DWORD construct, DWORD enqueue, DWORD queue,
                       DWORD tag, DWORD index, DWORD old_tag, DWORD old_index,
                       DWORD player, DWORD expected_vtable) {
    // После входа сюда исключение означает неопределённый исход. Повторная
    // отправка запрещена; изолированную сессию необходимо завершить.
    __try {
        DWORD memory = (DWORD)((void* (__cdecl*)(size_t))allocate)(0x50);
        if (!memory) return false;
        DWORD command = ((DWORD (__stdcall*)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD))construct)
            (memory, tag, index, old_tag, old_index, player);
        if (command != memory || *(DWORD*)command != expected_vtable ||
            *(DWORD*)(command + 0x3c) != tag || *(DWORD*)(command + 0x40) != index ||
            *(DWORD*)(command + 0x44) != old_tag || *(DWORD*)(command + 0x48) != old_index ||
            *(DWORD*)(command + 0x4c) != player) return false;
        ((void (__thiscall*)(void*, DWORD))enqueue)((void*)queue, command);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static DWORD Queue(DWORD base, DWORD manager, bool host) {
    DWORD queue = Field(manager + 0x38);
    Equal(queue, base + (host ? 0xa42728 : 0xa427c8));
    Equal(Field(queue) + 0x18, base + (host ? 0x662ea0 : 0x666b30));
    Method(base, queue, 0x18);
    return queue;
}

static void RequireButton(DWORD base, DWORD button) {
    DWORD status = 0;
    if (!TryGet(Method(base, button, 0xb0), button, &status))
        throw std::runtime_error("E_MP_NATIVE_EXCEPTION");
    // Штатный lobby update использует состояние 3 для недоступной кнопки.
    if (status == 3) throw std::runtime_error("E_MP_NOT_READY");
    if (status > 3) throw std::runtime_error("E_MP_LAYOUT");
}

} // namespace detail

static State Inspect(DWORD base, DWORD frontend, DWORD mainMenu) {
    const unsigned char getter[] = {
        0x8b,0x81,0xe8,0x01,0,0,0x8b,0x80,0x2c,0x01,0,0,0xc3
    };
    detail::Signature(base, 0x357700, getter);
    if (!frontend || !mainMenu || Field(frontend) != base + 0xa14ed0 ||
        Field(mainMenu) != base + 0xa13dbc || Field(frontend + 4) != mainMenu)
        throw std::runtime_error("E_MP_NOT_READY");
    detail::Equal(Field(mainMenu) + 0x44, base + 0x357700);
    // Это тот же getter, который использует row callback 007695F9: embedded
    // interface frontend+3C -> owner frontend+4 -> owner.vt44 -> manager.
    const unsigned char forwarder[] = {0x8b,0x49,0xc8,0x8b,0x01,0x8b,0x50,0x44,0xff,0xe2};
    detail::Signature(base, 0x375920, forwarder);
    detail::Equal(frontend + 0x3c, base + 0xa14f60);
    detail::Equal(base + 0xa14f60 + 0x28, base + 0x375920);
    State state;
    state.frontend = frontend; state.main_menu = mainMenu;
    state.manager = Field(Field(mainMenu + 0x1e8) + 0x12c);
    if (!state.manager) throw std::runtime_error("E_MP_NOT_READY");
    state.host = detail::Flag(state.manager + 0x34);
    DWORD mode = Field(base + 0xe589ac);
    if ((mode != 1 && mode != 2) || state.host != (mode == 1))
        throw std::runtime_error("E_MP_MODE");
    if (detail::Flag(state.manager + 0x234)) throw std::runtime_error("E_MP_BUSY");
    state.queue = detail::Queue(base, state.manager, state.host);
    state.root = Field(frontend + 0x278);
    if (!state.root) throw std::runtime_error("E_MP_NOT_READY");
    state.loading = detail::Flag(frontend + 0x5bc);
    state.loaded = detail::Flag(frontend + 0x5bd);
    return state;
}

static Result SelectSave(DWORD base, DWORD frontend, DWORD mainMenu, const char* basename) {
    if (!basename) throw std::runtime_error("E_MP_ARGUMENT");
    size_t size = strnlen_s(basename, 260);
    if (size < 4 || size >= 260 || strchr(basename, '/') || strchr(basename, '\\') ||
        strchr(basename, ':') || strcmp(basename + size - 3, ".v2"))
        throw std::runtime_error("E_MP_ARGUMENT");
    const State state = Inspect(base, frontend, mainMenu);
    if (!state.host) throw std::runtime_error("E_MP_HOST_REQUIRED");
    if (state.loading) throw std::runtime_error("E_MP_BUSY");
    Result result;
    if (state.loaded && detail::Text(frontend + 0x590) == basename) {
        result.already = true; return result;
    }
    const unsigned char callback[] = {0x55,0x8b,0xec,0x6a,0xff,0x68};
    const unsigned char list_contract[] = {0x8b,0x10,0x8b,0xc8,0x8b,0x42,0x2c,0xff,0xd0};
    const unsigned char row_contract[] = {0x8b,0x16,0x8b,0x4a,0x20,0x8b,0x01,0x8b,0x40,0x48};
    detail::Signature(base, 0x377db0, callback);
    detail::Signature(base, 0x377e8d, list_contract);
    detail::Signature(base, 0x377f75, row_contract);
    DWORD window = Control(base, state.root, 0x6c, "bookmarks");
    DWORD list = Control(base, window, 0x7c, "bookmarks_list"), header = 0;
    if (!detail::TryGet(Method(base, list, 0x2c), list, &header))
        throw std::runtime_error("E_MP_NATIVE_EXCEPTION");
    DWORD checkbox = 0;
    std::set<DWORD> seen;
    for (DWORD node = Field(header); node; node = Field(node + 8)) {
        if (seen.size() >= 8192 || !seen.insert(node).second)
            throw std::runtime_error("E_MP_LAYOUT");
        DWORD row = Field(node);
        // Bookmark item: E14F9C; savegame item: E15190 (ctor 769670).
        if (Field(row) != base + 0xa15190) continue;
        if (detail::Text(row + 0x28) != basename) continue;
        if (checkbox) throw std::runtime_error("E_MP_AMBIGUOUS_SAVE");
        checkbox = Control(base, Field(row + 0x20), 0x48, "save_game");
    }
    if (!checkbox) throw std::runtime_error("E_MP_SAVE_NOT_LISTED");
    // Именно callback: сначала native command (ctor 0072AE30) в сетевую очередь,
    // затем filename/selection и обычный асинхронный load. Локальных записей нет.
    if (!detail::TrySave(base + 0x377db0, frontend, checkbox))
        throw std::runtime_error("E_MP_NATIVE_OUTCOME_UNKNOWN");
    result.queued = true;
    return result;
}

static Result SelectCountry(DWORD base, DWORD frontend, DWORD mainMenu, const char* tag) {
    if (!tag || strnlen_s(tag, 4) != 3) throw std::runtime_error("E_MP_ARGUMENT");
    DWORD packed = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (tag[i] < 'A' || tag[i] > 'Z') throw std::runtime_error("E_MP_ARGUMENT");
        packed |= (DWORD)(unsigned char)tag[i] << (i * 8);
    }
    const State state = Inspect(base, frontend, mainMenu);
    if (!state.loaded || state.loading) throw std::runtime_error("E_MP_NOT_READY");
    const unsigned char constructor[] = {0x55,0x8b,0xec,0x64,0xa1,0,0,0,0};
    const unsigned char returns[] = {0xc2,0x18,0};
    const unsigned char allocate[] = {0x8b,0xff,0x55,0x8b,0xec,0x83,0xec,0x10};
    detail::Signature(base, 0x3297a0, constructor);
    detail::Signature(base, 0x329842, returns);
    detail::Signature(base, 0x6ae9af, allocate);
    DWORD world = Field(base + 0xe588e8), countries = Field(base + 0xe587e4);
    DWORD count = Field(countries), table = Field(countries + 4), index = 0;
    if (!world || !table || count < 2 || count > 65536) throw std::runtime_error("E_MP_LAYOUT");
    for (DWORD i = 1; i < count; ++i) {
        DWORD country = Field(table + i * 4);
        if (!country || Field(country + 0x1c) != packed) continue;
        if (Field(country + 0x20) != i || index) throw std::runtime_error("E_MP_LAYOUT");
        DWORD begin = Field(country + 0x9d8), end = Field(country + 0x9dc);
        if (!begin || end <= begin || ((end - begin) & 3))
            throw std::runtime_error("E_MP_COUNTRY_NOT_PLAYABLE");
        index = i;
    }
    if (!index) throw std::runtime_error("E_MP_COUNTRY_NOT_FOUND");
    DWORD old_tag = Field(world + 0xb5c), old_index = Field(world + 0xb60);
    DWORD player = Field(state.manager + 0x6c);
    Result result; result.country_index = index; result.country_tag = packed;
    if (old_tag == packed && old_index == index) { result.already = true; return result; }
    if (!detail::TryCountry(base + 0x6ae9af, base + 0x3297a0,
                           Method(base, state.queue, 0x18), state.queue,
                           packed, index, old_tag, old_index, player, base + 0xa11f3c))
        throw std::runtime_error("E_MP_NATIVE_OUTCOME_UNKNOWN");
    result.queued = true;
    return result;
}

static Result Play(DWORD base, DWORD frontend, DWORD mainMenu) {
    const State state = Inspect(base, frontend, mainMenu);
    if (!state.host) throw std::runtime_error("E_MP_HOST_REQUIRED");
    if (!state.loaded || state.loading) throw std::runtime_error("E_MP_NOT_READY");
    const unsigned char callback[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x64,0xa1,0,0,0,0};
    const unsigned char subscription[] = {0x8b,0x50,0x54,0x8d,0x48,0x54,0x8d,0x83,0xd8,0x01,0,0};
    detail::Signature(base, 0x372300, callback);
    detail::Signature(base, 0x36abcd, subscription);
    DWORD button = Control(base, state.root, 0x34, "play_button");
    detail::RequireButton(base, button);
    Press(base, button);
    Result result; result.queued = true;
    return result;
}

static bool CanStartWaiting(DWORD base, DWORD idler) {
    const vic2_mp::State state = vic2_mp::Inspect(base);
    if (state.idler != idler || state.mode != 1 || !state.host ||
        state.executing || !state.handler || state.network_ready) return false;
    DWORD lobby = state.handler;
    detail::Equal(lobby + 0xc0, idler);
    detail::Equal(lobby + 0xdc, state.manager);
    detail::Equal(lobby + 0xec, lobby);
    detail::Equal(lobby + 0xf0, base + 0x297f20);
    if (Field(lobby + 0xcc)) return false;
    DWORD button = Control(base, Field(lobby + 0x34), 0x34, "start_button");
    DWORD status = 0;
    if (!detail::TryGet(Method(base, button, 0xb0), button, &status))
        throw std::runtime_error("E_MP_NATIVE_EXCEPTION");
    if (status > 3) throw std::runtime_error("E_MP_LAYOUT");
    // Это только native lookup/getter. Не выполняем GUI Update, Signal и enqueue.
    return status != 3;
}

static Result StartWaiting(DWORD base, DWORD idler) {
    const vic2_mp::State state = vic2_mp::Inspect(base);
    if (state.idler != idler || state.mode != 1 || !state.handler)
        throw std::runtime_error("E_MP_NOT_READY");
    if (!state.host) throw std::runtime_error("E_MP_HOST_REQUIRED");
    if (state.executing) throw std::runtime_error("E_MP_BUSY");
    if (state.network_ready) { Result result; result.already = true; return result; }
    DWORD lobby = state.handler;
    detail::Equal(lobby + 0xc0, idler);
    detail::Equal(lobby + 0xdc, state.manager);
    detail::Equal(lobby + 0xec, lobby);
    detail::Equal(lobby + 0xf0, base + 0x297f20);
    if (Field(lobby + 0xcc)) throw std::runtime_error("E_MP_NOT_READY");
    const unsigned char callback[] = {0x55,0x8b,0xec,0x6a,0xff,0x68};
    const unsigned char enqueue[] = {0x8b,0x8e,0xdc,0,0,0,0x8b,0x49,0x38,0x8b,0x11,0x50,0x8b,0x42,0x18,0xff,0xd0};
    detail::Signature(base, 0x297f20, callback);
    detail::Signature(base, 0x297f65, enqueue);
    DWORD button = Control(base, Field(lobby + 0x34), 0x34, "start_button");
    detail::RequireButton(base, button);
    Press(base, button);
    Result result; result.queued = true;
    return result;
}

} // namespace vic2_mp_frontend
