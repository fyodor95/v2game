#include "conditions.h"

template <tTarget target>
tCondition<target> AverageMilitancy(float const num)
{
    static_assert(target != tTarget::kPop);
    return Set("average_militancy", ToString(num));
}

template tCondition<tTarget::kCountry > AverageMilitancy(float);
template tCondition<tTarget::kProvince> AverageMilitancy(float);

tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition)
{
    return tCondition<tTarget::kCountry>("any_owned_province", tComands{std::move(condition)});
}

tCondition<tTarget::kCountry> Civilized(tYesOrNo const flag)
{
    return Set("civilized", YesOrNo(flag));
}

tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions)
{
    return tCondition<tTarget::kCountry>("capital_scope", std::move(CastToComands(conditions)));
}

tCondition<tTarget::kCountry> Exists(tYesOrNo const flag)
{
    return Set("exists", YesOrNo(flag));
}

tCondition<tTarget::kCountry> HasDefaultPrimaryCulture()
{
    return IsPrimary<kDefaultCulture>();
}

tCondition<tTarget::kCountry> HasProvince(tConditions<tTarget::kProvince> && conditions)
{
    return {"any_owned_province", std::move(CastToComands(conditions))};
}

template <size_t i = 0u>
void AddTag(tConditions<tTarget::kCountry> & conditions)
{
    conditions.push_back(TagIs<kDominions[i]>());
    AddTag<i + 1u>(conditions);
}

template <>
void AddTag<kDominions.size()>(tConditions<tTarget::kCountry> &)
{
}

tCondition<tTarget::kCountry> IsDominion()
{
    tConditions<tTarget::kCountry> body;
    body.reserve(kDominions.size());
    AddTag(body);
    return Or(std::move(body));
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
    return Or<tTarget::kProvince>({IsInContinent<tContinent::kNorthAmerica>(),
                                   IsInContinent<tContinent::kSouthAmerica>(),
                                   IsInContinent<tContinent::kOceania     >()});
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
