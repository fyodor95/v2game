#pragma once

enum class tContinent : uint8_t
{
    kEurope,
    kAsia,
    kAfrica,
    kNorthAmerica,
    kSouthAmerica,
    kOceania
};

namespace nsContinent
{
    template <tContinent>
    char const * ToString();
}
