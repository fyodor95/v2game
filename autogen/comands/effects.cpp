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

template <tTarget target>
tEffect<target> SetGlobalFlag(char const * flag)
{
    return Set("set_global_flag", flag);
}

template tEffect<tTarget::kCountry > SetGlobalFlag(char const *);
template tEffect<tTarget::kProvince> SetGlobalFlag(char const *);

template <tTarget target>
tEffect<target> RemoveGlobalFlag(char const * const flag)
{
    return Set("clr_global_flag", flag);
}

template tEffect<tTarget::kCountry > RemoveGlobalFlag(char const *);
template tEffect<tTarget::kProvince> RemoveGlobalFlag(char const *);

tEffect<tTarget::kCountry> AddBadboy(float const value)
{
    return Set("badboy", ToString(value));
}

tEffect<tTarget::kCountry> AddPrestige(int const value)
{
    return Set("prestige", std::to_string(value));
}

template <tTarget target>
tEffect<tTarget::kCountry> Effects(char const * const _for, tConditions<target> && limit, tEffects<target> && effects)
{
    tEffect<tTarget::kCountry> res(_for, std::move(CastToComands(effects)));

    if (!limit.empty())
        res.AddToBody({tComand("limit", std::move(CastToComands(limit)))}, 0u);

    return res;
}

tEffect<tTarget::kCountry> EffectsForAllCountries
(tConditions<tTarget::kCountry> && limit, tEffects<tTarget::kCountry> && effects)
{
    return Effects<tTarget::kCountry>("any_country", std::move(limit), std::move(effects));
}

tEffect<tTarget::kCountry> EffectsForAllPops(tCondition<tTarget::kPop> && limit, tEffects<tTarget::kPop> && effects)
{
    return Effects<tTarget::kPop>("any_pop", {limit}, std::move(effects));
}

tEffect<tTarget::kCountry> EffectsForOwnedProvinces(tConditions <tTarget::kProvince> && conditions,
                                                    tEffects    <tTarget::kProvince> && effects)
{
    return Effects<tTarget::kProvince>("any_owned", std::move(conditions), std::move(effects));
}

tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects)
{
    return tEffect<tTarget::kProvince>("owner", std::move(CastToComands(effects)));
}

tEffect<tTarget::kCountry> EffectsForRandomCountry
(tConditions<tTarget::kCountry> && limit, tEffects<tTarget::kCountry> && effects)
{
    return Effects<tTarget::kCountry>("random_country", std::move(limit), std::move(effects));
}

tEffect<tTarget::kPop> AddMilitancy(int const value)
{
    return Set("militancy", std::to_string(value));
}
