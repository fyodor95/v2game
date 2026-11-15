#pragma once

// Внутренние операции механики на квалифицированной Victoria II 3.04.
// Владелец callback обеспечивает поток и общую фазу симуляции, срок жизни
// снимка в пределах этой фазы и одинаковый порядок операций во всех клиентах.
// Адаптер не хранит указатели и не отправляет отдельные сетевые команды.
// SP-проверка mutator/save/reload пройдена; MP-приёмка остаётся обязательной.
#include "world_reader.h"
#include "notify_probe.h"

namespace gpc_native_ops {

using gpc::CoreGrant;
using gpc::Id;
using gpc_world_reader::WorldSnapshot;
using gpc_probe_native::Identity;
using gpc_probe_native::World;

inline void Fail(const char* message) { throw std::runtime_error(message); }

inline World CurrentWorld(DWORD base, const WorldSnapshot& snapshot, bool mutation) {
    World world = {};
    DWORD date = 0;
    if (!gpc_probe_native::Contracts(base, mutation) ||
        !gpc_probe_native::GetWorld(base, &world) ||
        !gpc_probe_native::U32(world.world + 0xb0c, &date) || date != snapshot.date_raw)
        Fail("GPC native: неподходящий EXE, неготовый мир или устаревшая дата снимка");
    return world;
}

inline const gpc::Country& SnapshotCountry(const WorldSnapshot& snapshot, Id id) {
    // ReadWorld строит оба вектора по возрастанию native ID. Бинарный поиск
    // не добавляет перебор всех стран/провинций на каждую выдаваемую пару.
    const auto found = std::lower_bound(snapshot.countries.begin(), snapshot.countries.end(), id,
        [](const gpc::Country& country, Id key) { return country.id < key; });
    if (found == snapshot.countries.end() || found->id != id)
        Fail("GPC native: страна отсутствует в снимке");
    return *found;
}

inline const gpc::Province& SnapshotProvince(const WorldSnapshot& snapshot, Id id) {
    const auto found = std::lower_bound(snapshot.provinces.begin(), snapshot.provinces.end(), id,
        [](const gpc::Province& province, Id key) { return province.id < key; });
    if (found == snapshot.provinces.end() || found->id != id)
        Fail("GPC native: провинция отсутствует в снимке");
    return *found;
}

inline DWORD PackedTag(const std::string& tag) {
    if (tag.size() != 3) Fail("GPC native: неправильная длина тега снимка");
    DWORD packed = 0;
    for (unsigned n = 0; n < 3; ++n) {
        const unsigned char c = static_cast<unsigned char>(tag[n]);
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            Fail("GPC native: недопустимый тег снимка");
        packed |= DWORD(c) << (8 * n);
    }
    return packed;
}

inline DWORD ResolveCountry(const World& world, const WorldSnapshot& snapshot, Id id, Identity* identity) {
    const auto pointer = snapshot.country_ptr.find(id);
    const auto tag = snapshot.tags.find(id);
    DWORD actual = 0;
    if (pointer == snapshot.country_ptr.end() || tag == snapshot.tags.end() ||
        !gpc_probe_native::Country(world, id, &actual, identity) ||
        actual != pointer->second || identity->tag != PackedTag(tag->second))
        Fail("GPC native: указатель или identity страны изменились после снимка");
    return actual;
}

inline void Technology(DWORD base, const WorldSnapshot& snapshot, DWORD* object, DWORD* index) {
    if (!gpc_probe_native::Nationalism(base, object, index) || bool(*object) != snapshot.technology_present)
        Fail("GPC native: набор технологий изменился или lookup завершился ошибкой");
}

inline DWORD Recipient(const World& world, const WorldSnapshot& snapshot, Id id,
                       DWORD technology, DWORD technologyIndex, Identity* identity) {
    const auto& expected = SnapshotCountry(snapshot, id);
    if (!expected.exists || !expected.great_power || !expected.primary_culture ||
        (snapshot.technology_present && !expected.technology_researched))
        Fail("GPC native: получатель не соответствует условиям снимка");
    DWORD country = ResolveCountry(world, snapshot, id, identity);
    DWORD owned[2] = {}, primary = 0, cultureIndex = 0;
    unsigned char gp = 0;
    int level = 0;
    if (!gpc_probe_native::Read(country + 0x9d8, owned, sizeof(owned)) || !owned[0] ||
        owned[1] <= owned[0] || ((owned[1] - owned[0]) & 3) ||
        (owned[1] - owned[0]) / 4 > world.provinceCount ||
        !gpc_probe_native::Read(country + 0x31, &gp, sizeof(gp)) || !gp ||
        !gpc_probe_native::U32(country + 0xe64, &primary) || !primary ||
        !gpc_probe_native::U32(primary + 8, &cultureIndex) ||
        cultureIndex == UINT32_MAX || cultureIndex + 1 != expected.primary_culture ||
        (technology && (!gpc_probe_native::TechnologyLevel(country, technologyIndex, &level) || level < 1)))
        Fail("GPC native: существование, GP, primary или технология получателя изменились");
    return country;
}

struct Target {
    DWORD province = 0, country = 0;
    Identity identity = {};
    unsigned present = 0;
};

inline Target CheckTarget(const World& world, const WorldSnapshot& snapshot, const CoreGrant& grant,
                          DWORD technology, DWORD technologyIndex) {
    Target target;
    target.country = Recipient(world, snapshot, grant.country, technology, technologyIndex, &target.identity);
    const auto& expected = SnapshotProvince(snapshot, grant.province);
    const auto pointer = snapshot.province_ptr.find(grant.province);
    DWORD id = 0;
    Identity owner = {};
    if (!grant.province || grant.province >= world.provinceCount ||
        pointer == snapshot.province_ptr.end() ||
        !gpc_probe_native::U32(world.provinces + grant.province * 4, &target.province) ||
        !target.province || target.province != pointer->second ||
        !gpc_probe_native::U32(target.province + 0x58, &id) || id != grant.province ||
        !gpc_probe_native::Read(target.province + 0x128, &owner, sizeof(owner)) ||
        !owner.index || owner.index != expected.owner || owner.index == grant.country)
        Fail("GPC native: указатель, ID или владелец целевой провинции изменились");
    Identity canonicalOwner = {};
    ResolveCountry(world, snapshot, owner.index, &canonicalOwner);
    if (canonicalOwner.tag != owner.tag || !SnapshotCountry(snapshot, owner.index).exists)
        Fail("GPC native: identity юридического владельца не согласована");
    unsigned reverse = 0;
    if (!gpc_probe_native::CoreCount(world, target.province, grant.country, &target.present) ||
        !gpc_probe_native::ReverseCoreCount(world, target.country, grant.province, &reverse) ||
        target.present != reverse || target.present > 1)
        Fail("GPC native: прямая и обратная связи core не согласованы");
    return target;
}

struct Notification {
    DWORD event = 0, registry = 0;
};

inline Notification CheckNotification(DWORD base, const World& world, unsigned eventID) {
    Notification result;
    DWORD options = 0, optionsEnd = 0, option = 0, dummyProvince = 0, dummyField = 0;
    DWORD humans = 0, humansEnd = 0;
    if (!eventID || !gpc_probe_notify::Contracts(base) ||
        !gpc_probe_notify::FindCountryEvent(base, eventID, &result.event, &result.registry) ||
        !gpc_probe_native::U32(result.event + 0xb0, &options) || !options ||
        !gpc_probe_native::U32(result.event + 0xb4, &optionsEnd) ||
        optionsEnd < options || optionsEnd - options != 4 ||
        !gpc_probe_native::U32(options, &option) || !option ||
        !gpc_probe_native::U32(world.provinces, &dummyProvince) || !dummyProvince ||
        !gpc_probe_native::U32(dummyProvince + 0x188, &dummyField) ||
        !gpc_probe_native::U32(world.world + 0xaec, &humans) || !humans ||
        !gpc_probe_native::U32(world.world + 0xaf0, &humansEnd) || humansEnd < humans ||
        ((humansEnd - humans) & 3) || (humansEnd - humans) / 4 > 65536)
        Fail("GPC native: уведомление или native scope не прошли проверку");
    return result;
}

inline void CheckHumanEntry(const World& world, Id recipient) {
    DWORD begin = 0, end = 0, value = 0;
    if (!gpc_probe_native::U32(world.world + 0xaec, &begin) || !begin ||
        !gpc_probe_native::U32(world.world + 0xaf0, &end) || end < begin || ((end - begin) & 3) ||
        recipient >= (end - begin) / 4 || !gpc_probe_native::U32(begin + recipient * 4, &value))
        Fail("GPC native: запись human/AI получателя недоступна");
    // Значение определяет штатный путь CEvent::run; не отбираем здесь
    // локального игрока и не исключаем удалённых людей или ИИ.
}

// Не выполняет native writes. Отказ любого target/notification обнаруживается
// до первой мутации. Повторные пары и уже имеющиеся согласованные cores допустимы.
inline void Preflight(DWORD base, const WorldSnapshot& snapshot, const std::vector<CoreGrant>& grants,
                      unsigned eventID, GpcProbeLog log = nullptr) {
    const World world = CurrentWorld(base, snapshot, true);
    CheckNotification(base, world, eventID);
    if (!std::is_sorted(grants.begin(), grants.end()))
        Fail("GPC native: список выдачи не отсортирован по province/country ID");
    DWORD technology = 0, index = 0;
    Technology(base, snapshot, &technology, &index);
    std::set<CoreGrant> checked;
    std::set<Id> recipients;
    for (const auto& grant : grants) {
        if (!checked.insert(grant).second) continue;
        CheckTarget(world, snapshot, grant, technology, index);
        if (recipients.insert(grant.country).second) CheckHumanEntry(world, grant.country);
    }
    CurrentWorld(base, snapshot, true);
    if (log) log("GPC native: preflight ok pairs=%u recipients=%u date_raw=%lu",
        (unsigned)checked.size(), (unsigned)recipients.size(), snapshot.date_raw);
}

// false = согласованная пара уже существует; true = фактически добавлена.
// Любой отказ — исключение, чтобы его нельзя было принять за пустую выдачу.
inline bool ApplyCore(DWORD base, const WorldSnapshot& snapshot, const CoreGrant& grant, GpcProbeLog log) {
    const World world = CurrentWorld(base, snapshot, true);
    DWORD technology = 0, index = 0;
    Technology(base, snapshot, &technology, &index);
    const Target target = CheckTarget(world, snapshot, grant, technology, index);
    if (target.present) return false;
    if (!gpc_probe_native::InvokeAddCore(base + 0xb61a0, target.province, &target.identity, target.country)) {
        if (log) log("GPC native: ERROR add_core province=%u country=%u, возможна частичная выдача; повтора нет",
            grant.province, grant.country);
        Fail("GPC native: исключение при native add_core; результат может быть частичным");
    }
    unsigned forward = 0, reverse = 0;
    unsigned char dirty = 0;
    if (!gpc_probe_native::CoreCount(world, target.province, grant.country, &forward) ||
        !gpc_probe_native::ReverseCoreCount(world, target.country, grant.province, &reverse) ||
        !gpc_probe_native::Read(target.country + 0x1434, &dirty, sizeof(dirty)) ||
        forward != 1 || reverse != 1 || dirty != 1) {
        if (log) log("GPC native: ERROR postcheck province=%u country=%u forward=%u reverse=%u dirty=%u",
            grant.province, grant.country, forward, reverse, (unsigned)dirty);
        Fail("GPC native: проверка результата add_core не пройдена; автоматического повтора нет");
    }
    return true;
}

// Один вызов на страну после всех фактических выдач. Нативный движок сам
// выбирает локальное окно/удалённого человека/ИИ. true означает dispatch,
// а не пользовательское подтверждение окна или завершение queued AI command.
inline bool Notify(DWORD base, const WorldSnapshot& snapshot, Id recipient, unsigned eventID, GpcProbeLog log) {
    const World world = CurrentWorld(base, snapshot, false);
    DWORD technology = 0, index = 0;
    Technology(base, snapshot, &technology, &index);
    Identity identity = {};
    Recipient(world, snapshot, recipient, technology, index, &identity);
    CheckHumanEntry(world, recipient);
    const Notification notification = CheckNotification(base, world, eventID);
    if (!gpc_probe_notify::Dispatch(base, notification.event, notification.registry, identity)) {
        if (log) log("GPC native: ERROR notify event=%u country=%u; результат может быть частичным", eventID, recipient);
        Fail("GPC native: native dispatch уведомления завершился ошибкой");
    }
    if (log) log("GPC native: notify dispatched event=%u country=%u", eventID, recipient);
    return true;
}

} // namespace gpc_native_ops
