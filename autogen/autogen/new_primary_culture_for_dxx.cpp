#include "cultures.h"
#include "decisions.h"
#include "new_primary_culture_for_dxx.h"

template <tCulture culture>
void NewPrimaryCulturesForDXXRealImpl(tDecisionOutput & decisionOutput, std::ofstream & localizationOutput)
{
    static_assert(culture != kDefaultCulture);
    std::string name = "D_primary_";
    name += nsCulture::ToString<culture>();
    std::string title = nsCulture::Localization<culture>();
    title += " Ч наши брать€!";
    std::string desc = "ќтныне ";
    desc += nsCulture::Localization<culture>();
    desc += " €вл€ютс€ полноправными гражданами нашей страны!";
    tConditions<tTarget::kCountry> potential
    {
        HasDefaultPrimaryCulture(),
        ConditionsForCapital({MajorityCultureIs<culture>()}),
        IsDominion()
    };
    tEffects<tTarget::kCountry> effects
    {
        MakePrimary<culture>(),
        EffectsForAllPops(Or<tTarget::kPop>({HasAcceptedCulture(), HasPrimaryCulture()}), {AddMilitancy(-10)})
    };
    Decision(std::move(name), title.data(), desc.data(), decisionOutput, localizationOutput, std::move(potential),
             {ConditionsForCapital({MajorityCultureIs<culture>()})}, std::move(effects));
}

template <>
void NewPrimaryCulturesForDXXRealImpl<kDefaultCulture>(tDecisionOutput &, std::ofstream &)
{
}

template <size_t i = 0u>
void NewPrimaryCulturesForDXXImpl(tDecisionOutput & decisionOutput, std::ofstream & localizationOutput)
{
    NewPrimaryCulturesForDXXRealImpl<kCultures[i]>(decisionOutput, localizationOutput);
    NewPrimaryCulturesForDXXImpl<i + 1u>          (decisionOutput, localizationOutput);
}

template <>
void NewPrimaryCulturesForDXXImpl<kCultures.size()>(tDecisionOutput &, std::ofstream &)
{
}

void NewPrimaryCulturesForDXX()
{
    tDecisionOutput decisionOutput     ("decisions\\new_primary_culture_for_dxx.txt");
    std::ofstream localizationOutput("localisation\\new_primary_culture_for_dxx.csv");
    NewPrimaryCulturesForDXXImpl(decisionOutput, localizationOutput);
}
