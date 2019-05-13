#include "effects.h"

tEffect<tTarget::kCountry> AddPrestige(int const value)
{
    return Set("prestige", std::to_string(value));
}

tEffect<tTarget::kCountry> EffectsForAllPops(tCondition<tTarget::kPop> && limit, tEffects<tTarget::kPop> && effects)
{
    tEffect<tTarget::kCountry> res("any_pop", std::move(CastToComands(effects)));
    res.AddToBody({tComand("limit", tComands{std::move(limit)})}, 0u);
    return res;
}

tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects)
{
    return tEffect<tTarget::kProvince>("owner", std::move(CastToComands(effects)));
}

tEffect<tTarget::kPop> AddMilitancy(int const value)
{
    return Set("militancy", std::to_string(value));
}
