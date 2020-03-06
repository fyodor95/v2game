#include "capital_cultures_acceptance.h"
#include "cultures.h"
#include "decisions.h"

void CapitalCulturesAcceptanceImpl(size_t const i, tDecisionOutput & decisionOutput,
                                   std::ofstream & localizationOutput)
{
    tCulture const culture = kCultures[i];
    std::string name = "acceptance_capital_";
    name += ToString(culture);
    std::string title = Localization(culture);
    title += " Ч наши брать€!";
    std::string desc = "ќтныне ";
    desc += Localization(culture);
    desc += " €вл€ютс€ полноправными гражданами нашей страны!";
    tConditions<tTarget::kCountry> potential
    {
        ConditionsForCapital({MajorityCultureIs(culture), Not(IsInNewWorld())}),
        Not(Or<tTarget::kCountry>({IsAccepted(culture),
                                   IsPrimary(culture),
                                   And<tTarget::kCountry>({HasDefaultPrimaryCulture(), IsDominion()})}))
    };
    tConditions<tTarget::kCountry> allow
    {
        Not(CitizenshipPolicyIs(tCitizenshipPolicy::kResidency)),
        ConditionsForCapital({MajorityCultureIs(culture), PopulationIsGreaterOrEqual(20000u)})
    };
    Decision(std::move(name), title.data(), desc.data(), decisionOutput, localizationOutput, std::move(potential),
             std::move(allow), {AddPrestige(1), AddAcceptedCulture(culture)});
}

void CapitalCulturesAcceptance()
{
    tDecisionOutput decisionOutput     ("decisions\\capital_cultures_acceptance.txt");
    std::ofstream localizationOutput("localisation\\capital_cultures_acceptance.csv");
    for (size_t i = 0u; i != kCultures.size(); ++i)
        CapitalCulturesAcceptanceImpl(i, decisionOutput, localizationOutput);
}
