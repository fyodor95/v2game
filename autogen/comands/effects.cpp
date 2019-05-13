#include "effects.h"

tEffect<tTarget::kCountry> AddPrestige(int const value)
{
    return Set("prestige", std::to_string(value));
}

tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects)
{
    return tEffect<tTarget::kProvince>("owner", std::move(CastToComands(effects)));
}
