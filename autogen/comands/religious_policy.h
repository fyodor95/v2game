#pragma once

enum class tReligiousPolicy : uint16_t
{
      pro_atheism  // атеизм
    , secularized  // секул€ризаци€
    , pluralism    // плюрализм
    , moralism     // морализм
};

char const * ToString(tReligiousPolicy);
