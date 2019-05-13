#include "cultures.h"
#include "decisions.h"
#include "new_primary_culture_for_dxx.h"

template <tCulture culture>
void NewPrimaryCulturesForDXXRealImpl(tDesicionOutput & decisionOutput, std::ofstream & localizationOutput)
{
    static_assert(culture != tCulture::british);
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
        EffectsForAllPops(Or<tTarget::kPop>({IsAcceptedPop(), IsPrimaryPop()}), {AddMilitancy(-10)})
    };
    Desicion(std::move(name), title.data(), desc.data(), decisionOutput, localizationOutput, std::move(potential),
             {ConditionsForCapital({MajorityCultureIs<culture>()})}, std::move(effects));
}

template <>
void NewPrimaryCulturesForDXXRealImpl<tCulture::british>(tDesicionOutput &, std::ofstream &)
{
}

template <size_t i = 0u>
void NewPrimaryCulturesForDXXImpl(tDesicionOutput & decisionOutput, std::ofstream & localizationOutput)
{
    NewPrimaryCulturesForDXXRealImpl<Cultures[i]>(decisionOutput, localizationOutput);
    NewPrimaryCulturesForDXXImpl<i + 1u>         (decisionOutput, localizationOutput);
}

template <>
void NewPrimaryCulturesForDXXImpl<Cultures.size()>(tDesicionOutput &, std::ofstream &)
{
}

void NewPrimaryCulturesForDXX()
{
    tDesicionOutput decisionOutput     ("decisions\\new_primary_culture_for_dxx.txt");
    std::ofstream localizationOutput("localisation\\new_primary_culture_for_dxx.csv");
    NewPrimaryCulturesForDXXImpl(decisionOutput, localizationOutput);
}
