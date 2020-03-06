#include "adding_cores.h"
#include "conditions.h"
#include "culture_country_hardcode.h"
#include "effects.h"
#include "events.h"

template
<
    tCulture  culture,
    tTag      tag,
    bool      civilized,
    bool      main,
    bool      dxx
>
void ProcessCultureAndCountry(uint & id, std::ofstream & eventOutput, std::ofstream & localizationOutput)
{
    std::string const desc = "Провинция $PROVINCENAME$ стала считаться национальной для державы ";
    tConditions<tTarget::kProvince> conditions
    {
        ControlledByRebels(),
        Not(IsNationalFor<tag>())
    };
    tConditions<tTarget::kCountry> conditionsForOwner{Not(TagIs<tag>())};
    tConditions<tTarget::kCountry> conditionsForTag;
    tEffects<tTarget::kProvince> effects = {MakeNationalFor<tag>()};
    if constexpr (!main || !dxx)
        conditionsForTag.push_back(IsAcceptedOrPrimary<culture>());
    else
        effects.push_back(EffectsForSpecifiedCountry<tag, tTarget::kProvince>({AddAcceptedCulture<culture>()}));
    if constexpr (main)
    {
        conditions.push_back(MajorityCultureIs<culture>());
        conditionsForOwner.push_back(Not(Exists<tag>()));
        if constexpr (civilized && !dxx)
            conditionsForOwner.push_back(Civilized());
        conditionsForOwner.push_back(Not(AnyOwnedProvince(IsNationalFor<tag>())));
    }
    else
    {
        conditions.push_back(HasNeighborProvince({IsNationalFor<tag>()}));
        conditions.push_back(HasPop(HasCulture<culture>()));
    }
    conditionsForOwner.push_back(ConditionsForSpecifiedCountry<tag>(std::move(conditionsForTag)));
    conditions.push_back(ConditionsForOwner(std::move(conditionsForOwner)));
    ++id;
    Event(id, "Плоды восстаний", (desc + nsTag::Localization<tag>() + '.').data(), eventOutput, localizationOutput,
          Trigger          <tTarget::kProvince>(std::move(conditions)),
          MeanTimeToHappen <tTarget::kProvince>(4.f),
          {Option          <tTarget::kProvince>(std::move(effects), id)},
          {"Ок"});
}

template <size_t i = 0u>
static void ProcessCulture(uint & id, std::ofstream & eventOutput, std::ofstream & localizationOutput)
{
    ProcessCultureAndCountry
    <
        kCultureTagHardcode[i].Culture,
        kCultureTagHardcode[i].Tag,
        kCultureTagHardcode[i].Civilized,
        kCultureTagHardcode[i].Main,
        kCultureTagHardcode[i].DXX
    >
    (id, eventOutput, localizationOutput);
    if constexpr (kCultureTagHardcode[i].Main)
        ProcessCultureAndCountry
        <
            kCultureTagHardcode[i].Culture,
            kCultureTagHardcode[i].Tag,
            kCultureTagHardcode[i].Civilized,
            false,
            kCultureTagHardcode[i].DXX
        >
        (id, eventOutput, localizationOutput);
    ProcessCulture<i + 1u>(id, eventOutput, localizationOutput);
}

template <>
void ProcessCulture<kCultureTagHardcode.size()>(uint &, std::ofstream &, std::ofstream &)
{
}

void CoreAddingEvents()
{
    std::ofstream        eventOutput      ("events\\adding_cores.txt");
    std::ofstream localizationOutput("localisation\\adding_cores.csv");
    uint id = 24000u;
    ProcessCulture(id, eventOutput, localizationOutput);
}
