#include "governments.h"

using tStorage = std::array<char const *, 8u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;
    res[static_cast<uint16_t>(tGovernment::proletarian_dictatorship)] = "proletarian_dictatorship";
    res[static_cast<uint16_t>(tGovernment::presidential_dictatorship)] = "presidential_dictatorship";
    res[static_cast<uint16_t>(tGovernment::bourgeois_dictatorship)] = "bourgeois_dictatorship";
    res[static_cast<uint16_t>(tGovernment::fascist_dictatorship)] = "fascist_dictatorship";
    res[static_cast<uint16_t>(tGovernment::absolute_monarchy)] = "absolute_monarchy";
    res[static_cast<uint16_t>(tGovernment::prussian_constitutionalism)] = "prussian_constitutionalism";
    res[static_cast<uint16_t>(tGovernment::hms_government)] = "hms_government";
    res[static_cast<uint16_t>(tGovernment::democracy)] = "democracy";
    return res;
}

char const * ToString(tGovernment const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
