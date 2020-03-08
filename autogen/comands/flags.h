#pragma once

enum class tCountryFlag : uint16_t
{
    justice_was_restored,
    primary_culture_must_be_changed
};

char const * ToString(tCountryFlag);
