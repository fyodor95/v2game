#include "conditions.h"

template <tTarget target>
tCondition<target> AverageMilitancy(float const num)
{
    static_assert(target != tTarget::kPop);
    return Set("average_militancy", ToString(num));
}

template tCondition<tTarget::kCountry > AverageMilitancy(float);
template tCondition<tTarget::kProvince> AverageMilitancy(float);

template <tTarget target>
tCondition<target> HasGlobalFlag(char const * const flag)
{
    static_assert(target != tTarget::kPop);
    return Set("has_global_flag", flag);
}

template tCondition<tTarget::kCountry > HasGlobalFlag(char const *);
template tCondition<tTarget::kProvince> HasGlobalFlag(char const *);

tCondition<tTarget::kCountry> AllProvinces(tCondition<tTarget::kProvince> && condition)
{
    return Not(AnyOwnedProvince(Not(std::move(condition))));
}

tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition)
{
    return tCondition<tTarget::kCountry>("any_owned_province", tComands{std::move(condition)});
}

tCondition<tTarget::kCountry> CitizenshipPolicyIs(tCitizenshipPolicy const policy)
{
    return Set("citizenship_policy", ToString(policy));
}

tCondition<tTarget::kCountry> Civilized(tYesOrNo const flag)
{
    return Set("civilized", YesOrNo(flag));
}

tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions)
{
    return tCondition<tTarget::kCountry>("capital_scope", std::move(CastToComands(conditions)));
}

tCondition<tTarget::kCountry> HasDefaultPrimaryCulture()
{
    return IsPrimary(kDefaultCulture);
}

tCondition<tTarget::kCountry> HasNeighborSuchThat(tConditions<tTarget::kCountry> && conditions)
{
    return {"any_neighbor_country", std::move(CastToComands(conditions))};
}

tCondition<tTarget::kCountry> HasProvince(tConditions<tTarget::kProvince> && conditions)
{
    return {"any_owned_province", std::move(CastToComands(conditions))};
}

tCondition<tTarget::kCountry> HasTechnology(tTechnology const & tech)
{
    return Set(std::string(tech.ToString()), "1");
}

tCondition<tTarget::kCountry> HasVassal(tTag const tag)
{
    return Set("is_our_vassal", ToString(tag));
}

tCondition<tTarget::kCountry> IsAI(tYesOrNo const flag)
{
    return Set("ai", YesOrNo(flag));
}

tCondition<tTarget::kCountry> IsDominion()
{
    tConditions<tTarget::kCountry> body;
    body.reserve(kDominions.size());
    for (tTag const tag : kDominions)
        body.push_back(TagIs(tag));
    return Or(std::move(body));
}

tCondition<tTarget::kCountry> IsIndependant(tYesOrNo const flag)
{
    return Set("is_independant", YesOrNo(flag));
}

tCondition<tTarget::kCountry> IsMobilised(tYesOrNo const flag)
{
    return Set("is_mobilised", YesOrNo(flag));
}

tCondition<tTarget::kCountry> IsPartOfSphere(tYesOrNo const flag)
{
    return Set("part_of_sphere", YesOrNo(flag));
}

tCondition<tTarget::kCountry> War(tYesOrNo const flag)
{
    return Set("war", YesOrNo(flag));
}

tCondition<tTarget::kProvince> ConditionsForOwner(tConditions<tTarget::kCountry> && conditions)
{
    return tCondition<tTarget::kProvince>("owner", std::move(CastToComands(conditions)));
}

tCondition<tTarget::kProvince> ControlledByRebels(tYesOrNo const flag)
{
    return Set("controlled_by_rebels", YesOrNo(flag));
}

tCondition<tTarget::kProvince> Empty(tYesOrNo const flag)
{
    return Set("empty", YesOrNo(flag));
}

tCondition<tTarget::kProvince> ExistsPopWithMilitancy(float const num)
{
    return Set("pop_militancy", ToString(num));
}

tCondition<tTarget::kProvince> HasNeighborProvince(tConditions<tTarget::kProvince> && conditions)
{
    return {"any_neighbor_province", std::move(CastToComands(conditions))};
}

tCondition<tTarget::kProvince> HasPop(tCondition<tTarget::kPop> && condition)
{
    return {"any_pop", {std::move(condition)}};
}

tCondition<tTarget::kProvince> IsInNewWorld()
{
    return Or<tTarget::kProvince>({IsInContinent(tContinent::kNorthAmerica),
                                   IsInContinent(tContinent::kSouthAmerica),
                                   IsInContinent(tContinent::kOceania     )});
}

tCondition<tTarget::kProvince> MajorityCultureIsAccepted(tYesOrNo const flag)
{
    return Set("is_accepted_culture", YesOrNo(flag));
}

tCondition<tTarget::kProvince> MajorityCultureIsPrimary(tYesOrNo const flag)
{
    return Set("is_primary_culture", YesOrNo(flag));
}

tCondition<tTarget::kProvince> PopulationIsGreaterOrEqual(uint const num)
{
    return Set("total_pops", std::to_string(num));
}

tCondition<tTarget::kPop> HasAcceptedCulture(tYesOrNo const flag)
{
    return Set("is_accepted_culture", YesOrNo(flag));
}

tCondition<tTarget::kPop> HasPrimaryCulture(tYesOrNo const flag)
{
    return Set("is_primary_culture", YesOrNo(flag));
}
