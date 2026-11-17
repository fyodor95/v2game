// Изолированные проверки настоящего адаптера; игру и сетевую очередь не вызывают.
// MSVC x86: cl /nologo /std:c++17 /EHsc /W4 /utf-8 mp_events_test.cpp
#define NOMINMAX
#include <Windows.h>
#include "mp_events.h"
#include <array>
#include <iostream>
#include <string>

namespace {
using vic2_mp_events::detail::Address;

Address At(const void* value) {
    return static_cast<Address>(reinterpret_cast<std::uintptr_t>(value));
}

template<class T, std::size_t N>
void Put(std::array<unsigned char, N>& object, unsigned offset, T value) {
    if (offset + sizeof(value) > N) throw std::runtime_error("test bounds");
    std::memcpy(object.data() + offset, &value, sizeof(value));
}

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<class F>
void Reject(F action, const char* code) {
    try { action(); }
    catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(code) != std::string::npos, "wrong rejection");
        return;
    }
    throw std::runtime_error("missing rejection");
}

struct Fixture {
    static constexpr unsigned province_id = 546;
    std::array<unsigned char, 0xB00> world{};
    std::array<unsigned char, 0x100> idler{};
    std::array<unsigned char, 0xD0> popup{};
    std::array<unsigned char, 0x200> event{};
    std::array<unsigned char, 0x200> province{};
    std::array<std::array<unsigned char, 0x100>, 2> buttons{};
    std::array<std::array<unsigned char, 16>, 2> nodes{};
    std::array<std::array<unsigned char, 16>, 2> observers{};
    std::array<Address, 2> options{1, 2};
    std::array<Address, province_id + 1> provinces{};
    vic2_mp_events::detail::Context context;

    Fixture() {
        context.base = 0x400000;
        context.country = 23;
        context.tag = 0x00555250;  // PRU.
        context.state.world = At(world.data());
        context.state.idler = At(idler.data());
        context.state.mode = 1;
        context.state.network_ready = true;
        context.state.paused = true;
        Put(world, 0xACC, At(provinces.data()));
        Put(world, 0xAD0, At(provinces.data() + provinces.size()));
        provinces[province_id] = At(province.data());
        Put(province, 0x58, province_id);
        Put(province, 0x128, context.tag);
        Put(province, 0x12C, context.country);
        Put(popup, 0, context.base + 0xA2CDB8);
        Put(popup, 8, context.state.idler);
        Put(popup, 0x0C, At(event.data()));
        Put(popup, 0x10, context.base + 0xA2C904);
        Put(popup, 0x18, context.tag);
        Put(popup, 0x1C, context.country);
        Put(popup, 0x34, At(popup.data()) + 0x10);
        Put(popup, 0x58, static_cast<unsigned char>(1));
        Put(popup, 0x5C, context.base + 0xA2CDD4);
        Put(popup, 0x60, At(popup.data()));
        Put(popup, 0x64, context.base + 0x4AE150);
        Put(popup, 0xB4, At(nodes[0].data()));
        Put(popup, 0xB8, At(nodes[1].data()));
        Put(popup, 0xBC, 2u);
        Put(event, 8, 0x27u);
        Put(event, 0x0C, 24001u);
        Put(event, 0xB0, At(options.data()));
        Put(event, 0xB4, At(options.data() + options.size()));
        for (unsigned i = 0; i < 2; ++i) {
            Put(nodes[i], 0, At(buttons[i].data()));
            Put(nodes[i], 4, i ? At(nodes[0].data()) : 0u);
            Put(nodes[i], 8, i ? 0u : At(nodes[1].data()));
            Put(buttons[i], 0x58, At(observers[i].data()));
            Put(observers[i], 0, At(popup.data()) + 0x5C);
        }
    }

    void Province() {
        Put(popup, 0x18, 0x002D2D2Du);
        Put(popup, 0x1C, 0u);
        Put(popup, 0x20, province_id);
    }

    bool Own() const { return vic2_mp_events::detail::OwnPending(context, At(popup.data())); }
    vic2_mp_events::detail::Validated Validate() const {
        return vic2_mp_events::detail::Validate(context, At(popup.data()));
    }
};

unsigned callback_calls = 0;
bool callback_closes = true;
bool callback_throws = false;

// __fastcall без stack-аргументов принимает тот же ECX; EDX не используется.
void __fastcall Callback(void* object, void*) {
    ++callback_calls;
    if (callback_throws) RaiseException(0xE0420010, 0, 0, nullptr);
    if (callback_closes) {
        auto popup = static_cast<unsigned char*>(object);
        popup[0x58] = 0;
        popup[0xCC] = 1;
    }
}

