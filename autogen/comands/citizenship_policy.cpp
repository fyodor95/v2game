#include "citizenship_policy.h"

namespace nsCitizenshipPolicy
{

    template <>
    char const * ToString<tCitizenshipPolicy::kResidency>()
    {
        return "residency";
    }

    template <>
    char const * ToString<tCitizenshipPolicy::kLimited>()
    {
        return "limited_citizenship";
    }

    template <>
    char const * ToString<tCitizenshipPolicy::kFull>()
    {
        return "full_citizenship";
    }

}
