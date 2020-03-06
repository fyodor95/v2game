#pragma once

enum class tWarPolicy : uint16_t
{
      jingoism       // шовинизм
    , pro_military   // милитаризм
    , anti_military  // антимилитаризм
    , pacifism       // пацифизм
};

char const * ToString(tWarPolicy);
