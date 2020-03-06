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
inline tEffect<tTarget::kCountry> AddAcceptedCulture(tCulture culture)
{ return Set("add_accepted_culture", ToString(culture)); }

tEffect<tTarget::kCountry> AddBadboy(float value);

// Добавляет value престижа данной стране.
tEffect<tTarget::kCountry> AddPrestige(int value);

// Выставляет реформу типа tReformType в значение level.
inline tEffect<tTarget::kCountry> ChangeGovernmentType(tGovernment government)
{
    return Set("government", ToString(government));
}

// Выставляет реформу типа tReformType в значение level.
template <class tReformType, tReformType level>
tEffect<tTarget::kCountry> ChangeReformLevel()
{
    using namespace nsReforms;
    return Set(nsType::ToString<Type<tReformType>()>(), ToString<tReformType, level>());
}

// Увеличивает отношения между данной державой и державой tag на dif.
tEffect<tTarget::kCountry> ChangeRelation(tTag tag, int dif);

// Все попы в данной стране, удовлетворяющие условию limit, получают эффекты effects.
tEffect<tTarget::kCountry> EffectsForAllPops(tCondition<tTarget::kPop> && limit, tEffects<tTarget::kPop> && effects);

// Все провинции, которыми владеет данная держава и которые удовлетворяют всем условиям limit,
// получают эффекты effects.
tEffect<tTarget::kCountry> EffectsForOwnedProvinces(tConditions <tTarget::kProvince> && limit,
                                                    tEffects    <tTarget::kProvince> && effects);

// Держава tag получает эффекты effects.
template <tTarget target = tTarget::kCountry>
tEffect<target> EffectsForSpecifiedCountry(tTag tag, tEffects<tTarget::kCountry> && effects);

// В державе tag произойдёт событие с номером eventID.
tEffect<tTarget::kCountry> EventInCountry(tTag tag, uint eventID);

// Присоединяет державу tag к данной державе
inline tEffect<tTarget::kCountry> Inherit(tTag tag) { return Set("inherit", ToString(tag)); }

// Делает культуру culture основной для данной державы. Может использоваться для несуществующих государств.
inline tEffect<tTarget::kCountry> MakePrimary(tCulture culture)
{
    return Set("primary_culture", ToString(culture));
}

// Удаляет признанную культуру culture в данной стране.
inline tEffect<tTarget::kCountry> RemoveAcceptedCulture(tCulture culture)
{
    return Set("remove_accepted_culture", ToString(culture));
}

inline tEffect<tTarget::kCountry> RemoveFlag(tCountryFlag flag)
{
    return Set("clr_country_flag", ToString(flag));
}

inline tEffect<tTarget::kCountry> SetFlag(tCountryFlag flag)
{
    return Set("set_country_flag", ToString(flag));
}

// Владельцем данной провинции становится держава tag.
inline tEffect<tTarget::kProvince> ChangeOwner(tTag tag) { return Set("secede_province", ToString(tag)); }

// Владелец данной провинции получает эффекты effects.
tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects);

// Делает данную провинцию национальной для державы tag. Тэгом не может быть kFrom.
tEffect<tTarget::kProvince> MakeNationalFor(tTag tag, tYesOrNo flag = _Yes);

// Добавляет данному попу value агрессивности.
tEffect<tTarget::kPop> AddMilitancy(int value);

template <tTarget target>
tEffect<target> EffectsForSpecifiedCountry(tTag tag, tEffects<tTarget::kCountry> && effects)
{
    if constexpr (target == tTarget::kCountry)
        return tEffect<target>(ToString(tag), std::move(CastToComands(effects)));
    else
        return tEffect<target>(EffectsForOwner({EffectsForSpecifiedCountry(tag, std::move(effects))}));
}

inline tEffect<tTarget::kCountry> ChangeRelation(tTag tag, int const dif)
{
    assertion(-400 <= dif && dif <= 400);
    return tEffect<tTarget::kCountry>("relation",
                                      {Set("who", ToString(tag)), Set("value", std::to_string(dif))});
}

inline tEffect<tTarget::kCountry> EventInCountry(tTag tag, uint const eventID)
{
    return EffectsForSpecifiedCountry
    (
        tag,
        {
            tEffect<tTarget::kCountry>("country_event = { id=" + std::to_string(eventID) + " days=0 }")
        }
    );
}

inline tEffect<tTarget::kProvince> MakeNationalFor(tTag tag, tYesOrNo const flag)
{
    assertion(flag || tag != tTag::kFrom);
    return Set(flag ? "add_core" : "remove_core", ToString(tag));
}
