#include "flags.h"

using tStorage = std::array<char const *, 2u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;
    res[static_cast<uint16_t>(tCountryFlag::justice_was_restored            )] = "justice_was_restored"            ;
    res[static_cast<uint16_t>(tCountryFlag::primary_culture_must_be_changed )] = "primary_culture_must_be_changed" ;
    return res;
}

char const * ToString(tCountryFlag const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
