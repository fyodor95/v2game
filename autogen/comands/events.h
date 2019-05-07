#pragma once

#include "conditions.h"
#include "effects.h"
#include "modifier.h"
#include "target_comand.h"

COMAND(tTrigger);

template <tTarget target>
tTrigger<target> Trigger(tConditions<target> && conditions)
{
    static_assert(target != tTarget::kPop);
    return tTrigger<target>("trigger", std::move(CastToComands(conditions)));
}

COMAND(tMeanTimeToHappen);

template <tTarget target>
tMeanTimeToHappen<target> MeanTimeToHappen(float const months, tModifiers<target> && modifiers)
{
    static_assert(target != tTarget::kPop);
    tMeanTimeToHappen<target> res("mean_time_to_happen", {tComand("months", months)});
    res.AddToBody(std::move(CastToComands(modifiers)), 1u);
    return res;
}

// ¬озможно три случа€:
// 1) property == "title" && propertyName == "NAME";
// 2) property == "desc"  && propertyName == "DESC";
// 3) property == "name"  && propertyName == "OPT".
// јргумент optionNum нужен только дл€ третьего случа€ (и он не должен быть нулевым в этом случае).
tString EventProperty(char const * property, char const * propertyName,
                      uint eventID, uint optionNum = 0u);

COMAND(tOption);

template <tTarget target>
tOption<target> Option(tEffects<target> && effects, uint const eventID, uint const optionNum = 1u)
{
    static_assert(target != tTarget::kPop);
    assertion(optionNum && optionNum <= 26u);
    tOption<target> res("option", tComands{});
    res.AddToBody(EventProperty("name", "OPT", eventID, optionNum));
    res.AddToBody(std::move(CastToComands(effects)), 1u);
    return res;
}

tComand CountryEvent(uint id, char const * picture,
                     tTrigger           <tTarget::kCountry> &&,
                     tMeanTimeToHappen  <tTarget::kCountry> &&,
                     tOptions           <tTarget::kCountry> &&);

tComand ProvinceEvent(uint id,
                      tTrigger          <tTarget::kProvince> &&,
                      tMeanTimeToHappen <tTarget::kProvince> &&,
                      tOptions          <tTarget::kProvince> &&);
