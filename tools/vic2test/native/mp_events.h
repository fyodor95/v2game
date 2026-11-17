#pragma once

#include "mp_control.h"
#include <algorithm>
#include <vector>

// Выбор существующего события страны/провинции локального игрока. Участники MP
// используют штатный callback и очередь; события и scope этот адаптер не создаёт.
// Вызывающий мост проверяет owner thread и дедупликацию запросов.
namespace vic2_mp_events {

struct PopupInfo {
    std::uint32_t popup = 0;
    std::uint32_t event = 0;
    unsigned event_id = 0;
    unsigned country = 0;
    unsigned province = 0;  // 0 — страновое окно; country/tag всегда локальный игрок.
    char tag[4]{};
    unsigned option_count = 0;
};

struct PickResult {
    PopupInfo before;
    unsigned option_index = 0;
    bool queued = false;  // Не означает исполнения эффекта на всех участниках.
};

namespace detail {

using vic2_mp::detail::Address;
using vic2_mp::detail::Add;
using vic2_mp::detail::Flag;
using vic2_mp::detail::Pointer;
using vic2_mp::detail::Read;
using vic2_mp::detail::Signature;
using vic2_mp::detail::Value;

inline void Signatures(Address base) {
    static const unsigned char prologue[] = {0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0};
    static const unsigned char select[] = {
        0x8B, 0x08, 0x80, 0x79, 0x38, 0x00, 0x8B, 0x40,
        0x08, 0x75, 0x16, 0x47, 0x85, 0xC0, 0x75, 0xF0
    };
    static const unsigned char create[] = {
        0x57, 0x83, 0xEC, 0x48, 0x8D, 0x4E, 0x10, 0x8B, 0xC4, 0x89, 0x65, 0xEC,
        0xE8, 0xF1, 0xA6, 0xFF, 0xFF, 0x8B, 0x46, 0x0C, 0x50, 0x53,
        0xE8, 0x57, 0x9B, 0x05, 0x00
    };
    static const unsigned char enqueue[] = {
        0x8B, 0x4E, 0x08, 0x8B, 0x11, 0x8B, 0x42, 0x44, 0xFF, 0xD0,
        0x8B, 0x48, 0x38, 0x8B, 0x11, 0x8B, 0x42, 0x18, 0x57, 0xFF, 0xD0,
        0xC6, 0x46, 0x58, 0x00, 0xC6, 0x86, 0xCC, 0x00, 0x00, 0x00, 0x01
    };
    static const unsigned char recipient[] = {
        0x8B, 0x48, 0x04, 0x8B, 0x04, 0x91, 0x8B, 0x80, 0xDC, 0x0C, 0x00, 0x00,
        0x89, 0x47, 0x08, 0x8B, 0x45, 0x0C, 0x89, 0x47, 0x0C, 0x8D, 0x77, 0x10,
        0x8D, 0x4D, 0x10, 0x8B, 0xC6, 0xE8, 0x44, 0xF9, 0xFF, 0xFF
    };
    static const unsigned char popup_self[] = {0x89, 0x77, 0x34, 0xC6, 0x47, 0x58, 0x01};
    static const unsigned char command_self[] = {
        0x8D, 0x7E, 0x68, 0x89, 0x56, 0x3C, 0x89, 0xBC, 0x24, 0x84, 0x00, 0x00,
        0x00, 0x89, 0xBE, 0x8C, 0x00, 0x00, 0x00
    };
    static const unsigned char command_flags[] = {
        0x66, 0x89, 0x4E, 0x28, 0x66, 0x89, 0x5E, 0x36, 0x89, 0x5E, 0x38
    };
    static const unsigned char register_head[] = {
        0x8B, 0x46, 0x60, 0x53, 0x8B, 0x5D, 0x08, 0x57, 0x85, 0xC0
    };
    static const unsigned char register_node[] = {
        0x89, 0x18, 0x89, 0x78, 0x04, 0xC7, 0x40, 0x08, 0x00, 0x00, 0x00, 0x00
    };
    static const unsigned char register_tail[] = {
        0x8B, 0x4E, 0x68, 0x8D, 0x51, 0x01, 0x89, 0x46, 0x64, 0x89, 0x56, 0x68,
        0x85, 0xC9, 0x74, 0x09, 0x89, 0x47, 0x08, 0x5F, 0x5B, 0x5D,
        0xC2, 0x04, 0x00, 0x89, 0x46, 0x60
    };
    // Province scope сохраняется в общей команде, получатель — текущий владелец.
    static const unsigned char province_recipient[] = {
        0x8B, 0x5E, 0x10, 0x8B, 0x04, 0x98, 0x8B, 0x98, 0x28, 0x01, 0x00, 0x00,
        0x89, 0x5C, 0x24, 0x18, 0x8B, 0x98, 0x2C, 0x01, 0x00, 0x00
    };
    static const unsigned char copy_province[] = {0x8B, 0x51, 0x10, 0x89, 0x50, 0x10};
    static const unsigned char command_province[] = {0x8B, 0x46, 0x78, 0x3B, 0xC3};
    static const unsigned char command_province_owner[] = {
        0x8B, 0x8A, 0xCC, 0x0A, 0x00, 0x00, 0x8B, 0x0C, 0x81,
        0x39, 0x99, 0x2C, 0x01, 0x00, 0x00
    };
    static const unsigned char callback_return[] = {0x5B, 0x8B, 0xE5, 0x5D, 0xC3};
    Signature(base, 0x4AE150, prologue);
    Signature(base, 0x4AE190, select);
    Signature(base, 0x4AE1CE, create);
    Signature(base, 0x4AE1F6, enqueue);
    // Вход конструктора уже перехвачен мостом; проверяем его неизменённое тело.
    Signature(base, 0x4A8F6A, recipient);
    Signature(base, 0x4A9062, popup_self);
    Signature(base, 0x507E68, command_self);
    Signature(base, 0x507DA2, command_flags);
    Signature(base, 0x2480E3, register_head);
    Signature(base, 0x24810C, register_node);
    Signature(base, 0x248120, register_tail);
    Signature(base, 0x4A816C, province_recipient);
    Signature(base, 0x4A88E9, copy_province);
    Signature(base, 0x508001, command_province);
    Signature(base, 0x508012, command_province_owner);
    Signature(base, 0x4AE222, callback_return);
    Value(Add(base, 0xA333BC + 0x18), Add(base, 0x507E00));
}

struct Context {
    vic2_mp::State state;
    Address base = 0;
    Address country_pointer = 0;
    unsigned country = 0;
    std::uint32_t tag = 0;
};

inline Context Current(Address base, unsigned localcountry) {
    Context context;
    context.state = vic2_mp::Inspect(base);
    context.base = base;
    if (context.state.mode != 1 || !context.state.network_ready)
        throw std::runtime_error("E_MP_NOT_READY: требуется активная сетевая кампания");
    if (context.state.executing)
        throw std::runtime_error("E_MP_BUSY: движок исполняет игровую команду");
    Signatures(base);
    context.country = Read<unsigned>(Add(context.state.world, 0xB60));
    if (!localcountry || context.country != localcountry)
        throw std::runtime_error("E_MP_EVENT_RECIPIENT: получатель не является локальным игроком");
    const Address countries = Pointer(Add(base, 0xE587E4));
    const unsigned count = Read<unsigned>(countries);
    if (!count || count > 10000 || localcountry >= count)
        throw std::runtime_error("E_MP_LAYOUT: недопустимый индекс страны");
    context.country_pointer = Pointer(Add(Pointer(Add(countries, 4)), localcountry * 4));
    Value(Add(context.country_pointer, 0x20), localcountry);
    Value(Add(context.country_pointer, 0xCDC), context.state.idler);
    context.tag = Read<std::uint32_t>(Add(context.country_pointer, 0x1C));
    Value(Add(context.state.world, 0xB5C), context.tag);
    for (unsigned i = 0; i != 3; ++i) {
        const auto c = static_cast<unsigned char>(context.tag >> (i * 8));
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            throw std::runtime_error("E_MP_LAYOUT: недопустимый тег страны");
    }
    if (context.tag >> 24)
        throw std::runtime_error("E_MP_LAYOUT: недопустимый тег страны");
    return context;
}

inline std::vector<Address> ListValues(Address head, Address tail, unsigned count, unsigned limit) {
    if (count > limit)
        throw std::runtime_error("E_MP_LAYOUT: превышен предел списка интерфейса");
    std::vector<Address> values;
    std::vector<Address> nodes;
    Address previous = 0;
    for (Address node = head; node; node = Read<Address>(Add(node, 8))) {
        if (values.size() >= count || std::find(nodes.begin(), nodes.end(), node) != nodes.end())
            throw std::runtime_error("E_MP_LAYOUT: повреждён список интерфейса");
        Value(Add(node, 4), previous);
        nodes.push_back(node);
        const Address object = Pointer(node);
        if (std::find(values.begin(), values.end(), object) != values.end())
            throw std::runtime_error("E_MP_LAYOUT: повтор объекта в списке интерфейса");
        values.push_back(object);
        previous = node;
    }
    if (values.size() != count || previous != tail)
        throw std::runtime_error("E_MP_LAYOUT: длина списка интерфейса не совпадает");
    return values;
}

inline std::vector<Address> Registered(const Context& context) {
    const Address idler = context.state.idler;
    return ListValues(Read<Address>(Add(idler, 0x60)), Read<Address>(Add(idler, 0x64)),
                      Read<unsigned>(Add(idler, 0x68)), 4096);
}

inline bool OwnPending(const Context& context, Address popup) {
    if (Read<Address>(popup) != Add(context.base, 0xA2CDB8) ||
        !Flag(Add(popup, 0x58)) || Flag(Add(popup, 0xCC)))
        return false;
    const auto country = Read<unsigned>(Add(popup, 0x1C));
    const auto tag = Read<std::uint32_t>(Add(popup, 0x18));
    const auto province = Read<unsigned>(Add(popup, 0x20));
    if (!province)
        return country == context.country && tag == context.tag;
    // Нативный province scope имеет пустые country index/tag. Получателя
    // определяем по canonical province, не подменяя содержимое scope окна.
    if (country || tag != 0x002D2D2D)
        return false;
    const Address begin = Pointer(Add(context.state.world, 0xACC));
    const Address end = Read<Address>(Add(context.state.world, 0xAD0));
    if (end <= begin || end - begin > 100000 * 4 || ((end - begin) & 3) ||
        province >= (end - begin) / 4)
        throw std::runtime_error("E_MP_LAYOUT: недопустимая провинция события");
    const Address object = Pointer(Add(begin, province * 4));
    Value(Add(object, 0x58), province);
    return Read<unsigned>(Add(object, 0x12C)) == context.country &&
        Read<std::uint32_t>(Add(object, 0x128)) == context.tag;
}

inline void CheckObserver(Address control, Address popup) {
    const Address expected = Add(popup, 0x5C);
    std::vector<Address> visited;
    for (Address node = Read<Address>(Add(control, 0x58)); node; node = Read<Address>(Add(node, 8))) {
        if (visited.size() >= 64 || std::find(visited.begin(), visited.end(), node) != visited.end())
            throw std::runtime_error("E_MP_LAYOUT: повреждён список обработчиков кнопки");
        visited.push_back(node);
        if (Read<Address>(node) == expected)
            return;
    }
    throw std::runtime_error("E_MP_LAYOUT: кнопка не связана с выбранным событием");
}

struct Validated {
    PopupInfo info;
    std::vector<Address> controls;
};

inline Validated Validate(const Context& context, Address popup) {
    if (!OwnPending(context, popup))
        throw std::runtime_error("E_MP_EVENT_MISMATCH: нет ожидающего окна страны/провинции локального игрока");
    const Address base = context.base;
    Value(Add(popup, 8), context.state.idler);
    Value(Add(popup, 0x10), Add(base, 0xA2C904));
    Value(Add(popup, 0x34), Add(popup, 0x10));  // Не исправлять чужой scope вручную.
    Value(Add(popup, 0x5C), Add(base, 0xA2CDD4));
    Value(Add(popup, 0x60), popup);
    Value(Add(popup, 0x64), Add(base, 0x4AE150));
    Validated result;
    result.info.popup = popup;
    result.info.country = context.country;
    result.info.province = Read<unsigned>(Add(popup, 0x20));
    std::memcpy(result.info.tag, &context.tag, sizeof(result.info.tag));
    result.info.event = Pointer(Add(popup, 0x0C));
    Value(Add(result.info.event, 8), 0x27);
    result.info.event_id = Read<unsigned>(Add(result.info.event, 0x0C));
    if (!result.info.event_id)
        throw std::runtime_error("E_MP_LAYOUT: у события отсутствует идентификатор");
    const Address begin = Pointer(Add(result.info.event, 0xB0));
    const Address end = Read<Address>(Add(result.info.event, 0xB4));
    if (end <= begin || end - begin > 400 || ((end - begin) & 3))
        throw std::runtime_error("E_MP_LAYOUT: повреждён список опций события");
    result.info.option_count = (end - begin) / 4;
    result.controls = ListValues(Read<Address>(Add(popup, 0xB4)), Read<Address>(Add(popup, 0xB8)),
                                 Read<unsigned>(Add(popup, 0xBC)), 100);
    if (result.controls.size() != result.info.option_count)
        throw std::runtime_error("E_MP_LAYOUT: кнопки не соответствуют опциям события");
    for (const Address control : result.controls) {
        Flag(Add(control, 0x38));
        CheckObserver(control, popup);
    }
    return result;
}

// Все проверки выполнены до записи. Эти байты принадлежат локальным кнопкам,
// а игровой эффект выбирает и исполняет штатная сетевая команда callback.
inline bool TryPick(Address callback, Address popup, const Address* controls,
                    std::size_t count, unsigned option) noexcept {
    __try {
        for (std::size_t i = 0; i < count; ++i)
            *reinterpret_cast<std::uint8_t*>(controls[i] + 0x38) = i == option ? 1 : 0;
        reinterpret_cast<void (__thiscall*)(void*)>(callback)(reinterpret_cast<void*>(popup));
        return *reinterpret_cast<const std::uint8_t*>(popup + 0x58) == 0 &&
               *reinterpret_cast<const std::uint8_t*>(popup + 0xCC) == 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}  // namespace detail

// localcountry — индекс из world+0xB60. Публичные методы не принимают адресов
// чужих стран и не полагаются на requestedEventId, установленный CLI.
inline PopupInfo ValidatePopup(std::uint32_t base, std::uint32_t popup, unsigned localcountry) {
    const auto context = detail::Current(base, localcountry);
    const auto registered = detail::Registered(context);
    if (std::find(registered.begin(), registered.end(), popup) == registered.end())
        throw std::runtime_error("E_MP_EVENT_STALE: окно не зарегистрировано у текущего idler");
    return detail::Validate(context, popup).info;
}

// Read-only восстановление списка по живым объектам движка, включая окна,
// которые не попали в диагностический массив constructor hook.
inline std::vector<PopupInfo> List(std::uint32_t base, unsigned localcountry) {
    const auto context = detail::Current(base, localcountry);
    std::vector<PopupInfo> result;
    for (const auto popup : detail::Registered(context))
        if (detail::OwnPending(context, popup))
            result.push_back(detail::Validate(context, popup).info);
    return result;
}

inline PickResult Pick(std::uint32_t base, std::uint32_t popup, unsigned optionIndex,
                       unsigned localcountry) {
    const auto context = detail::Current(base, localcountry);
    const auto registered = detail::Registered(context);
    if (std::find(registered.begin(), registered.end(), popup) == registered.end())
        throw std::runtime_error("E_MP_EVENT_STALE: окно не зарегистрировано у текущего idler");
    const auto validated = detail::Validate(context, popup);
    if (optionIndex >= validated.info.option_count)
        throw std::runtime_error("E_MP_ARGUMENT: индекс опции вне диапазона");
    if (!detail::TryPick(detail::Add(base, 0x4AE150), popup, validated.controls.data(),
                         validated.controls.size(), optionIndex))
        throw std::runtime_error(
            "E_MP_NATIVE_CALL: исход выбора опции неизвестен; повторная отправка запрещена");
    PickResult result;
    result.before = validated.info;
    result.option_index = optionIndex;
    result.queued = true;
    return result;
}

}  // namespace vic2_mp_events
