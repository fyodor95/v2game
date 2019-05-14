#include "continents.h"

namespace nsContinent
{

    template <>
    char const * ToString<tContinent::kEurope>()
    {
        return "europe";
    }

    template <>
    char const * ToString<tContinent::kAsia>()
    {
        return "asia";
    }

    template <>
    char const * ToString<tContinent::kAfrica>()
    {
        return "africa";
    }

    template <>
    char const * ToString<tContinent::kNorthAmerica>()
    {
        return "north_america";
    }

    template <>
    char const * ToString<tContinent::kSouthAmerica>()
    {
        return "south_america";
    }

    template <>
    char const * ToString<tContinent::kOceania>()
    {
        return "oceania";
    }

}
