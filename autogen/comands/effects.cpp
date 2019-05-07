#include "effects.h"

tEffect<tTarget::kCountry> RemoveAcceptedCulture(char const * const culture)
{
    return Set("remove_accepted_culture", culture);
}

tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects)
{
    return tEffect<tTarget::kProvince>("owner", std::move(CastToComands(effects)));
}
