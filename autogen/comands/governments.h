#pragma once

enum class tGovernment : uint8_t
{
      proletarian_dictatorship
    , presidential_dictatorship
    , bourgeois_dictatorship
    , fascist_dictatorship
    , absolute_monarchy
    , prussian_constitutionalism
    , hms_government
    , democracy
};

namespace nsGovernment
{
    template <tGovernment>
    char const * ToString();
}
