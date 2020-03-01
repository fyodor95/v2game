#include "cultures.h"
#include "events.h"
#include "events_to_change_primary_culturies.h"

static UNCONST_SINGLETON(std::ofstream, EventOutput)
{
    return std::ofstream("events\\events_to_change_primary_culturies.txt");
}

static UNCONST_SINGLETON(std::ofstream, LocalisationOutput)
{
    return std::ofstream("localisation\\events_to_change_primary_culturies.csv");
}

static UNCONST_SINGLETON(uint, ID)
{
    return 7000u;
}

LOOP_BY_CULTURES(GenerateEvents)
{
    tCondition<tTarget::kCountry> const flagIsSet = HasFlag<tCountryFlag::primary_culture_must_be_changed>();
    tCouple<tConditions<tTarget::kCountry>> conditions
    {
        tConditions<tTarget::kCountry>
        {
            flagIsSet,
            ConditionsForCapital({MajorityCultureIs<culture>()})
        },
        tConditions<tTarget::kCountry>
        {
            flagIsSet,
            IsAccepted<culture>()
        }
    };
    tCouple<float> const months{8, 8};
    tEffects<tTarget::kCountry> effects
    {
        MakePrimary<culture>(),
        RemoveFlag<tCountryFlag::primary_culture_must_be_changed>()
    };
    for (uint id = ++ID(), i = 0u; i < 2u; ++id, ++i)
        CountryEvent
            (id, "Смена основной культуры", "Основная культура нашего государства изменилась.",
             EventOutput(), LocalisationOutput(),
             Trigger          <tTarget::kCountry>(std::move(conditions[i])),
             MeanTimeToHappen <tTarget::kCountry>(months[i]),
             {Option          <tTarget::kCountry>(tEffects<tTarget::kCountry>(effects), id)},
             {"Ок"}, "Administration");
    ++ID();
}

void EventsToChangePrimaryCultures()
{
    tConditions<tTarget::kCountry> conditions
    {
        Not(HasFlag<tCountryFlag::primary_culture_must_be_changed>()),
        Not(HasProvince({HasPop(HasPrimaryCulture())}))
    };
    CountryEvent
        (ID(), "Кто такие $COUNTRYCULTURE$?",
         "В нашей стране не проживает ни одного представителя народа $COUNTRYCULTURE$. Это означает, что вскоре основная культура нашего государства изменится.",
         EventOutput(), LocalisationOutput(),
         Trigger          <tTarget::kCountry>(std::move(conditions)),
         MeanTimeToHappen <tTarget::kCountry>(9.f),
         {Option          <tTarget::kCountry>({SetFlag<tCountryFlag::primary_culture_must_be_changed>()}, ID())},
         {"Ок"}, "Administration");
    GenerateEvents();
}
