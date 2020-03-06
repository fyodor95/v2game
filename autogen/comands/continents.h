#pragma once

enum class tContinent : uint16_t
{
    kEurope,
    kAsia,
    kAfrica,
    kNorthAmerica,
    kSouthAmerica,
    kOceania
};

char const * ToString(tContinent);
