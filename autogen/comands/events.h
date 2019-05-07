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

COMAND(tOption);

template <tTarget target>
tOption<target> Option(tEffects<target> &&, uint eventID, uint optionNum = 1u);

using tOptionDescriptions = std::vector<char const *>;

template <tTarget target>
void Event(uint id, char const * title, char const * desc,
           std::ostream & eventOutput, std::ostream & localizationOutput,
           tTrigger           <target> &&,
           tMeanTimeToHappen  <target> &&,
           tOptions           <target> &&,
           tOptionDescriptions const &, char const * picture = nullptr);
