#include "native.h"
#include <wincrypt.h>
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#pragma comment(lib, "advapi32.lib")

namespace state_religion_native {
static_assert(sizeof(void*) == 4 && sizeof(GameString) == 24, "Victoria II x86 ABI");

bool Read(DWORD address, void* out, unsigned bytes) {
    if (!address || !out || !bytes || bytes > 16 * 1024 * 1024 || address + bytes < address)
        return false;
    __try { memcpy(out, reinterpret_cast<void*>(address), bytes); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

DWORD Read32(DWORD address) {
    DWORD result = 0;
    if (!Read(address, &result, sizeof(result)))
        throw std::runtime_error("unreadable native field");
    return result;
}

std::vector<DWORD> PointerVector(DWORD address, unsigned maximum) {
    DWORD range[2]{};
    if (!Read(address, range, sizeof(range)) || range[1] < range[0] ||
        (range[1] - range[0]) % 4 || (range[1] - range[0]) / 4 > maximum ||
        (range[0] != range[1] && !range[0]))
        throw std::runtime_error("invalid native vector bounds");
    std::vector<DWORD> result((range[1] - range[0]) / 4);
    if (!result.empty() && !Read(range[0], result.data(), static_cast<unsigned>(result.size() * 4)))
        throw std::runtime_error("unreadable native vector");
    return result;
}

std::string String(DWORD address) {
    GameString value{};
    if (!Read(address, &value, sizeof(value)) || value.length > value.capacity ||
        value.length > 4096 || (value.capacity < 16 && value.length >= 16))
        throw std::runtime_error("invalid native string");
    std::string result(value.length, '\0');
    if (value.length && !Read(value.capacity < 16 ? address : (DWORD)value.data.pointer,
        &result[0], value.length))
        throw std::runtime_error("unreadable native string");
    return result;
}

bool ExecutableMatches() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    bool ok = CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
    BYTE buffer[32768]{};
    DWORD count = 0;
    while (ok) {
        if (!ReadFile(file, buffer, sizeof(buffer), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        if (!CryptHashData(hash, buffer, count, 0)) { ok = false; break; }
    }
    BYTE actual[32]{};
    DWORD size = sizeof(actual);
    // Хеш читается с диска: загруженный образ содержит ASLR и штатные патчи.
    // Строка ниже — каноническая проверенная версия, без неоднозначности записи байтов.
    const char* expectedHex = "62d48c204364dd706584777c2e2b3c7ab3c5f1dd0170872554943575d53d6648";
    ok = ok && CryptGetHashParam(hash, HP_HASHVAL, actual, &size, 0) && size == 32;
    if (ok) {
        static const char hex[] = "0123456789abcdef";
        char rendered[65]{};
        for (unsigned i = 0; i < 32; ++i) {
            rendered[i * 2] = hex[actual[i] >> 4];
            rendered[i * 2 + 1] = hex[actual[i] & 15];
        }
        ok = strcmp(rendered, expectedHex) == 0;
    }
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
}

bool Contracts(DWORD base) {
    const BYTE execute[] = {0x55,0x8b,0xec,0x83,0xec,0x08,0x8b,0x45,
        0x08,0x8b,0x50,0x08,0x8b,0x40,0x0c,0x53};
    const BYTE assign[] = {0x55,0x8b,0xec,0x53,0x8b,0x5d,0x08,0x56,0x8b,0xf1};
    BYTE bytes[16]{};
    const BYTE boolParse[] = {0x55,0x8b,0xec,0x8b,0x45,0x08,0x66,0xc7,0x41,0x1c,0,0};
    const BYTE boolExecute[] = {0x80,0x79,0x20,0};
    if (!Read(base + 0x47c410, bytes, sizeof(boolParse)) || memcmp(bytes, boolParse, sizeof(boolParse)) ||
        !Read(base + 0x49417e, bytes, sizeof(boolExecute)) || memcmp(bytes, boolExecute, sizeof(boolExecute)))
        return false;
    if (!Read(base + 0x494520, bytes, sizeof(execute)) || memcmp(bytes, execute, sizeof(execute)))
        return false;
    if (!Read(base + 0x009350, bytes, sizeof(assign)) || memcmp(bytes, assign, sizeof(assign)))
        return false;
    const BYTE ret8[] = {0xc2,0x08,0x00};
    if (!Read(base + 0x494598, bytes, sizeof(ret8)) || memcmp(bytes, ret8, sizeof(ret8)))
        return false;
    const BYTE render[] = {0x55,0x8b,0xec,0x64,0xa1,0,0,0,0,0x6a,0xff,0x68};
    return Read(base + 0x65ecc0, bytes, sizeof(render)) && !memcmp(bytes, render, sizeof(render)) &&
        Read(base + 0x65edbb, bytes, sizeof(ret8)) && !memcmp(bytes, ret8, sizeof(ret8));
}

DWORD CountryAt(DWORD base, DWORD index, DWORD tag) {
    const DWORD manager = Read32(base + 0xe587e4);
    const DWORD count = Read32(manager);
    if (!index || count > 65536 || index >= count || !tag || tag == 0x2d2d2d)
        throw std::runtime_error("invalid country identity");
    const DWORD country = Read32(Read32(manager + 4) + index * 4);
    if (!country || Read32(country + 0x1c) != tag || Read32(country + 0x20) != index)
        throw std::runtime_error("country identity mismatch");
    return country;
}

DWORD PlayerCountry(DWORD base) {
    const DWORD world = Read32(base + 0xe588e8);
    return CountryAt(base, Read32(world + 0xb60), Read32(world + 0xb5c));
}

std::vector<DWORD> Religions(DWORD base) {
    const DWORD manager = Read32(base + 0xe5ea24);
    auto result = PointerVector(manager + 0x1c, 4096);
    if (result.size() < 2) throw std::runtime_error("religion definitions unavailable");
    std::set<DWORD> unique;
    for (unsigned index = 1; index < result.size(); ++index) {
        if (!result[index] || !unique.insert(result[index]).second ||
            Read32(result[index] + 8) != index)
            throw std::runtime_error("religion definition order mismatch");
    }
    return result;
}

CountryPopulation ReadPopulation(DWORD base, void* context) {
    const DWORD scope = reinterpret_cast<DWORD>(context);
    if (!scope || Read32(scope + 0x10))
        throw std::runtime_error("country scope required");
    // CEventScope хранит пустой CPop по +0x14, даже в области страны:
    // 008A86CE..D1 и008A87AB..E4 берут его из DAT_01258A10.
    const DWORD scopedPop = Read32(scope + 0x14);
    if (scopedPop && scopedPop != Read32(base + 0xe58a10))
        throw std::runtime_error("country scope required (real POP)");
    CountryPopulation result{};
    result.index = Read32(scope + 0x0c);
    const DWORD tag = Read32(scope + 8);
    result.country = CountryAt(base, result.index, tag);
    result.primary = Read32(result.country + 0xe64);
    if (!result.primary) throw std::runtime_error("country primary culture absent");
    result.religions = Religions(base);
    const DWORD current = Read32(result.country + 0xe68);
    const auto currentAt = std::find(result.religions.begin() + 1, result.religions.end(), current);
    result.current = currentAt == result.religions.end() ? 0 :
        static_cast<DWORD>(currentAt - result.religions.begin());
    const DWORD world = Read32(base + 0xe588e8);
    const auto provinces = PointerVector(world + 0xacc, 1000000);
    const auto owned = PointerVector(result.country + 0x9d8, static_cast<unsigned>(provinces.size()));
    const DWORD popManager = Read32(base + 0xf1bb34);
    const auto types = PointerVector(popManager + 0x0c, 4096);
    if (types.empty()) throw std::runtime_error("POP types unavailable");
    std::set<DWORD> visitedProvinces;
    for (DWORD provinceId : owned) {
        if (!provinceId || provinceId >= provinces.size() || !visitedProvinces.insert(provinceId).second)
            throw std::runtime_error("owned province identity invalid");
        const DWORD province = provinces[provinceId];
        if (!province || Read32(province + 0x58) != provinceId ||
            Read32(province + 0x128) != tag || Read32(province + 0x12c) != result.index)
            throw std::runtime_error("owned province identity mismatch");
        const DWORD lists = Read32(province + 0x194);
        if (!lists) continue;
        std::set<DWORD> visitedPops;
        for (unsigned type = 0; type < types.size(); ++type) {
            DWORD pop = Read32(lists + type * 16);
            while (pop) {
                if (visitedPops.size() >= 1000000 || !visitedPops.insert(pop).second)
                    throw std::runtime_error("POP list cycle/duplicate");
                DWORD fields[(0x280 - 0x58) / 4]{};
                if (!Read(pop + 0x58, fields, sizeof(fields)) || fields[0] > INT32_MAX ||
                    fields[3] != province || fields[4] != types[type])
                    throw std::runtime_error("POP size/province/type invalid");
                if (fields[5] == result.primary && fields[0]) {
                    const auto religion = std::find(result.religions.begin() + 1,
                        result.religions.end(), fields[6]);
                    if (religion == result.religions.end())
                        throw std::runtime_error("POP religion outside definitions");
                    result.pops.push_back({result.index, result.primary,
                        static_cast<DWORD>(religion - result.religions.begin()), fields[0]});
                }
                pop = fields[(0x27c - 0x58) / 4];
            }
        }
    }
    return result;
}

bool ApplyReligion(DWORD base, void* context, DWORD religion) {
    // CReligionEffect::Execute читает из эффекта только religion* по +0x20.
    // Штатная функция добавляет историю страны и устанавливает +0xE68.
    DWORD effect[9]{};
    effect[8] = religion;
    using Execute = void(__thiscall*)(void*, void*, int);
    __try {
        reinterpret_cast<Execute>(base + 0x494520)(effect, context, 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void InitString(GameString* string) {
    memset(string, 0, sizeof(*string));
    string->capacity = 15;
}

void SetString(DWORD base, GameString* string, const char* value) {
    using Assign = void*(__thiscall*)(void*, const char*, unsigned);
    reinterpret_cast<Assign>(base + 0x009350)(string, value, static_cast<unsigned>(strlen(value)));
}

void FreeString(DWORD base, GameString* string) {
    using Delete = void(__cdecl*)(void*);
    if (string->capacity >= 16 && string->data.pointer)
        reinterpret_cast<Delete>(base + 0x6ae91b)(string->data.pointer);
    InitString(string);
}

static void RenderText(DWORD renderer, GameString* text, DWORD function) {
    __asm {
        push 0
        push text
        mov esi, renderer
        call function
    }
}

void SetText(DWORD base, void* widget, const char* text) {
    const DWORD address = reinterpret_cast<DWORD>(widget);
    if (!address) throw std::runtime_error("text widget absent");
    auto string = reinterpret_cast<GameString*>(address + 0xdc);
    SetString(base, string, text);
    RenderText(Read32(address + 0xa0), string, base + 0x65ecc0);
}
}
