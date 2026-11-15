#pragma once

// Читающий адаптер квалифицированной Victoria II 3.04. Не выполняет мутаций,
// не расходует RNG и не хранит указатели между вызовами/загрузками кампаний.
// Пока использует общие квалифицированные функции из диагностических headers;
// их выделение в отдельный ABI-адаптер не меняет контракт снимка.
#include "calculation.h"
#include "native_probe.h"
#include "snapshot_probe.h"
#include <utility>

namespace gpc_world_reader {

struct ReadStats {
    std::uint64_t countries = 0;
    std::uint64_t provinces = 0;
    std::uint64_t pops = 0;
    std::uint64_t cultures = 0;
    std::uint64_t population = 0;
};

struct WorldSnapshot {
    std::vector<gpc::Country> countries;
    std::vector<gpc::Province> provinces;
    bool technology_present = false;
    std::map<gpc::Id, std::string> tags;
    std::map<gpc::Id, DWORD> country_ptr;
    std::map<gpc::Id, DWORD> province_ptr;
    DWORD date_raw = 0;
    ReadStats stats;
};

inline WorldSnapshot ReadWorld(DWORD base, bool qualification_mode = false) {
    using namespace gpc_probe_native;
    using gpc_snapshot_probe::Read32;
    using gpc_snapshot_probe::CultureID;
    using gpc_snapshot_probe::CultureTable;

    World world = {};
    if (!Contracts(base, false) || !GetWorld(base, &world))
        throw std::runtime_error("GPC world reader: executable/world contract");
    if (qualification_mode) {
        const unsigned char before[] = {0x53, 0x8b, 0x1d};
        const unsigned char after[] = {0x56, 0x57, 0x8b, 0x78, 0x48};
        DWORD address = 0;
        if (!Signature(base, 0x5692c0, before, sizeof(before)) ||
            !U32(base + 0x5692c3, &address) || address != base + 0xe58a20 ||
            !Signature(base, 0x5692c7, after, sizeof(after)))
            throw std::runtime_error("GPC world reader: native winner contract");
    }
    const auto cultures = CultureTable(base);
    WorldSnapshot result;
    result.date_raw = Read32(world.world + 0xb0c);
    result.stats.cultures = cultures.size();

    DWORD technology = 0, technology_index = 0;
    if (!Nationalism(base, &technology, &technology_index))
        throw std::runtime_error("GPC world reader: nationalism lookup");
    result.technology_present = technology != 0;

    DWORD pop_manager = Read32(base + 0xf1bb34);
    DWORD type_range[2] = {};
    if (!pop_manager || !Read(pop_manager + 0xc, type_range, sizeof(type_range)) ||
        !type_range[0] || type_range[1] <= type_range[0] ||
        ((type_range[1] - type_range[0]) & 3) ||
        (type_range[1] - type_range[0]) / 4 > 4096)
        throw std::runtime_error("GPC world reader: POP type vector");
    std::vector<DWORD> pop_types((type_range[1] - type_range[0]) / 4);
    if (!Read(type_range[0], pop_types.data(), pop_types.size() * sizeof(DWORD)))
        throw std::runtime_error("GPC world reader: POP type vector unreadable");
    std::set<DWORD> unique_types;
    for (DWORD type : pop_types)
        if (!type || !unique_types.insert(type).second)
            throw std::runtime_error("GPC world reader: invalid POP type pointer");

    std::set<DWORD> great_powers;
    for (DWORD n = 0; n < world.gpCount; ++n) {
        DWORD pointer = Read32(world.gpBegin + n * 4);
        Identity identity = {}, canonical = {};
        DWORD country = 0;
        if (!pointer || !Read(pointer + 0x1c, &identity, sizeof(identity)) ||
            !Country(world, identity.index, &country, &canonical) ||
            country != pointer || canonical.tag != identity.tag ||
            !great_powers.insert(identity.index).second)
            throw std::runtime_error("GPC world reader: GP vector identity");
    }

    std::map<DWORD, DWORD> packed_tags;
    std::map<DWORD, bool> existing;
    std::set<std::string> unique_tags;
    for (DWORD id = 1; id < world.countryCount; ++id) {
        DWORD pointer = 0;
        Identity identity = {};
        // Пустые слоты и служебная --- не являются каноническими странами.
        if (!Country(world, id, &pointer, &identity)) continue;
        const std::string tag = gpc_snapshot_probe::Tag(identity);
        if (!unique_tags.insert(tag).second)
            throw std::runtime_error("GPC world reader: duplicate country tag");
        DWORD owned[2] = {};
        unsigned char gp = 0;
        if (!Read(pointer + 0x9d8, owned, sizeof(owned)) || owned[1] < owned[0] ||
            ((owned[1] - owned[0]) & 3) || (owned[1] - owned[0]) / 4 > world.provinceCount ||
            (owned[1] != owned[0] && !owned[0]) || !Read(pointer + 0x31, &gp, sizeof(gp)))
            throw std::runtime_error("GPC world reader: country ownership/GP fields");
        const bool exists = owned[0] != owned[1];
        if (exists && bool(gp) != bool(great_powers.count(id)))
            throw std::runtime_error("GPC world reader: GP flag/vector mismatch");
        int technology_level = 0;
        if (technology && !TechnologyLevel(pointer, technology_index, &technology_level))
            throw std::runtime_error("GPC world reader: country technology state");
        if (technology_level < 0)
            throw std::runtime_error("GPC world reader: negative technology level");
        const DWORD primary_pointer = Read32(pointer + 0xe64);
        gpc::Country country{id, exists, bool(gp), technology_level > 0,
                             primary_pointer ? CultureID(primary_pointer, cultures) + 1 : 0, {}};
        // Признанные культуры не участвуют в механике: их связанный список
        // намеренно не читается, диагностическое поле снимка остаётся пустым.
        result.countries.push_back(std::move(country));
        result.tags.emplace(id, tag);
        result.country_ptr.emplace(id, pointer);
        packed_tags.emplace(id, identity.tag);
        existing.emplace(id, exists);
        ++result.stats.countries;
    }

    for (DWORD id = 1; id < world.provinceCount; ++id) {
        DWORD pointer = Read32(world.provinces + id * 4);
        if (!pointer) continue;
        if (Read32(pointer + 0x58) != id)
            throw std::runtime_error("GPC world reader: province identity");
        Identity owner = {};
        if (!Read(pointer + 0x128, &owner, sizeof(owner)))
            throw std::runtime_error("GPC world reader: owner identity unreadable");
        if (owner.index && (!packed_tags.count(owner.index) ||
            packed_tags.at(owner.index) != owner.tag || !existing.at(owner.index)))
            throw std::runtime_error("GPC world reader: unknown/extinct owner");
        gpc::Province province{id, owner.index, {}, {}};

        DWORD cores[2] = {};
        if (!Read(pointer + 0x148, cores, sizeof(cores)) || cores[1] < cores[0] ||
            ((cores[1] - cores[0]) & 7) || (cores[1] - cores[0]) / 8 > world.countryCount ||
            (cores[0] != cores[1] && !cores[0]))
            throw std::runtime_error("GPC world reader: core vector bounds");
        for (DWORD at = cores[0]; at < cores[1]; at += 8) {
            Identity core = {};
            if (!Read(at, &core, sizeof(core)) || !packed_tags.count(core.index) ||
                packed_tags.at(core.index) != core.tag)
                throw std::runtime_error("GPC world reader: unknown core identity");
            province.cores.push_back(core.index);
        }

        DWORD lists = Read32(pointer + 0x194);
        std::vector<DWORD> list_heads;
        if (lists) {
            // Четыре DWORD на тип: head, tail, count и служебное поле.
            list_heads.resize(pop_types.size() * 4);
            if (!Read(lists, list_heads.data(), list_heads.size() * sizeof(DWORD)))
                throw std::runtime_error("GPC world reader: POP lists unreadable");
        }
        std::vector<gpc::Population> pops;
        std::set<DWORD> seen_pops;
        for (size_t type = 0; lists && type < pop_types.size(); ++type) {
            DWORD pop = list_heads[type * 4];
            while (pop) {
                if (seen_pops.size() >= 1000000 || !seen_pops.insert(pop).second)
                    throw std::runtime_error("GPC world reader: POP cycle/duplicate");
                // Один защищённый memcpy вместо VirtualQuery для каждого поля.
                // Последний DWORD — next +0x27C; начальный — size +0x58.
                DWORD fields[(0x280 - 0x58) / 4] = {};
                if (!Read(pop + 0x58, fields, sizeof(fields)) ||
                    fields[0] > INT32_MAX || fields[3] != pointer || fields[4] != pop_types[type])
                    throw std::runtime_error("GPC world reader: POP size/province/type");
                const gpc::Id culture = CultureID(fields[5], cultures) + 1;
                pops.push_back({culture, fields[0]});
                if (fields[0] > (std::numeric_limits<std::uint64_t>::max)() - result.stats.population)
                    throw std::overflow_error("GPC world reader: total population overflow");
                result.stats.population += fields[0];
                ++result.stats.pops;
                pop = fields[(0x27c - 0x58) / 4];
            }
        }
        // DominantCultures суммирует все профессии и возвращает все равные
        // максимумы по возрастанию remapped ID. Новая механика учитывает их все;
        // первый остаётся контрольным значением штатного выбора одной культуры.
        province.dominant_cultures = gpc::DominantCultures(pops);

        if (qualification_mode) {
            const auto totals = gpc::SumPopulationByCulture(pops);
            DWORD cache = Read32(pointer + 0x1f0);
            std::vector<DWORD> native_counts(cultures.size(), 0);
            if (cache && !Read(cache, native_counts.data(), native_counts.size() * sizeof(DWORD)))
                throw std::runtime_error("GPC world reader: native culture cache unreadable");
            for (size_t c = 0; c < cultures.size(); ++c) {
                const auto found = totals.find(static_cast<gpc::Id>(c + 1));
                const std::uint64_t expected = found == totals.end() ? 0 : found->second;
                if (expected != native_counts[c])
                    throw std::runtime_error("GPC world reader: native culture cache mismatch");
            }
            if (!province.dominant_cultures.empty()) {
                DWORD native = 0;
                if (!cache || !gpc_snapshot_probe::NativeWinner(base + 0x5692c0, pointer + 0x1a8, &native) ||
                    CultureID(native, cultures) + 1 != province.dominant_cultures.front())
                    throw std::runtime_error("GPC world reader: native winner mismatch");
            }
        }
        result.provinces.push_back(std::move(province));
        result.province_ptr.emplace(id, pointer);
        ++result.stats.provinces;
    }
    return result;
}

} // namespace gpc_world_reader
