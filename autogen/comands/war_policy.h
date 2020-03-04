#pragma once

enum class tWarPolicy : uint8_t
{
      jingoism       // шовинизм
    , pro_military   // милитаризм
    , anti_military  // антимилитаризм
    , pacifism       // пацифизм
};

namespace nsWarPolicy
{
    template <tWarPolicy>
    char const * ToString();
}
