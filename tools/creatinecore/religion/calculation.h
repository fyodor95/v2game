#pragma once
#include <cstdint>
#include <vector>

namespace state_religion {
struct Population {
    std::uint32_t owner;
    std::uint32_t culture;
    std::uint32_t religion;
    std::uint64_t people;
};
struct Selection {
    bool found;
    std::uint32_t religion;
    std::uint64_t people;
};
// Индексы религий задаются порядком определений движка, а не порядком POP.
Selection Choose(std::uint32_t country, std::uint32_t primary,
                 std::uint32_t current, const std::vector<Population>& pops);
}
