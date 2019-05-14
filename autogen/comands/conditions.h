#pragma once

#include "citizenship_policy.h"
#include "continents.h"
#include "cultures.h"
#include "governments.h"
#include "reforms.h"
#include "tags.h"
#include "target_comand.h"

COMAND(tCondition);

template <tTarget target>
tCondition<target> And(tConditions<target> && conditions)
{
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

// Возвращает true, если культура culture является признанной в данной стране.
template <tCulture culture>
tCondition<tTarget::kCountry> IsAccepted() { return Set("accepted_culture", nsCulture::ToString<culture>()); }

// Возвращает true, если для любой провинции, принадлежащей данной стране, выполняется условие condition.
tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition);

// Возвращает true, если законом о гражданстве в данной стране является policy.
template <tCitizenshipPolicy policy>
tCondition<tTarget::kCountry> CitizenshipPolicyIs()
{ return Set("citizenship_policy", nsCitizenshipPolicy::ToString<policy>()); }

// Проверяет условия coditions для столицы данной страны. Возвращает true, если выполнено каждое из условий.
tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions);

// Возвращает true, если данная страна имеют форму правления government.
template <tGovernment government>
tCondition<tTarget::kCountry> GovernmentIs()
{
    return Set("government", nsGovernment::ToString<government>());
}

// Условие для доминионов (стран с тэгами DXX (01 <= XX <= 50)). По умолчанию их культурой является tCulture::british.
// Возвращает true, если основная культура государства совпадает с культурой по умолчанию.
tCondition<tTarget::kCountry> HasDefaultPrimaryCulture();

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

// Возвращает true, если в данной провинции существует поп с агрессивностью не менее, чем num.
tCondition<tTarget::kProvince> ExistsPopWithMilitancy(float num);

// Возвращает true, если данная провинция находится на континенте continent.
template <tContinent continent>
tCondition<tTarget::kProvince> IsInContinent() { return Set("continent", nsContinent::ToString<continent>()); }

// Возвращает true, если данная провинция находится в Америке или Океании.
tCondition<tTarget::kProvince> IsInNewWorld();

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
