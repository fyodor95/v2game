// Нативные проверки контракта, независимые от Victoria II и её ABI.
#include "calculation.h"
#include <iostream>
#include <random>
#include <string>

using namespace gpc;

static unsigned checks = 0;

static void Require(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}

template<class Exception, class Fn>
static void Throws(Fn fn, const char* description) {
    bool thrown = false;
    try { fn(); } catch (const Exception&) { thrown = true; }
    Require(thrown, description);
}

// Намеренно медленная независимая реализация: все страны x все провинции,
// без индекса культур, без helper производственного расчёта.
static std::vector<CoreGrant> Reference(const std::vector<Country>& countries,
                                      const std::vector<Province>& provinces,
                                      bool technology_present) {
    std::vector<CoreGrant> expected;
    for (const auto& c : countries) {
        if (!c.exists || !c.great_power ||
            (technology_present && !c.technology_researched)) continue;
        for (const auto& p : provinces) {
            if (!p.owner || p.owner == c.id || p.dominant_cultures.empty()) continue;
            bool accepted = false;
            for (Id dominant : p.dominant_cultures)
                accepted = accepted || c.primary_culture == dominant;
            if (!accepted) continue;
            bool core = false;
            for (Id tag : p.cores) core = core || tag == c.id;
            if (!core) expected.push_back({p.id, c.id});
        }
    }
    std::sort(expected.begin(), expected.end());
    return expected;
}

static void Functional() {
    const std::vector<Country> countries = {
        {3, true, true, true, 10, {10, 20, 20}},
        {1, true, true, false, 20, {30}},
        {2, true, false, true, 10, {}},
        {4, false, true, true, 10, {}},
        {5, true, true, true, 10, {}}
    };
    auto provinces = std::vector<Province>{
        {9, 2, {10}, {}},       // primary, два получателя
        {8, 2, {20}, {}},       // только accepted у 3, primary у 1 без технологии
        {7, 3, {10}, {}},       // своя для 3, чужая для 5
        {6, 2, {10}, {3, 3}},   // существующая корка
        {5, 0, {10}, {}},       // нет владельца
        {4, 2, {}, {}},
        {3, 2, {99}, {}},       // непризнанная
        {2, 2, {30}, {}},       // только accepted у 1, выдачи нет даже без tech gate
        {1, 1, {20}, {}}
    };
    const auto expected = std::vector<CoreGrant>{
        {6, 5}, {7, 5}, {9, 3}, {9, 5}
    };
    const auto actual = Calculate(countries, provinces, true);
    Require(actual == expected, "Exact primary-only/core/owner/GP result");
    const auto without_tech = Calculate(countries, provinces, false);
    Require(without_tech == std::vector<CoreGrant>({
        {6, 5}, {7, 5}, {8, 1}, {9, 3}, {9, 5}
    }), "Absent technology disables only technology gate");
    for (const auto& grant : actual)
        for (auto& p : provinces)
            if (p.id == grant.province) p.cores.push_back(grant.country);
    Require(Calculate(countries, provinces, true).empty(), "Second pass is empty");
    Require(Calculate({}, {}, true).empty(), "Empty world");

    Require(DominantCultures({{10, 40}, {20, 35}, {30, 25}}) ==
            std::vector<Id>{10}, "Plurality below fifty percent");
    Require(DominantCultures({{10, 20}, {20, 35}, {30, 25}, {10, 20}}) ==
            std::vector<Id>{10}, "Sum split POP, not largest individual POP");
    Require(DominantCultures({{10, 40}, {20, 30}, {30, 30}}) ==
            std::vector<Id>{10}, "Accepted cultures do not merge");
    Require(DominantCultures({{20, 20}, {10, 40}, {20, 20}}) ==
            std::vector<Id>({10, 20}), "Tie is returned without choosing a winner");
    Require(DominantCultures({{10, 0}}).empty(), "Zero population has no winner");
    Require(DominantCultures({}).empty(), "Empty population has no winner");
    const auto max = (std::numeric_limits<std::uint64_t>::max)();
    Require(SumPopulationByCulture({{10, max}}).at(10) == max,
            "Full uint64 population is preserved");
    Throws<std::overflow_error>([&] { SumPopulationByCulture({{10, max}, {10, 1}}); },
                                "Same culture overflow rejected");
    Throws<std::overflow_error>([&] { SumPopulationByCulture({{10, max}, {20, 1}}); },
                                "Total province overflow rejected");
    Throws<std::invalid_argument>([] { SumPopulationByCulture({{0, 10}}); },
                                  "Invalid POP culture rejected");
    Throws<std::invalid_argument>([&] {
        auto duplicate = countries; duplicate.push_back(countries.front());
        Calculate(duplicate, provinces, true);
    }, "Duplicate country rejected");
    Throws<std::invalid_argument>([&] {
        auto invalid = countries; invalid.front().id = 0;
        Calculate(invalid, {}, true);
    }, "Zero country ID rejected");
    auto ignored_accepted = countries;
    ignored_accepted.front().accepted_cultures = {0, 0, 10, 99};
    auto fresh = provinces;
    for (auto& province : fresh) province.cores.clear();
    Require(Calculate(ignored_accepted, fresh, true) == Calculate(countries, fresh, true),
            "Accepted culture data is neither used nor validated");
    Throws<std::invalid_argument>([&] {
        auto duplicate = provinces; duplicate.push_back(provinces.front());
        Calculate(countries, duplicate, true);
    }, "Duplicate province rejected");
    Throws<std::invalid_argument>([&] {
        Calculate(countries, {{1, 99, {10}, {}}}, true);
    }, "Unknown owner rejected");
    Throws<std::invalid_argument>([&] {
        Calculate(countries, {{1, 4, {10}, {}}}, true);
    }, "Extinct owner rejected");
    Throws<std::invalid_argument>([&] {
        Calculate(countries, {{1, 2, {10}, {99}}}, true);
    }, "Unknown core country rejected");
    Throws<std::invalid_argument>([&] {
        Calculate(countries, {{0, 2, {10}, {}}}, true);
    }, "Zero province ID rejected");
    Throws<std::invalid_argument>([&] {
        Calculate(countries, {{1, 2, {Id{0}}, {}}}, true);
    }, "Zero dominant culture rejected");
}

