#include "native.h"
#include "V2ModAPI.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
const V2ModAPI* api = nullptr;
using Update = DWORD(__thiscall*)(void*);
Update original = nullptr;
Update originalFrame = nullptr;
using Localize = int(__cdecl*)(const char*, char*, unsigned);
Localize localize = nullptr;
LONG failureReported = 0;
LONG successReported = 0;

void SetIconVisible(void* icon, bool visible) {
    using namespace state_religion_native;
    // Штатный unciv_overlay в 006D7245/006D728B: +38 скрывает значок
    // цивилизованной страны, +34 показывает его для нецивилизованной.
    using Toggle = void(__thiscall*)(void*);
    const DWORD table = Read32(reinterpret_cast<DWORD>(icon));
    reinterpret_cast<Toggle>(Read32(table + (visible ? 0x34 : 0x38)))(icon);
}

void* Find(void* root, unsigned slot, const char* name) {
    using namespace state_religion_native;
    const DWORD table = Read32(reinterpret_cast<DWORD>(root));
    const DWORD function = Read32(table + slot);
    GameString key{};
    InitString(&key);
    SetString(api->gameBase, &key, name);
    using Lookup = void*(__thiscall*)(void*, const GameString*);
    void* found = reinterpret_cast<Lookup>(function)(root, &key);
    FreeString(api->gameBase, &key);
    return found;
}

void Refresh(void* self) {
    using namespace state_religion_native;
    const DWORD base = api->gameBase;
    const DWORD country = PlayerCountry(base);
    const DWORD religion = Read32(country + 0xe68);
    const auto definitions = Religions(base);
    if (std::find(definitions.begin() + 1, definitions.end(), religion) == definitions.end())
        throw std::runtime_error("country religion outside definitions");
    const DWORD frame = Read32(religion + 0x70);
    if (!frame || frame > 14) throw std::runtime_error("religion icon frame outside atlas");
    const std::string name = String(religion + 0x10);
    const std::string key = "CC_STATE_RELIGION_" + name;
    char label[256]{};
    if (!localize(key.c_str(), label, sizeof(label)))
        throw std::runtime_error("state religion localization missing");
    void* root = reinterpret_cast<void*>(Read32(reinterpret_cast<DWORD>(self) + 0x4c));
    void* icon = Find(root, 0x44, "state_religion");
    void* text = Find(root, 0x3c, "state_religion_name");
    if (!icon || !text) throw std::runtime_error("state religion GUI controls absent");
    if (String(reinterpret_cast<DWORD>(text) + 0xdc) != label) {
        using SetFrame = void(__thiscall*)(void*, int);
        const DWORD iconTable = Read32(reinterpret_cast<DWORD>(icon));
        reinterpret_cast<SetFrame>(Read32(iconTable + 0x48))(icon, static_cast<int>(frame));
        SetText(base, text, label);
    }
    SetIconVisible(icon, true);
    if (InterlockedCompareExchange(&successReported, 1, 0) == 0)
        api->Log("StateReligionUI: first refresh country_index=%lu religion=%s frame=%lu",
            Read32(country + 0x20), name.c_str(), frame);
}

void RefreshCatch(void* self) {
    try { Refresh(self); }
    catch (const std::exception& error) {
        try {
            using namespace state_religion_native;
            void* root = reinterpret_cast<void*>(Read32(reinterpret_cast<DWORD>(self) + 0x4c));
            void* icon = Find(root, 0x44, "state_religion");
            void* text = Find(root, 0x3c, "state_religion_name");
            char unknown[128]{};
            if (icon) SetIconVisible(icon, false);
            if (text && localize("CC_STATE_RELIGION_UNKNOWN", unknown, sizeof(unknown)))
                SetText(api->gameBase, text, unknown);
        } catch (const std::exception&) {}
        if (InterlockedCompareExchange(&failureReported, 1, 0) == 0)
            api->Log("StateReligionUI: refresh REJECT %s", error.what());
    }
}

void RefreshProtected(void* self) {
    // Защита относится только к добавленному UI. Штатный обработчик вызывается
    // без перехвата исключений; его ошибки нельзя выдавать за успешное обновление.
    __try { RefreshCatch(self); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (InterlockedCompareExchange(&failureReported, 1, 0) == 0)
            api->Log("StateReligionUI: refresh REJECT native exception");
    }
}

DWORD __fastcall OnUpdate(void* self, void*) {
    const DWORD result = original(self);
    RefreshProtected(self);
    return result;
}

DWORD __fastcall OnFrame(void* self, void*) {
    const DWORD result = originalFrame(self);
    RefreshProtected(self);
    return result;
}
}

extern "C" __declspec(dllexport) int __cdecl V2Mod_Init(const V2ModAPI* value) {
    if (!value || value->size < sizeof(V2ModAPI) || value->apiVersion != V2MOD_API_VERSION ||
        !value->gameBase || !value->Log || !value->PatchSlot) return 0;
    api = value;
    using namespace state_religion_native;
    try {
        if (!ExecutableMatches() || !Contracts(api->gameBase))
            throw std::runtime_error("executable/native contract mismatch");
        const DWORD slot = api->gameBase + 0xa0e458 + 11 * 4;
        if (Read32(slot) != api->gameBase + 0x2d7180)
            throw std::runtime_error("politics update slot already changed");
        if (Read32(api->gameBase + 0xa0e458 + 6 * 4) != api->gameBase + 0x2d2c80)
            throw std::runtime_error("politics frame slot already changed");
        const BYTE signature[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x64,0xa1,0,0,0,0};
        BYTE actual[sizeof(signature)]{};
        if (!Read(api->gameBase + 0x2d7180, actual, sizeof(actual)) || memcmp(actual, signature, sizeof(actual)))
            throw std::runtime_error("politics update signature mismatch");
        HMODULE core = GetModuleHandleW(L"creatinecore.dll");
        localize = core ? reinterpret_cast<Localize>(GetProcAddress(core, "CCore_LocalizeKey")) : nullptr;
        if (!localize && core)
            localize = reinterpret_cast<Localize>(GetProcAddress(core, "_CCore_LocalizeKey"));
        if (!localize) throw std::runtime_error("core localization export absent");
        void* saved = nullptr;
        if (!api->PatchSlot(0xa0e458, 11, reinterpret_cast<void*>(&OnUpdate), &saved))
            throw std::runtime_error("politics update hook rejected");
        original = reinterpret_cast<Update>(saved);
        saved = nullptr;
        if (!api->PatchSlot(0xa0e458, 6, reinterpret_cast<void*>(&OnFrame), &saved)) {
            void* ignored = nullptr;
            if (!api->PatchSlot(0xa0e458, 11, reinterpret_cast<void*>(original), &ignored)) {
                api->Log("StateReligionUI: PARTIAL install; frame hook and update rollback failed; DLL retained");
                return 1;
            }
            throw std::runtime_error("politics frame hook rejected; update hook restored");
        }
        originalFrame = reinterpret_cast<Update>(saved);
        api->Log("StateReligionUI: installed=1 politics_vtable_rva=A0E458 slots=11,6 atlas_frames=14");
        return 1;
    } catch (const std::exception& error) {
        api->Log("StateReligionUI: NOT_INSTALLED %s", error.what());
        return 0;
    }
}
