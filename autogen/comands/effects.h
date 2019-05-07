#pragma once

#include "target_comand.h"

COMAND(tEffect);

// Удаляет признанную культуру culture в данной стране.
tEffect<tTarget::kCountry> RemoveAcceptedCulture(char const * culture);

// Владелец данной провинции получает эффекты effects.
tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects);
