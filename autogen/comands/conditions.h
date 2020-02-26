#pragma once

#include "citizenship_policy.h"
#include "continents.h"
#include "cultures.h"
#include "flags.h"
#include "governments.h"
#include "pop_types.h"
#include "reforms.h"
#include "tags.h"
#include "target_comand.h"

COMAND(tCondition);

template <tTarget target>
tCondition<target> And(tConditions<target> && conditions)
{
    static_assert(target != tTarget::kPop, "результат этой операции может быть очень неожиданным");
    return tCondition<target>("AND", std::move(CastToComands(conditions)));
}

template <tTarget target>
tCondition<target> Or(tConditions<target> && conditions)
{
    return tCondition<target>("OR", std::move(CastToComands(conditions)));
}

template <tTarget target>
tCondition<target> Not(tCondition<target> && condition)
{
    return tCondition<target>("NOT", {static_cast<tComand &&>(condition)});
}

template <tTarget target>
tCondition<target> NotOr(tCondition<target> && conditions)
{
    return Not<target>(Or<target>(std::move(conditions)));
}

// Возвращает true, если средняя агрессивность населения в данной провинции или стране не меньше, чем num.
template <tTarget target>
tCondition<target> AverageMilitancy(float num);

// Возвращает true, если хотя бы для одной провинции, принадлежащей данной стране, выполняется условие condition.
tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition);

// Возвращает true, если законом о гражданстве в данной стране является policy.
template <tCitizenshipPolicy policy>
tCondition<tTarget::kCountry> CitizenshipPolicyIs()
{ return Set("citizenship_policy", nsCitizenshipPolicy::ToString<policy>()); }

// Возвращает true, если данная страна вестернизирована.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kCountry> Civilized(tYesOrNo flag = _Yes);

// Проверяет условия conditions для столицы данной страны. Возвращает true, если выполнено каждое из условий.
tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions);

// Проверяет условия conditions для державы tag. Возвращает true, если выполнено каждое из условий.
template <tTag tag, tTarget target = tTarget::kCountry>
tCondition<target> ConditionsForSpecifiedCountry(tConditions<tTarget::kCountry> && conditions);

// Возвращает true, если данная страна существует в данный момент.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kCountry> Exists(tYesOrNo flag = _Yes);

// Возвращает true, если данная страна имеют форму правления government.
template <tGovernment government>
tCondition<tTarget::kCountry> GovernmentIs() { return Set("government", nsGovernment::ToString<government>()); }

// Условие для доминионов (стран с тэгами DXX (01 <= XX <= 50)). По умолчанию их культурой является tCulture::british.
// Возвращает true, если основная культура государства совпадает с культурой по умолчанию.
tCondition<tTarget::kCountry> HasDefaultPrimaryCulture();

template <tCountryFlag flag>
tCondition<tTarget::kCountry> HasFlag() { return Set("has_country_flag", nsCountryFlag::ToString<flag>()); }

// Возвращает true, если в данной стране существует провинция, удовлетворяющая набору условий conditions.
tCondition<tTarget::kCountry> HasProvince(tConditions<tTarget::kProvince> && conditions);

// Возвращает true, если культура culture является признанной в данной стране.
template <tCulture culture>
tCondition<tTarget::kCountry> IsAccepted() { return Set("accepted_culture", nsCulture::ToString<culture>()); }

// Возвращает true, если культура culture является признанной или основной в данной стране.
template <tCulture culture>
tCondition<tTarget::kCountry> IsAcceptedOrPrimary();

// Возвращает true, если тэгом является DXX (01 <= XX <= 50).
tCondition<tTarget::kCountry> IsDominion();

// Возвращает true, если культура culture является основной в данной стране.
template <tCulture culture>
tCondition<tTarget::kCountry> IsPrimary() { return Set("primary_culture", nsCulture::ToString<culture>()); }

// Возвращает true, если уровень реформы tReformType в данной стране равен level.
template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIs()
{
    return Set(std::string(nsReforms::ToString<tReformType>()), nsReforms::ToString<tReformType, level>());
}

// Возвращает true, если уровень реформы tReformType в данной стране меньше, чем level.
template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsLessThan();

// Возвращает true, если уровень реформы tReformType в данной стране больше или равен level.
template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsGreaterOrEaqualThan();

template <tTag tag>
tCondition<tTarget::kCountry> TagIs() { return Set("tag", nsTag::ToString<tag>()); }

// Проверяет условия conditions для владельца данной провинции. Возвращает true, если выполнено каждое из условий.
tCondition<tTarget::kProvince> ConditionsForOwner(tConditions<tTarget::kCountry> && conditions);

// Возвращает true, если данная провинция оккупирована повстанцами.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> ControlledByRebels(tYesOrNo flag = _Yes);

// Возвращает true, если данная провинция не принадлежит никому (то есть ещё ни кем не колонизированной).
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> Empty(tYesOrNo flag = _Yes);

