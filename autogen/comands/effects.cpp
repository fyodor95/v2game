#include "effects.h"

template <tTarget target>
tEffect<target> RandomEffectFrom(tEffects<target> && effects)
{
    static_assert(target != tTarget::kPop);
    for (tEffect<target> & effect : effects)
        effect = tEffect<target>("1", {std::move(effect)});
    return {"random_list", std::move(CastToComands(effects))};
}

template tEffect<tTarget::kCountry > RandomEffectFrom(tEffects<tTarget::kCountry > &&);
template tEffect<tTarget::kProvince> RandomEffectFrom(tEffects<tTarget::kProvince> &&);

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
