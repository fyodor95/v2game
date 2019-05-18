#include "pop_types.h"

namespace nsPopType
{

    template <>
    char const * ToString<tPopType::aristocrats>()
    {
        return "aristocrats";
    }

    template <>
    char const * ToString<tPopType::artisans>()
    {
        return "artisans";
    }

    template <>
    char const * ToString<tPopType::bureaucrats>()
    {
        return "bureaucrats";
    }

    template <>
    char const * ToString<tPopType::capitalists>()
    {
        return "capitalists";
    }

    template <>
    char const * ToString<tPopType::clergymen>()
    {
        return "clergymen";
    }

    template <>
    char const * ToString<tPopType::clerks>()
    {
        return "clerks";
    }

    template <>
    char const * ToString<tPopType::craftsmen>()
    {
        return "craftsmen";
    }

    template <>
    char const * ToString<tPopType::farmers>()
    {
        return "farmers";
    }

    template <>
    char const * ToString<tPopType::labourers>()
    {
        return "labourers";
    }

    template <>
    char const * ToString<tPopType::officers>()
    {
        return "officers";
    }

    template <>
    char const * ToString<tPopType::slaves>()
    {
        return "slaves";
    }

    template <>
    char const * ToString<tPopType::soldiers>()
    {
        return "soldiers";
    }

}
