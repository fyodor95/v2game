#include "effects.h"

tEffect<tTarget::kCountry> RemoveAcceptedCulture(char const * const culture)
{
    return Set("remove_accepted_culture", culture);
}
