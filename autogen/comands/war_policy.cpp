#include "war_policy.h"

using tStorage = std::array<char const *, 4u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;
    res[static_cast<uint16_t>(tWarPolicy::jingoism)] = "jingoism";
    res[static_cast<uint16_t>(tWarPolicy::pro_military)] = "pro_military";
    res[static_cast<uint16_t>(tWarPolicy::anti_military)] = "anti_military";
    res[static_cast<uint16_t>(tWarPolicy::pacifism)] = "pacifism";
    return res;
}

char const * ToString(tWarPolicy const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
