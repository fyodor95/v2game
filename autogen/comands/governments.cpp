#include "governments.h"

namespace nsGovernment
{

    template <>
    char const * ToString<tGovernment::proletarian_dictatorship>()
    {
        return "proletarian_dictatorship";
    }

    template <>
    char const * ToString<tGovernment::presidential_dictatorship>()
    {
        return "presidential_dictatorship";
    }

    template <>
    char const * ToString<tGovernment::bourgeois_dictatorship>()
    {
        return "bourgeois_dictatorship";
    }

    template <>
    char const * ToString<tGovernment::fascist_dictatorship>()
    {
        return "fascist_dictatorship";
    }

    template <>
    char const * ToString<tGovernment::absolute_monarchy>()
    {
        return "absolute_monarchy";
    }

    template <>
    char const * ToString<tGovernment::prussian_constitutionalism>()
    {
        return "prussian_constitutionalism";
    }

    template <>
    char const * ToString<tGovernment::hms_government>()
    {
        return "hms_government";
    }

    template <>
    char const * ToString<tGovernment::democracy>()
    {
        return "democracy";
    }

}
