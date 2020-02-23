#include "adding_cores.h"
#include "conditions.h"
#include "culture_country_hardcode.h"
#include "effects.h"
#include "events.h"

template <tCulture culture, tTag tag>
void ProcessCultureAndCountry(uint & id, std::ofstream & eventOutput, std::ofstream & localizationOutput)
{
    if constexpr (tag == tTag::kInvalidTag)
        return;
    std::string const desc = "Провинция $PROVINCENAME$ стала считаться национальной для державы ";
    tConditions<tTarget::kProvince> conditions
    {
        ControlledByRebels(),
        Not(IsNationalFor<tag>()),
        ConditionsForOwner
        ({
            Not(TagIs<tag>()),
            ConditionsForSpecifiedCountry<tag>({IsAcceptedOrPrimary<culture>()})
        }),
        Or<tTarget::kProvince>
        ({
            And<tTarget::kProvince>
            ({
                MajorityCultureIs<culture>(),
                ConditionsForOwner
                ({
                    ConditionsForSpecifiedCountry<tag>({Exists(_No)}),
                    Not(AnyOwnedProvince(IsNationalFor<tag>()))
                })
            }),
            And<tTarget::kProvince>
            ({
                HasPop(HasCulture<culture>()),
                HasNeighborProvince({IsNationalFor<tag>()})
            })
        })
    };
    ++id;
    Event(id, "Плоды восстаний", (desc + nsTag::Localization<tag>() + '.').data(), eventOutput, localizationOutput,
          Trigger          <tTarget::kProvince>(std::move(conditions)),
          MeanTimeToHappen <tTarget::kProvince>(4.f),
          {Option          <tTarget::kProvince>({MakeNationalFor<tag>()}, id)},
          {"Ок"});
}

template <size_t i = 0u>
static void ProcessCulture(uint & id, std::ofstream & eventOutput, std::ofstream & localizationOutput)
{
    ProcessCultureAndCountry<kCultureTagHardcode[i].first, kCultureTagHardcode[i].second[0]>(id, eventOutput, localizationOutput);
    ProcessCultureAndCountry<kCultureTagHardcode[i].first, kCultureTagHardcode[i].second[1]>(id, eventOutput, localizationOutput);
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
