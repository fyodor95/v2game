#pragma once

#if !defined(_MSC_VER) || !defined(_M_IX86)
#error mp_save.h requires MSVC x86
#endif

#include "mp_control.h"
#include "mp_lobby.h"
#include <cstdio>

// Только тестовый host. Driver до вызова подтверждает общую паузу на обоих
// участниках; адаптер повторно проверяет собственную симуляцию на owner thread.
// После входа в InvokeMenu/InvokeSave любая ошибка имеет внешний код
// E_MP_SAVE_NATIVE_OUTCOME_UNKNOWN: мост обязан poison сессию, повтор запрещён.
namespace vic2_mp_save {

struct Result {
    bool completed = false;
    bool menu_open = false;
    bool already = false;
    DWORD date = 0;
    unsigned long long bytes = 0;
    std::string path;
};

namespace detail {

using vic2_mp_lobby::Field;
using vic2_mp_lobby::Copy;
using vic2_mp_lobby::Control;
using vic2_mp_lobby::Method;

template<size_t N>
static void Signature(DWORD base, DWORD rva, const unsigned char (&bytes)[N]) {
    unsigned char actual[N] = {};
    if (!Copy(base + rva, actual, N) || memcmp(actual, bytes, N))
        throw std::runtime_error("E_MP_SAVE_SIGNATURE");
}

static bool Flag(DWORD at) {
    unsigned char value = 0;
    if (!Copy(at, &value, 1) || value > 1) throw std::runtime_error("E_MP_SAVE_LAYOUT");
    return value != 0;
}

static void Signatures(DWORD base) {
    const unsigned char save[] = {0x55,0x8b,0xec,0x6a,0xff,0x68};
    const unsigned char menu[] = {0x55,0x8b,0xec,0x64,0xa1,0,0,0,0};
    const unsigned char close_gate[] = {
        0x38,0x9e,0x38,0x15,0,0,0x74,0x19,0x38,0x5e,0x5c,0x75,0x14,
        0x38,0x9e,0x04,0x1e,0,0,0x75,0x0c
    };
    const unsigned char open_gate[] = {
        0x8a,0x86,0x38,0x15,0,0,0x88,0x86,0x04,0x1e,0,0,0x3a,0xc3,0x75,0x11,
        0x38,0x5e,0x5c,0x75,0x0c
    };
    const unsigned char string_assign[] = {0x55,0x8b,0xec,0x53,0x8b,0x5d,0x08,0x56,0x8b,0xf1};
    const unsigned char string_destroy[] = {0x56,0x8b,0xf1,0x83,0x7e,0x14,0x10,0x72,0x0b};
    Signature(base, 0x275a80, save);
    if (Field(base + 0x275a86) != base + 0x79e3ac)
        throw std::runtime_error("E_MP_SAVE_SIGNATURE");
    const unsigned char save_tail[] = {0x8b,0xce,0xe8};
    Signature(base, 0x275bbd, save_tail);
    if (base + 0x275bc4 + Field(base + 0x275bc0) != base + 0x26eee0)
        throw std::runtime_error("E_MP_SAVE_SIGNATURE");
    Signature(base, 0x26eee0, menu);
    Signature(base, 0x26f1e7, close_gate);
    Signature(base, 0x26ef25, open_gate);
    Signature(base, 0x9350, string_assign);
    Signature(base, 0x8ad0, string_destroy);
}

static vic2_mp::State Require(DWORD base) {
    Signatures(base);
    const vic2_mp::State state = vic2_mp::Inspect(base);
    vic2_mp::detail::RequireHost(state);
    if (state.executing) throw std::runtime_error("E_MP_BUSY");
    if (!state.paused) throw std::runtime_error("E_MP_SAVE_REQUIRES_PAUSE");
    return state;
}

static bool TryGetBool(DWORD function, DWORD object, bool* result) {
    __try { *result = ((bool (__thiscall*)(void*))function)((void*)object); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static DWORD Menu(DWORD base, DWORD idler) {
    DWORD menu = Field(idler + 0x1d24);
    if (!menu) throw std::runtime_error("E_MP_SAVE_NOT_READY");
    // 66EEE0 иначе имеет отдельную ветку закрытия modal с вызовом pause action.
    DWORD modal = Field(idler + 0x1e00);
    if (!modal || Flag(Field(modal + 0x28) + 0x67))
        throw std::runtime_error("E_MP_SAVE_MODAL_OPEN");
    // Virtual IsBusy (+144) равен [idler+1D80]!=0 у точного CInGameIdler.
    if (Field(idler + 0x1d80)) throw std::runtime_error("E_MP_SAVE_MODAL_OPEN");
    if (Flag(Control(base, menu, 0x6c, "menu_save") + 0x67) ||
        Flag(Control(base, menu, 0x6c, "menu_message_settings") + 0x67))
        throw std::runtime_error("E_MP_SAVE_MODAL_OPEN");
    DWORD extra = Field(idler + 0x718);
    if (extra && Flag(Field(extra + 0x30) + 0x67))
        throw std::runtime_error("E_MP_SAVE_MODAL_OPEN");
    DWORD settings = Field(idler + 0x1d68);
    bool active = false;
    if (settings && !TryGetBool(Method(base, settings, 0x0c), settings, &active))
        throw std::runtime_error("E_MP_SAVE_NATIVE_EXCEPTION");
    if (active) throw std::runtime_error("E_MP_SAVE_MODAL_OPEN");
    return menu;
}

static bool InvokeMenu(DWORD function, DWORD idler) {
    __try { ((void (__thiscall*)(void*))function)((void*)idler); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

struct NativeString { unsigned char data[28]; };

static bool Assign(DWORD function, NativeString* text, const char* chars, unsigned length) {
    __try {
        ((void* (__thiscall*)(void*, const char*, unsigned))function)(text, chars, length);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static bool Destroy(DWORD function, NativeString* text) {
    __try { ((void (__thiscall*)(void*))function)(text); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static bool InvokeSave(DWORD function, NativeString* text, DWORD idler) {
    __try { ((void (__thiscall*)(void*, void*))function)(text, (void*)idler); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static std::string Path(const char* name) {
    if (!name) throw std::runtime_error("E_MP_SAVE_ARGUMENT");
    size_t length = strnlen_s(name, 16);
    if (!length || length > 15 ||
        strspn(name, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != length)
        throw std::runtime_error("E_MP_SAVE_ARGUMENT");
    char executable[MAX_PATH] = {}, filename[MAX_PATH] = {};
    DWORD used = GetModuleFileNameA(0, executable, sizeof(executable));
    if (!used || used >= sizeof(executable) || !strstr(executable, "\\.work\\"))
        throw std::runtime_error("E_MP_SAVE_PATH");
    char* slash = strrchr(executable, '\\');
    if (!slash) throw std::runtime_error("E_MP_SAVE_PATH");
    *slash = 0; slash = strrchr(executable, '\\');
    if (!slash || strcmp(slash + 1, "game")) throw std::runtime_error("E_MP_SAVE_PATH");
    *slash = 0;
    if (sprintf_s(filename, "%s\\userdata\\save games\\%s.v2", executable, name) < 0)
        throw std::runtime_error("E_MP_SAVE_PATH");
    return filename;
}

static void RequireAbsent(const std::string& filename) {
    if (GetFileAttributesA(filename.c_str()) != INVALID_FILE_ATTRIBUTES)
        throw std::runtime_error("E_MP_SAVE_EXISTS");
    DWORD error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND) throw std::runtime_error("E_MP_SAVE_PATH");
}

} // namespace detail

static Result OpenSaveMenu(DWORD base) {
    const vic2_mp::State before = detail::Require(base);
    DWORD menu = detail::Menu(base, before.idler);
    Result result;
    result.date = detail::Field(before.world + 0xb0c);
    if (detail::Flag(menu + 0x67)) {
        result.already = true; result.menu_open = true; return result;
    }
    // Начиная с этого вызова даже E_MP_BUSY из повторного Require означает
    // неизвестный исход уже выполненной мутации, а не разрешение её повторить.
    try {
        if (!detail::InvokeMenu(base + 0x26eee0, before.idler))
            throw std::runtime_error("E_MP_SAVE_MENU_CALL");
        const vic2_mp::State after = detail::Require(base);
        if (after.world != before.world || after.idler != before.idler ||
            detail::Field(after.world + 0xb0c) != result.date ||
            !detail::Flag(detail::Menu(base, after.idler) + 0x67))
            throw std::runtime_error("E_MP_SAVE_STATE_CHANGED");
        result.menu_open = true;
        return result;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("E_MP_SAVE_NATIVE_OUTCOME_UNKNOWN: ") + error.what());
    } catch (...) {
        throw std::runtime_error("E_MP_SAVE_NATIVE_OUTCOME_UNKNOWN");
    }
}

static Result Save(DWORD base, const char* name) {
    Result result;
    result.path = detail::Path(name);
    detail::RequireAbsent(result.path);
    const vic2_mp::State before = detail::Require(base);
    DWORD menu = detail::Menu(base, before.idler);
    if (!detail::Flag(menu + 0x67)) throw std::runtime_error("E_MP_SAVE_MENU_REQUIRED");
    result.date = detail::Field(before.world + 0xb0c);
    char virtual_name[40] = {};
    sprintf_s(virtual_name, "save games/%s.v2", name);
    detail::NativeString text = {};
    *(DWORD*)(text.data + 0x14) = 15;
    if (!detail::Assign(base + 0x9350, &text, virtual_name, (unsigned)strlen(virtual_name)))
        throw std::runtime_error("E_MP_SAVE_NATIVE_OUTCOME_UNKNOWN");
    // Последняя проверка перед входом в штатный writer; Driver владеет каталогом
    // тестовой сессии и не запускает параллельные записи под тем же именем.
    try {
        detail::RequireAbsent(result.path);
    } catch (...) {
        if (!detail::Destroy(base + 0x8ad0, &text))
            throw std::runtime_error("E_MP_SAVE_NATIVE_OUTCOME_UNKNOWN");
        throw;
    }
    // Ни cleanup, ни последующие проверки больше не могут выдавать наружу
    // ошибку предусловия: writer мог изменить файл и локальное меню.
    try {
        bool saved = detail::InvokeSave(base + 0x275a80, &text, before.idler);
        bool destroyed = detail::Destroy(base + 0x8ad0, &text);
        if (!saved || !destroyed) throw std::runtime_error("E_MP_SAVE_WRITER_OR_CLEANUP");
        const vic2_mp::State after = detail::Require(base);
        if (after.world != before.world || after.idler != before.idler ||
            detail::Field(after.world + 0xb0c) != result.date ||
            detail::Flag(detail::Menu(base, after.idler) + 0x67))
            throw std::runtime_error("E_MP_SAVE_STATE_CHANGED");
        WIN32_FILE_ATTRIBUTE_DATA file = {};
        if (!GetFileAttributesExA(result.path.c_str(), GetFileExInfoStandard, &file) ||
            (file.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            throw std::runtime_error("E_MP_SAVE_MISSING");
        result.bytes = ((unsigned long long)file.nFileSizeHigh << 32) | file.nFileSizeLow;
        if (!result.bytes) throw std::runtime_error("E_MP_SAVE_EMPTY");
        result.completed = true;
        return result;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("E_MP_SAVE_NATIVE_OUTCOME_UNKNOWN: ") + error.what());
    } catch (...) {
        throw std::runtime_error("E_MP_SAVE_NATIVE_OUTCOME_UNKNOWN");
    }
}

} // namespace vic2_mp_save
