#include "calculation.h"
#include <limits>
#include <map>
#include <stdexcept>

namespace state_religion {
Selection Choose(std::uint32_t country, std::uint32_t primary,
                 std::uint32_t current, const std::vector<Population>& pops) {
    if (!country || !primary)
        throw std::invalid_argument("country/primary identity is absent");
    std::map<std::uint32_t, std::uint64_t> counts;
    for (const auto& pop : pops) {
        if (pop.owner != country || pop.culture != primary || !pop.people)
            continue;
        if (!pop.religion)
            throw std::invalid_argument("POP religion identity is absent");
        auto& count = counts[pop.religion];
        if (pop.people > (std::numeric_limits<std::uint64_t>::max)() - count)
            throw std::overflow_error("religion population overflow");
        count += pop.people;
    }
    Selection result{false, current, 0};
    for (const auto& item : counts) {
        if (!result.found || item.second > result.people ||
            (item.second == result.people && item.first == current))
            result = {true, item.first, item.second};
    }
    return result;
}
}
