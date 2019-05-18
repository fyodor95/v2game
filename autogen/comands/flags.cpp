#include "flags.h"

namespace nsCountryFlag
{
    template <>
    char const * ToString<tCountryFlag::primary_culture_must_be_changed>()
    {
        return "primary_culture_must_be_changed";
    }
}
