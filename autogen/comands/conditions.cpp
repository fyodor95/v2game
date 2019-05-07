#include "conditions.h"

tCondition<tTarget::kCountry> AcceptedCulture(char const * const culture)
{
    return Set("accepted_culture", culture);
}

tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition)
{
    return tCondition<tTarget::kCountry>("any_owned_province", tComands{std::move(condition)});
}

tCondition<tTarget::kProvince> MajorityCultureIs(char const * const culture)
{
    return Set("culture", culture);
}
