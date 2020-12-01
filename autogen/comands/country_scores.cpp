#include "country_scores.h"

using tStorage = std::array<char const *, 3u>;

static SINGLETON(Storage, tStorage)
{
    tStorage res;

    res[static_cast<uint16_t>(tScoreType::industrial_score )] = "industrial_score" ;
    res[static_cast<uint16_t>(tScoreType::military_score   )] = "military_score"   ;
    res[static_cast<uint16_t>(tScoreType::prestige         )] = "prestige"         ;

    return res;
}

char const * ToString(tScoreType const input)
{
    return Storage()[static_cast<uint16_t>(input)];
}
