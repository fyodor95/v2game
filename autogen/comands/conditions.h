#pragma once

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

// ¬озвращает true, если культура culture €вл€етс€ признанной в данной стране.
tCondition<tTarget::kCountry> AcceptedCulture(char const * culture);

// ¬озвращает true, если дл€ любой провинции, принадлежащей данной стране, выполн€етс€ условие condition.
tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition);

// ¬озвращает true, если в данной провинции больше всего людей имеют культуру culture.
tCondition<tTarget::kProvince> MajorityCultureIs(char const * culture);
