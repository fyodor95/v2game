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

tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions)
{
    return tCondition<tTarget::kCountry>("capital_scope", std::move(CastToComands(conditions)));
}

tCondition<tTarget::kCountry> HasDefaultPrimaryCulture()
{
    return IsPrimary<tCulture::british>();
}

template <size_t i = 0u>
void AddTag(tConditions<tTarget::kCountry> & conditions)
{
    conditions.push_back(TagIs<Dominions[i]>());
    AddTag<i + 1u>(conditions);
}

template <>
void AddTag<Dominions.size()>(tConditions<tTarget::kCountry> &)
{
}

tCondition<tTarget::kCountry> IsDominion()
{
    tConditions<tTarget::kCountry> body;
    body.reserve(Dominions.size());
    AddTag(body);
    return Or(std::move(body));
}

tCondition<tTarget::kProvince> ConditionsForOwner(tConditions<tTarget::kCountry> && conditions)
{
    return tCondition<tTarget::kProvince>("owner", std::move(CastToComands(conditions)));
}

tCondition<tTarget::kProvince> ExistsPopWithMilitancy(float const num)
{
    return Set("pop_militancy", ToString(num));
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

tCondition<tTarget::kPop> IsAcceptedPop(tYesOrNo const flag)
{
    return Set("is_accepted_culture", YesOrNo(flag));
}

tCondition<tTarget::kPop> IsPrimaryPop(tYesOrNo const flag)
{
    return Set("is_primary_culture", YesOrNo(flag));
}
