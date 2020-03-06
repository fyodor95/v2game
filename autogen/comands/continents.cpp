#include "continents.h"

using tStorage = std::array<char const *, 6u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;
    res[static_cast<uint16_t>(tContinent::kEurope)] = "europe";
    res[static_cast<uint16_t>(tContinent::kAsia)] = "asia";
    res[static_cast<uint16_t>(tContinent::kAfrica)] = "africa";
    res[static_cast<uint16_t>(tContinent::kNorthAmerica)] = "north_america";
    res[static_cast<uint16_t>(tContinent::kSouthAmerica)] = "south_america";
    res[static_cast<uint16_t>(tContinent::kOceania)] = "oceania";
    return res;
}

char const * ToString(tContinent const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
