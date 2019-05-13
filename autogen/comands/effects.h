#pragma once

#include "conditions.h"
#include "cultures.h"
#include "target_comand.h"

COMAND(tEffect);

// Делает культуру culture признанной в данной стране.
template <tCulture culture>
tEffect<tTarget::kCountry> AddAcceptedCulture() { return Set("add_accepted_culture", nsCulture::ToString<culture>()); }

// Добавляет value престижа данной стране.
tEffect<tTarget::kCountry> AddPrestige(int value);

// Все попы в данной стране, удовлетворяющие условию limit, получают эффекты effects.
tEffect<tTarget::kCountry> EffectsForAllPops(tCondition<tTarget::kPop> && limit, tEffects<tTarget::kPop> && effects);

// Делает культуру culture лосновной для данной державы. Может использоваться для несуществующих государств.
template <tCulture culture>
tEffect<tTarget::kCountry> MakePrimary()
{
    return Set("primary_culture", nsCulture::ToString<culture>());
}

// Удаляет признанную культуру culture в данной стране.
template <tCulture culture>
tEffect<tTarget::kCountry> RemoveAcceptedCulture()
{
    return Set("remove_accepted_culture", nsCulture::ToString<culture>());
}

// Владелец данной провинции получает эффекты effects.
tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects);

// Добавляет данному попу value агрессивности.
tEffect<tTarget::kPop> AddMilitancy(int value);
