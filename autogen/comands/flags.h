#pragma once

enum class tCountryFlag
{
    primary_culture_must_be_changed
};

namespace nsCountryFlag
{
    template <tCountryFlag>
    char const * ToString();
}