void ScopeTests() {
    Fixture f;
    auto valid = f.Validate();
    Require(valid.info.province == 0 && valid.info.country == 23 &&
            std::string(valid.info.tag) == "PRU", "country compatibility");
    f.Province();
    valid = f.Validate();
    Require(valid.info.province == 546 && valid.info.country == 23 &&
            std::string(valid.info.tag) == "PRU" && valid.info.option_count == 2,
            "province recipient must be local owner, not empty scope tag");
    f.context.state.paused = false;
    Require(f.Validate().info.province == 546, "existing unpaused validation contract");
    Put(f.province, 0x12C, 24u);
    Require(!f.Own(), "foreign index accepted");
    Reject([&] { f.Validate(); }, "E_MP_EVENT_MISMATCH");
    Put(f.province, 0x12C, 23u);
    Put(f.province, 0x128, 0x00535541u);  // AUS.
    Require(!f.Own(), "foreign tag accepted");
    Put(f.province, 0x128, f.context.tag);
    Put(f.popup, 0x1C, 23u);
    Require(!f.Own(), "mixed province/country scope accepted");
    Put(f.popup, 0x1C, 0u);
    Put(f.popup, 0x18, f.context.tag);
    Require(!f.Own(), "noncanonical province tag accepted");
    f.Province();
    Put(f.province, 0x58, 545u);
    Reject([&] { f.Validate(); }, "E_MP_LAYOUT");
    Put(f.province, 0x58, 546u);
    Put(f.popup, 0x20, 547u);
    Reject([&] { f.Validate(); }, "E_MP_LAYOUT");
    f.Province();
    Put(f.world, 0xAD0, At(f.provinces.data()) + 3u);
    Reject([&] { f.Validate(); }, "E_MP_LAYOUT");
    Put(f.world, 0xAD0, At(f.provinces.data() + f.provinces.size()));
    Put(f.popup, 0x58, static_cast<unsigned char>(0));
    Require(!f.Own(), "closed popup accepted");
    Put(f.popup, 0x58, static_cast<unsigned char>(1));
    Put(f.popup, 0xCC, static_cast<unsigned char>(1));
    Require(!f.Own(), "already queued popup accepted");
}

void StructuralTests() {
    Fixture f;
    f.Province();
    Put(f.popup, 0x34, At(f.popup.data()));
    Reject([&] { f.Validate(); }, "E_MP_LAYOUT");
    Put(f.popup, 0x34, At(f.popup.data()) + 0x10);
    Put(f.observers[0], 0, 0u);
    Reject([&] { f.Validate(); }, "E_MP_LAYOUT");
    Put(f.observers[0], 0, At(f.popup.data()) + 0x5C);
    Put(f.nodes[1], 0, At(f.buttons[0].data()));
    Reject([&] { f.Validate(); }, "E_MP_LAYOUT");
    Put(f.nodes[1], 0, At(f.buttons[1].data()));
    Put(f.nodes[1], 8, At(f.nodes[0].data()));
    Reject([&] { f.Validate(); }, "E_MP_LAYOUT");
}

void CallbackTests() {
    Fixture f;
    f.Province();
    const auto valid = f.Validate();
    callback_calls = 0;
    const auto callback = static_cast<Address>(reinterpret_cast<std::uintptr_t>(&Callback));
    Require(vic2_mp_events::detail::TryPick(callback, At(f.popup.data()),
            valid.controls.data(), valid.controls.size(), 1), "callback result");
    Require(callback_calls == 1 && !f.buttons[0][0x38] && f.buttons[1][0x38],
            "exactly one native callback / selected control");
    Reject([&] { f.Validate(); }, "E_MP_EVENT_MISMATCH");
    Require(callback_calls == 1, "closed popup selection repeated");

    Fixture unknown;
    unknown.Province();
    auto unknown_valid = unknown.Validate();
    callback_closes = false;
    Require(!vic2_mp_events::detail::TryPick(callback, At(unknown.popup.data()),
            unknown_valid.controls.data(), unknown_valid.controls.size(), 0), "unknown result accepted");
    Require(callback_calls == 2, "unknown outcome was retried internally");
    Fixture fault;
    fault.Province();
    const auto fault_valid = fault.Validate();
    callback_throws = true;
    Require(!vic2_mp_events::detail::TryPick(callback, At(fault.popup.data()),
            fault_valid.controls.data(), fault_valid.controls.size(), 0), "SEH outcome accepted");
    Require(callback_calls == 3, "exception outcome was retried internally");
}
}  // namespace

int main() {
    try {
        ScopeTests();
        StructuralTests();
        CallbackTests();
        std::cout << "mp_events: country/province ownership, structure, callback PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
