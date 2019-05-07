#include "conditions.h"

tCondition<tTarget::kCountry> AcceptedCulture(char const * const culture)
{
    return Set("accepted_culture", culture);
}

tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition)
{
    return tCondition<tTarget::kCountry>("any_owned_province", tComands{std::move(condition)});
}

tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions)
{
    return tCondition<tTarget::kCountry>("capital_scope", std::move(CastToComands(conditions)));
}

tCondition<tTarget::kProvince> ConditionsForOwner(tConditions<tTarget::kCountry> && conditions)
{
    return tCondition<tTarget::kProvince>("owner", std::move(CastToComands(conditions)));
}

tCondition<tTarget::kProvince> MajorityCultureIs(char const * const culture)
{
    return Set("culture", culture);
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

tCondition<tTarget::kProvince> MilitancyIsGreaterOrEqual(float const num)
{
    return Set("pop_militancy", ToString(num));
}