static void RandomWorlds() {
    std::mt19937 random(0x475043);
    for (unsigned iteration = 0; iteration < 2000; ++iteration) {
        std::vector<Country> countries;
        std::vector<Id> existing;
        for (Id id = 1; id <= 12; ++id) {
            const bool exists = id == 1 || random() % 4 != 0;
            Country c{id, exists, random() % 2 != 0, random() % 2 != 0,
                      static_cast<Id>(1 + random() % 7), {}};
            for (unsigned n = random() % 10; n; --n)
                c.accepted_cultures.push_back(static_cast<Id>(random() % 8));
            countries.push_back(c);
            if (exists) existing.push_back(id);
        }
        std::vector<Province> provinces;
        for (Id id = 1; id <= 30; ++id) {
            Province p{id, random() % 5 == 0 ? Id{0} : existing[random() % existing.size()],
                       {}, {}};
            for (unsigned n = random() % 5; n; --n)
                p.dominant_cultures.push_back(static_cast<Id>(1 + random() % 9));
            for (unsigned n = random() % 6; n; --n)
                p.cores.push_back(static_cast<Id>(1 + random() % 12));
            provinces.push_back(p);
        }
        const bool tech = random() % 2 != 0;
        const auto expected = Reference(countries, provinces, tech);
        Require(Calculate(countries, provinces, tech) == expected, "Reference world result");
        std::shuffle(countries.begin(), countries.end(), random);
        std::shuffle(provinces.begin(), provinces.end(), random);
        for (auto& c : countries)
            std::shuffle(c.accepted_cultures.begin(), c.accepted_cultures.end(), random);
        for (auto& p : provinces) {
            std::shuffle(p.cores.begin(), p.cores.end(), random);
            std::shuffle(p.dominant_cultures.begin(), p.dominant_cultures.end(), random);
        }
        Require(Calculate(countries, provinces, tech) == expected,
                "Stable result after arbitrary input permutation");
    }
}

static void AllLargestCultures() {
    const std::vector<Country> countries = {
        {1, true, true, true, 10, {20}},
        {2, true, true, true, 30, {20}},
        {3, true, true, true, 20, {10}},
        {4, true, true, true, 99, {}},
        {5, true, false, true, 99, {}}
    };
    auto provinces = std::vector<Province>{
        {1, 5, {20, 10, 20}, {}},
        {2, 5, {30, 10}, {}},
        {3, 5, {10, 20}, {1}},
        {4, 1, {10, 20}, {}},
        {5, 0, {10, 20}, {}},
        {6, 5, {}, {}},
        {7, 5, {99}, {}},
        {8, 5, {40, 50}, {}}
    };
    const auto expected = std::vector<CoreGrant>{
        {1, 1}, {1, 3},
        {2, 1}, {2, 2},
        {3, 3}, {4, 3}, {7, 4}
    };
    const auto grants = Calculate(countries, provinces, true);
    Require(grants == expected,
            "All tied cultures grant only to matching primary cultures");
    Require(DominantCultures({{30, 20}, {20, 40}, {10, 20}, {10, 20}}) ==
                std::vector<Id>({10, 20}),
            "Tied maxima exclude a smaller third culture");
    for (const auto& grant : grants)
        for (auto& province : provinces)
            if (province.id == grant.province) province.cores.push_back(grant.country);
    Require(Calculate(countries, provinces, true).empty(), "Tie grants remain idempotent");
    Throws<std::invalid_argument>([&] {
        Calculate(countries, {{9, 5, {10, 0, 20}, {}}}, true);
    }, "Zero in tied cultures rejected");
}

static void PrimaryChangePreservesOldCores() {
    std::vector<Country> countries = {
        {1, true, true, true, 10, {}},
        {2, true, true, true, 20, {10}},
        {3, true, false, true, 30, {}}
    };
    std::vector<Province> provinces = {{1, 3, {10}, {}}};
    const auto original = Calculate(countries, provinces, true);
    Require(original == std::vector<CoreGrant>({{1, 1}}),
            "Accepted-only country never receives initial core");
    provinces.front().cores.push_back(original.front().country);
    countries.front().primary_culture = 30;
    countries.front().accepted_cultures = {10};
    provinces.push_back({2, 3, {10}, {}});
    Require(Calculate(countries, provinces, true).empty(),
            "Old primary remaining accepted does not grant new cores");
    Require(provinces.front().cores == std::vector<Id>{1},
            "Previously granted core survives primary culture change");
}

int main() {
    try {
        Functional();
        AllLargestCultures();
        PrimaryChangePreservesOldCores();
        RandomWorlds();
        std::cout << "PASS " << checks << " checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
