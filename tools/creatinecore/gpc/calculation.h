#pragma once

// Чистый расчёт: ни указателей движка, ни выдачи корок, ни случайности.
// Нулевой ID обозначает отсутствие страны/культуры. Нативный адаптер обязан
// преобразовать реальные идентификаторы игры в этот контракт без коллизий.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gpc {

using Id = std::uint32_t;

struct Country {
    Id id;
    bool exists;
    bool great_power;
    bool technology_researched;
    Id primary_culture;
    std::vector<Id> accepted_cultures;
};

struct Province {
    Id id;
    Id owner;
    // nullopt: нет населения/культуры. Разрешение ничьей принадлежит адаптеру,
    // который должен воспроизвести квалифицированное правило движка.
    std::optional<Id> dominant_culture;
    std::vector<Id> cores;
};

struct CoreGrant {
    Id province;
    Id country;
    bool operator==(const CoreGrant& other) const {
        return province == other.province && country == other.country;
    }
    bool operator<(const CoreGrant& other) const {
        return province < other.province ||
               (province == other.province && country < other.country);
    }
};

struct Population {
    Id culture;
    // Целая неотрицательная численность в одинаковых единицах для всех POP.
    // Масштаб fixed-point, если он есть, выбирает и проверяет адаптер.
    std::uint64_t size;
};

inline std::map<Id, std::uint64_t> SumPopulationByCulture(
    const std::vector<Population>& pops) {
    std::map<Id, std::uint64_t> totals;
    std::uint64_t population = 0;
    for (const auto& pop : pops) {
        if (!pop.culture) throw std::invalid_argument("POP culture ID is zero");
        // Проверяем и общий итог: размер провинции не может переполниться
        // даже тогда, когда каждая отдельная культура ещё помещается в uint64.
        if (pop.size > (std::numeric_limits<std::uint64_t>::max)() - population)
            throw std::overflow_error("Province population overflow");
        population += pop.size;
        totals[pop.culture] += pop.size;
    }
    return totals;
}

inline std::vector<Id> DominantCultures(const std::vector<Population>& pops) {
    const auto totals = SumPopulationByCulture(pops);
    std::uint64_t maximum = 0;
    std::vector<Id> result;
    for (const auto& item : totals) {
        if (item.second > maximum) {
            maximum = item.second;
            result.clear();
        }
        if (maximum && item.second == maximum) result.push_back(item.first);
    }
    return result;
}

inline std::vector<CoreGrant> Calculate(const std::vector<Country>& countries,
                                     const std::vector<Province>& provinces,
                                     bool technology_present) {
    std::unordered_map<Id, const Country*> by_id;
    std::unordered_map<Id, std::vector<Id>> by_culture;
    for (const auto& country : countries) {
        if (!country.id || !by_id.emplace(country.id, &country).second)
            throw std::invalid_argument("Invalid or duplicate country ID");
        std::set<Id> cultures(country.accepted_cultures.begin(),
                              country.accepted_cultures.end());
        if (cultures.count(0)) throw std::invalid_argument("Accepted culture ID is zero");
        if (country.primary_culture) cultures.insert(country.primary_culture);
        if (!country.exists || !country.great_power ||
            (technology_present && !country.technology_researched)) continue;
        for (Id culture : cultures) by_culture[culture].push_back(country.id);
    }

    std::unordered_set<Id> province_ids;
    std::vector<CoreGrant> result;
    // Ровно один проход по входным провинциям. Индекс не обходится целиком
    // для каждой провинции: выбирается только её крупнейшая культура.
    for (const auto& province : provinces) {
        if (!province.id || !province_ids.insert(province.id).second)
            throw std::invalid_argument("Invalid or duplicate province ID");
        if (province.dominant_culture && !*province.dominant_culture)
            throw std::invalid_argument("Dominant culture ID is zero");
        std::unordered_set<Id> cores;
        for (Id core : province.cores) {
            if (!by_id.count(core)) throw std::invalid_argument("Unknown core country ID");
            cores.insert(core);
        }
        if (!province.owner) continue;
        const auto owner = by_id.find(province.owner);
        if (owner == by_id.end() || !owner->second->exists)
            throw std::invalid_argument("Province owner does not exist");
        if (!province.dominant_culture) continue;
        const auto claimants = by_culture.find(*province.dominant_culture);
        if (claimants == by_culture.end()) continue;
        for (Id claimant : claimants->second) {
            if (claimant != province.owner && !cores.count(claimant))
                result.push_back({province.id, claimant});
        }
    }
    // Порядок unordered_map и адреса объектов не попадают в результат.
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace gpc
