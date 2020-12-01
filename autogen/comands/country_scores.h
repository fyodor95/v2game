#pragma once

enum class tScoreType : uint16_t
{
      industrial_score
    , military_score
    , prestige
};

char const * ToString(tScoreType);
