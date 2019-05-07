#pragma once

#include "citizenship_policy.h"
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

// ¬озвращает true, если средн€€ агрессивность населени€ в данной провинции или стране не меньше, чем num.
template <tTarget target>
tCondition<target> AverageMilitancy(float num);

// ¬озвращает true, если культура culture €вл€етс€ признанной в данной стране.
tCondition<tTarget::kCountry> AcceptedCulture(char const * culture);

// ¬озвращает true, если дл€ любой провинции, принадлежащей данной стране, выполн€етс€ условие condition.
tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition);

// ¬озвращает true, если законом о гражданстве в данной стране €вл€етс€ policy.
template <tCitizenshipPolicy policy>
tCondition<tTarget::kCountry> CitizenshipPolicyIs() { return Set("citizenship_policy", ToString<policy>()); }

// ѕровер€ет услови€ coditions дл€ столицы данной страны. ¬озвращает true, если выполнено каждое из условий.
tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions);

// ѕровер€ет услови€ conditions дл€ владельца данной провинции. ¬озвращает true, если выполнено каждое из условий.
tCondition<tTarget::kProvince> ConditionsForOwner(tConditions<tTarget::kCountry> && conditions);

// ¬озвращает true, если в данной провинции существует поп с агрессивностью не менее, чем num.
tCondition<tTarget::kProvince> ExistsPopWithMilitancy(float num);

// ¬озвращает true, если в данной провинции больше всего людей имеют культуру culture.
tCondition<tTarget::kProvince> MajorityCultureIs(char const * culture);

// ¬озвращает true, если наиболее многочисленна€ культура в данной провинции €вл€етс€ признанной или основной.
// ≈сли flag == _No, то возвращаемое значение инвертируетс€.
tCondition<tTarget::kProvince> MajorityCultureIsAccepted(tYesOrNo flag = _Yes);

// ¬озвращает true, если наиболее многочисленна€ культура в данной провинции €вл€етс€ основной.
// ≈сли flag == _No, то возвращаемое значение инвертируетс€.
tCondition<tTarget::kProvince> MajorityCultureIsPrimary(tYesOrNo flag = _Yes);

// ¬озвращает true, если в количество населени€ в данной провинции не меньше, чем num.
tCondition<tTarget::kProvince> PopulationIsGreaterOrEqual(uint num);
