#include "capital_cultures_acceptance.h"
#include "cultures.h"
#include "decisions.h"

template <size_t i = 0u>
void CapitalCulturesAcceptanceImpl(tDecisionOutput & decisionOutput, std::ofstream & localizationOutput)
{
    constexpr tCulture culture = kCultures[i];
    std::string name = "acceptance_capital_";
    name += nsCulture::ToString<culture>();
    std::string title = nsCulture::Localization<culture>();
    title += " Ч наши брать€!";
    std::string desc = "ќтныне ";
    desc += nsCulture::Localization<culture>();
    desc += " €вл€ютс€ полноправными гражданами нашей страны!";
    tConditions<tTarget::kCountry> potential
    {
        ConditionsForCapital({MajorityCultureIs<culture>(), Not(IsInNewWorld())}),
        Not(Or<tTarget::kCountry>({IsAccepted<culture>(),
                                   IsPrimary<culture>(),
                                   And<tTarget::kCountry>({HasDefaultPrimaryCulture(), IsDominion()})}))
    };
    Decision(std::move(name), title.data(), desc.data(), decisionOutput, localizationOutput, std::move(potential),
             {ConditionsForCapital({MajorityCultureIs<culture>(), PopulationIsGreaterOrEqual(20000u)})},
             {AddPrestige(1), AddAcceptedCulture<culture>()});
    CapitalCulturesAcceptanceImpl<i + 1u>(decisionOutput, localizationOutput);
}

template <>
void CapitalCulturesAcceptanceImpl<kCultures.size()>(tDecisionOutput &, std::ofstream &)
{
}

void CapitalCulturesAcceptance()
{
    tDecisionOutput decisionOutput     ("decisions\\capital_cultures_acceptance.txt");
    std::ofstream localizationOutput("localisation\\capital_cultures_acceptance.csv");
    CapitalCulturesAcceptanceImpl(decisionOutput, localizationOutput);
}
