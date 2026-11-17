#include "calculation.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
    using namespace state_religion;
    // Чужая культура и население других владельцев не влияют на выбор.
    std::vector<Population> pops{{1, 2, 1, 10}, {1, 2, 1, 10},
        {1, 2, 2, 100}, {1, 3, 1, 10000}, {2, 2, 1, 10000}};
    auto result = Choose(1, 2, 1, pops);
    assert(result.found && result.religion == 2 && result.people == 100);
    assert(Choose(1, 2, result.religion, pops).religion == 2);
    std::reverse(pops.begin(), pops.end());
    assert(Choose(1, 2, 1, pops).religion == 2);
    pops = {{1, 2, 9, 100}, {1, 2, 2, 60}, {1, 2, 2, 40}};
    assert(Choose(1, 2, 9, pops).religion == 9);
    assert(Choose(1, 2, 2, pops).religion == 2);
    assert(Choose(1, 2, 7, pops).religion == 2);
    std::reverse(pops.begin(), pops.end());
    assert(Choose(1, 2, 7, pops).religion == 2);
    result = Choose(1, 2, 9, {{1, 3, 2, 200}, {1, 2, 0, 0}});
    assert(!result.found && result.religion == 9 && !result.people);
    result = Choose(1, 2, 9, {});
    assert(!result.found && result.religion == 9);
    assert(Choose(1, 2, 9, {{1, 2, 2, 1}}).religion == 2);
    assert(Choose(1, 2, 9, {{1, 2, 2, 3000000000ULL},
        {1, 2, 2, 3000000000ULL}}).people == 6000000000ULL);
    bool overflow = false;
    try { Choose(1, 2, 9, {{1, 2, 2, (std::numeric_limits<std::uint64_t>::max)()},
                         {1, 2, 2, 1}}); }
    catch (const std::overflow_error&) { overflow = true; }
    assert(overflow);
    bool invalid = false;
    try { Choose(1, 2, 9, {{1, 2, 0, 1}}); }
    catch (const std::invalid_argument&) { invalid = true; }
    assert(invalid);
    std::cout << "state religion calculation: PASS\n";
}
