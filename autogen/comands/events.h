#pragma once

#include "conditions.h"
#include "effects.h"
#include "event_pictures.h"
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

// Не рекомендуется делать параметр months больше 6000 с учётом всех факторов, потому что иначе с определённого
// значения событие начинает происходить очень часто (по-видимому, где-то происходит переполнение).
template <tTarget target>
tMeanTimeToHappen<target> MeanTimeToHappen(float const months, tModifiers<target> && modifiers = {})
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

template <tTarget target, tEventPicture = tEventPicture::NONE>
void Event(uint id, char const * title, char const * desc,
           std::ostream & eventOutput, std::ostream & localizationOutput,
           tTrigger           <target> &&,
           tMeanTimeToHappen  <target> &&,
           tOptions           <target> &&,
           tOptionDescriptions const &);

template <tEventPicture picture>
void CountryEvent(uint id, char const * title, char const * desc,
                  std::ostream & eventOutput, std::ostream & localizationOutput,
                  tTrigger           <tTarget::kCountry> && trigger,
                  tMeanTimeToHappen  <tTarget::kCountry> && meanTimeToHappen,
                  tOptions           <tTarget::kCountry> && options,
                  tOptionDescriptions const & optionDescriptions)
{
    return Event<tTarget::kCountry, picture>(id, title, desc, eventOutput, localizationOutput, std::move(trigger),
                                             std::move(meanTimeToHappen), std::move(options), optionDescriptions);
}
