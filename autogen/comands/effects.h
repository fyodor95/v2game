#pragma once

#include "conditions.h"
#include "cultures.h"
#include "flags.h"
#include "governments.h"
#include "reforms.h"
#include "target_comand.h"

COMAND(tEffect);

// Совершает один из поданных эффектов. Выбор равновероятен.
template <tTarget target>
tEffect<target> RandomEffectFrom(tEffects<target> &&);

template <tTarget target = tTarget::kCountry>
tEffect<target> SetGlobalFlag(char const * flag);

template <tTarget target = tTarget::kCountry>
tEffect<target> RemoveGlobalFlag(char const * flag);

// Делает культуру culture признанной в данной стране.
template <tCulture culture>
tEffect<tTarget::kCountry> AddAcceptedCulture() { return Set("add_accepted_culture", nsCulture::ToString<culture>()); }

// Добавляет value престижа данной стране.
tEffect<tTarget::kCountry> AddPrestige(int value);

// Выставляет реформу типа tReformType в значение level.
template <tGovernment government>
tEffect<tTarget::kCountry> ChangeGovernmentType()
{
    return Set("government", nsGovernment::ToString<government>());
}

// Выставляет реформу типа tReformType в значение level.
template <class tReformType, tReformType level>
tEffect<tTarget::kCountry> ChangeReformLevel()
{
    using namespace nsReforms;
    return Set(nsType::ToString<Type<tReformType>()>(), ToString<tReformType, level>());
}

// Все попы в данной стране, удовлетворяющие условию limit, получают эффекты effects.
tEffect<tTarget::kCountry> EffectsForAllPops(tCondition<tTarget::kPop> && limit, tEffects<tTarget::kPop> && effects);

// Держава tag получает эффекты effects.
template <tTag tag, tTarget target = tTarget::kCountry>
tEffect<target> EffectsForSpecifiedCountry(tEffects<tTarget::kCountry> && effects);

// Делает культуру culture основной для данной державы. Может использоваться для несуществующих государств.
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

template <tCountryFlag flag>
tEffect<tTarget::kCountry> RemoveFlag()
{
    return Set("clr_country_flag", nsCountryFlag::ToString<flag>());
}

template <tCountryFlag flag>
tEffect<tTarget::kCountry> SetFlag()
{
    return Set("set_country_flag", nsCountryFlag::ToString<flag>());
}

// Владелец данной провинции получает эффекты effects.
tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects);

// Делает данную провинцию национальной для державы tag.
template <tTag tag>
tEffect<tTarget::kProvince> MakeNationalFor()
{
    return Set("add_core", nsTag::ToString<tag>());
}

// Добавляет данному попу value агрессивности.
tEffect<tTarget::kPop> AddMilitancy(int value);

template <tTag tag, tTarget target>
tEffect<target> EffectsForSpecifiedCountry(tEffects<tTarget::kCountry> && effects)
{
    if constexpr (target == tTarget::kCountry)
        return tEffect<target>(nsTag::ToString<tag>(), std::move(CastToComands(effects)));
    else
        return tEffect<target>(EffectsForOwner({EffectsForSpecifiedCountry<tag>(std::move(effects))}));
}