// Возвращает true, если в данной провинции существует поп с агрессивностью не менее, чем num.
tCondition<tTarget::kProvince> ExistsPopWithMilitancy(float num);

// Возвращает true, если среди соседей данной провинции существует хотя бы одна НЕПУСТАЯ провинция, удовлетворяющая
// набору условий conditions.
tCondition<tTarget::kProvince> HasNeighborProvince(tConditions<tTarget::kProvince> && conditions);

// Возвращает true, если в данной провинции проживает поп, удовлетворяющий условию condition.
tCondition<tTarget::kProvince> HasPop(tCondition<tTarget::kPop> && condition);

// Возвращает true, если данная провинция находится на континенте continent.
template <tContinent continent>
tCondition<tTarget::kProvince> IsInContinent() { return Set("continent", nsContinent::ToString<continent>()); }

// Возвращает true, если данная провинция находится в Америке или Океании.
tCondition<tTarget::kProvince> IsInNewWorld();

// Возвращает true, если данная провинция является национальной для державы tag.
template <tTag tag>
tCondition<tTarget::kProvince> IsNationalFor()
{
    return Set("is_core", nsTag::ToString<tag>());
}

// Возвращает true, если culture является самой многочисленной культурой данной провинции.
template <tCulture culture>
tCondition<tTarget::kProvince> MajorityCultureIs() { return Set("culture", nsCulture::ToString<culture>()); }

// Возвращает true, если наиболее многочисленная культура в данной провинции является признанной или основной.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> MajorityCultureIsAccepted(tYesOrNo flag = _Yes);

// Возвращает true, если наиболее многочисленная культура в данной провинции является основной.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> MajorityCultureIsPrimary(tYesOrNo flag = _Yes);

// Возвращает true, если в количество населения в данной провинции не меньше, чем num.
tCondition<tTarget::kProvince> PopulationIsGreaterOrEqual(uint num);

// Возвращает true, если культурой попа является culture.
template <tCulture culture>
tCondition<tTarget::kPop> HasCulture()
{
    return Set("has_pop_culture", nsCulture::ToString<culture>());
}

// Возвращает true, если поп имеет тип type.
template <tPopType type>
tCondition<tTarget::kPop> HasType()
{
    return Set("type", nsPopType::ToString<type>());
}

// Возвращает true, если культура попа является признанной культурой страны, в которой он проживает.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kPop> HasAcceptedCulture(tYesOrNo flag = _Yes);

// Возвращает true, если культура попа совпадает с основной культурой страны, в которой он проживает.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kPop> HasPrimaryCulture(tYesOrNo flag = _Yes);

template <class tReformType, uint8_t from, uint8_t to>
void AddConditions(tConditions<tTarget::kCountry> & where)
{
    if constexpr (from == to)
        return;
    else
    {
        static_assert(from != to);
        where.push_back(ReformLevelIs<tReformType, static_cast<tReformType>(from)>());
        AddConditions<tReformType, from + 1u, to>(where);
    }
}

template <tTag tag, tTarget target>
tCondition<target> ConditionsForSpecifiedCountry(tConditions<tTarget::kCountry> && conditions)
{
    if constexpr (target == tTarget::kCountry)
        return tCondition<target>(nsTag::ToString<tag>(), std::move(CastToComands(conditions)));
    else
        return tCondition<target>(ConditionsForOwner({ConditionsForSpecifiedCountry<tag>(std::move(conditions))}));
}

template <tCulture culture>
tCondition<tTarget::kCountry> IsAcceptedOrPrimary()
{
    return Or<tTarget::kCountry>({IsAccepted<culture>(), IsPrimary<culture>()});
}

template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsLessThan()
{
    static_assert(static_cast<uint8_t>(level));
    if constexpr (static_cast<uint8_t>(level) > static_cast<uint8_t>(tReformType::END) - static_cast<uint8_t>(level))
        return Not(ReformLevelIsGreaterOrEaqualThan<tReformType, level>());
    else
    {
        tConditions<tTarget::kCountry> conditions;
        AddConditions<tReformType, 0u, static_cast<uint8_t>(level)>(conditions);
        return Or<tTarget::kCountry>(std::move(conditions));
    }
}

template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsGreaterOrEaqualThan()
{
    static_assert(static_cast<uint8_t>(level) < static_cast<uint8_t>(tReformType::END) - 1u);
    if constexpr (static_cast<uint8_t>(level) <= static_cast<uint8_t>(tReformType::END) - static_cast<uint8_t>(level))
        return Not(ReformLevelIsLessThan<tReformType, level>());
    else
    {
        tConditions<tTarget::kCountry> conditions;
        AddConditions<tReformType, static_cast<uint8_t>(level), static_cast<uint8_t>(tReformType::END)>(conditions);
        return Or<tTarget::kCountry>(std::move(conditions));
    }
}
