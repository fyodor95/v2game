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

template <tContinent>
char const * ToString();
