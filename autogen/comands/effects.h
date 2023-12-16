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

// Меняет тип правительства в стране на government.
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

// Все страны, удовлетворяющие условиям limit, получают эффекты effects. Эта функция работает по-разному в событиях и
// решениях. В решениях эффекты распространяются лишь на существующие на данный момент страны за исключением страны,
// для которой эта функция вызывается. В событиях эффекты воистину распространяются на все страны.
tEffect<tTarget::kCountry> EffectsForAllCountries
(tConditions<tTarget::kCountry> && limit, tEffects<tTarget::kCountry> && effects);

// Все попы в данной стране, удовлетворяющие условию limit, получают эффекты effects.
tEffect<tTarget::kCountry> EffectsForAllPops(tCondition<tTarget::kPop> && limit, tEffects<tTarget::kPop> && effects);

// Все провинции, которыми владеет данная держава и которые удовлетворяют всем условиям limit,
// получают эффекты effects.
tEffect<tTarget::kCountry> EffectsForOwnedProvinces(tConditions <tTarget::kProvince> && limit,
                                                    tEffects    <tTarget::kProvince> && effects);

// Случайная держава, удовлетворяющая условиям limit, получает эффекты effects.
tEffect<tTarget::kCountry> EffectsForRandomCountry
(tConditions<tTarget::kCountry> && limit, tEffects<tTarget::kCountry> && effects);

// Держава tag получает эффекты effects.
template <tTarget target = tTarget::kCountry>
tEffect<target> EffectsForSpecifiedCountry(tTag tag, tEffects<tTarget::kCountry> && effects);

// Провинция provID получает эффекты effects.
template <tTarget target = tTarget::kCountry>
tEffect<target> EffectsForSpecifiedProvince(uint provID, tEffects<tTarget::kProvince> && effects);

// В державе tag произойдёт событие с номером eventID. Если tag == tTag::kInvelidTag, событие произойдёт в державе, для
// которой эта функция вызывается.
tEffect<tTarget::kCountry> EventInCountry(uint eventID, tTag tag = tTag::kInvalidTag);

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

inline tEffect<tTarget::kCountry> RemoveCountryModifier(char const * modifierName)
{
    return Set("remove_country_modifier", modifierName);
}

inline tEffect<tTarget::kCountry> RemoveFlag(tCountryFlag flag)
{
    return Set("clr_country_flag", ToString(flag));
}

inline tEffect<tTarget::kCountry> SetFlag(tCountryFlag flag)
{
    return Set("set_country_flag", ToString(flag));
}

// Игрок, управляющий данной державой, начинает управлять державой tag.
inline tEffect<tTarget::kCountry> SwitchTo(tTag tag) { return Set("change_tag_no_core_switch", ToString(tag)); }

// Владельцем данной провинции становится держава tag.
inline tEffect<tTarget::kProvince> ChangeOwner(tTag tag) { return Set("secede_province", ToString(tag)); }

// Владелец данной провинции получает эффекты effects.
tEffect<tTarget::kProvince> EffectsForOwner(tEffects<tTarget::kCountry> && effects);

// Делает данную провинцию национальной для державы tag. Тэгом не может быть kFrom.
tEffect<tTarget::kProvince> MakeNationalFor(tTag tag, tYesOrNo flag = _Yes);

inline tEffect<tTarget::kProvince> RemoveProvinceModifier(char const * modifierName)
{
    return Set("remove_province_modifier", modifierName);
}

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

template <tTarget target>
tEffect<target> EffectsForSpecifiedProvince(uint provID, tEffects<tTarget::kProvince> && effects)
{
    return tEffect<target>(std::to_string(provID), std::move(CastToComands(effects)));
}

inline tEffect<tTarget::kCountry> ChangeRelation(tTag tag, int const dif)
{
    assertion(-400 <= dif && dif <= 400);
    return tEffect<tTarget::kCountry>("relation",
                                      {Set("who", ToString(tag)), Set("value", std::to_string(dif))});
}

inline tEffect<tTarget::kCountry> EventInCountry(uint const eventID, tTag tag)
{
    tEffect<tTarget::kCountry> res("country_event = { id=" + std::to_string(eventID) + " days=0 }");
    if (tag == tTag::kInvalidTag)
        return res;
    return EffectsForSpecifiedCountry
    (
        tag,
        {
            std::move(res)
        }
    );
}

inline tEffect<tTarget::kProvince> MakeNationalFor(tTag tag, tYesOrNo const flag)
{
    assertion(flag || tag != tTag::kFrom);
    return Set(flag ? "add_core" : "remove_core", ToString(tag));
}
