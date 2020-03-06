#include "pop_types.h"

using tStorage = std::array<char const *, 12u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;
    res[static_cast<uint16_t>(tPopType::aristocrats)] = "aristocrats";
    res[static_cast<uint16_t>(tPopType::artisans)] = "artisans";
    res[static_cast<uint16_t>(tPopType::bureaucrats)] = "bureaucrats";
    res[static_cast<uint16_t>(tPopType::capitalists)] = "capitalists";
    res[static_cast<uint16_t>(tPopType::clergymen)] = "clergymen";
    res[static_cast<uint16_t>(tPopType::clerks)] = "clerks";
    res[static_cast<uint16_t>(tPopType::craftsmen)] = "craftsmen";
    res[static_cast<uint16_t>(tPopType::farmers)] = "farmers";
    res[static_cast<uint16_t>(tPopType::labourers)] = "labourers";
    res[static_cast<uint16_t>(tPopType::officers)] = "officers";
    res[static_cast<uint16_t>(tPopType::slaves)] = "slaves";
    res[static_cast<uint16_t>(tPopType::soldiers)] = "soldiers";
    return res;
}

char const * ToString(tPopType const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
