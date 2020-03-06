#include "citizenship_policy.h"

using tStorage = std::array<char const *, 3u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;
    res[static_cast<uint16_t>(tCitizenshipPolicy::kResidency)] = "residency";
    res[static_cast<uint16_t>(tCitizenshipPolicy::kLimited)] = "limited_citizenship";
    res[static_cast<uint16_t>(tCitizenshipPolicy::kFull)] = "full_citizenship";
    return res;
}

char const * ToString(tCitizenshipPolicy const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
