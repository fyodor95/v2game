#pragma once

#include "cultures.h"
#include "target_comand.h"

COMAND(tEffect);

// Делает культуру culture признанной в данной стране.
template <tCulture culture>
tEffect<tTarget::kCountry> AddAcceptedCulture() { return Set("add_accepted_culture", nsCulture::ToString<culture>()); }

// Добавляет value престижа данной стране.
tEffect<tTarget::kCountry> AddPrestige(int value);

// Удаляет признанную культуру culture в данной стране.
template <tCulture culture>
tEffect<tTarget::kCountry> RemoveAcceptedCulture()
{
    return Set("remove_accepted_culture", nsCulture::ToString<culture>());
}

// Владелец данной провинции получает эффекты effects.
tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects);
