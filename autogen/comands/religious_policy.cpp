#include "religious_policy.h"

using tStorage = std::array<char const *, 4u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;

    res[static_cast<uint16_t>(tReligiousPolicy::pro_atheism )] = "pro_atheism" ;
    res[static_cast<uint16_t>(tReligiousPolicy::secularized )] = "secularized" ;
    res[static_cast<uint16_t>(tReligiousPolicy::pluralism   )] = "pluralism"   ;
    res[static_cast<uint16_t>(tReligiousPolicy::moralism    )] = "moralism"    ;

    return res;
}

char const * ToString(tReligiousPolicy const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
