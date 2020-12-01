#include "change_tag.h"
#include "conditions.h"
#include "effects.h"
#include "events.h"

void ChangeTag()
{
    std::ofstream        eventOutput      ("events\\change_tag.txt");
    std::ofstream localizationOutput("localisation\\change_tag.csv");

    uint id = 1472073;

    for (tTag const tag : kTags)
    {
        uint const initialId = id;

        tConditions<tTarget::kCountry> isOkCountry
        {
            TagIs(tag),
            IsAI(),
            PopulationIsGreaterOrEqual<tTarget::kCountry>(100000),

            Or<tTarget::kCountry>
            ({
                PopulationIsGreaterOrEqual<tTarget::kCountry>(1000000),

                And<tTarget::kCountry>
                ({
                    ScoreIsGreaterOrEqual(tScoreType::military_score, 1),
                    IsIndependant(),
                    RankIsGreaterOrEqual(100)
                })
            })
        };

        tModifiers<tTarget::kCountry> modifiers
        {
            tModifier( 0.95f, Civilized()),
            tModifier( 0.95f, RankIsGreaterOrEqual(16)),
            tModifier( 0.95f, IsGreatPower()),
            tModifier( 0.95f, ScoreIsGreaterOrEqual(tScoreType::industrial_score, 100)),
            tModifier( 0.95f, ScoreIsGreaterOrEqual(tScoreType::industrial_score, 1000)),

            tModifier( 2.15f, ReligiousPolicyIs(tReligiousPolicy::moralism    )),
            tModifier( 1.29f, ReligiousPolicyIs(tReligiousPolicy::pluralism   )),
            tModifier( 0.77f, ReligiousPolicyIs(tReligiousPolicy::secularized )),
            tModifier( 0.46f, ReligiousPolicyIs(tReligiousPolicy::pro_atheism )),

            tModifier( 2.15f, WarPolicyIs(tWarPolicy::jingoism      )),
            tModifier( 1.29f, WarPolicyIs(tWarPolicy::pro_military  )),
            tModifier( 0.77f, WarPolicyIs(tWarPolicy::anti_military )),
            tModifier( 0.46f, WarPolicyIs(tWarPolicy::pacifism      )),

            tModifier( 2.15f, CitizenshipPolicyIs(tCitizenshipPolicy::kResidency )),
            tModifier( 0.46f, CitizenshipPolicyIs(tCitizenshipPolicy::kFull      )),
        };

        auto switchToCurrentTag = [initialId]
        {
            return tEffects<tTarget::kCountry>{EventInCountry(initialId + 2u)};
        };

        tEffects<tTarget::kCountry> _switch
        {
            SetFlag(tCountryFlag::not_ai),
            EffectsForRandomCountry({IsAI(_No), War(_No)}, switchToCurrentTag())
        };

        Event
        (
            id,
            "0",
            "0",
            eventOutput,
            localizationOutput,

            Trigger          <tTarget::kCountry>(std::move(isOkCountry)),
            MeanTimeToHappen <tTarget::kCountry>(1200.f, std::move(modifiers)),
            {Option          <tTarget::kCountry>(std::move(_switch), id)},

            {"0"},
            "greatpowers"
        );

        ++id;

        tConditions<tTarget::kCountry> isFormerKozelCountry
        {
            HasFlag(tCountryFlag::not_ai),
            TagIs(tag),
            HasGlobalFlag("kozelam_net"),
            IsAI()
        };

        tEffects<tTarget::kCountry> switchBack
        {
            EffectsForRandomCountry({IsAI(_No), Not(HasFlag(tCountryFlag::not_ai))}, switchToCurrentTag())
        };

        Event
        (
            id,
            "0",
            "0",
            eventOutput,
            localizationOutput,

            Trigger          <tTarget::kCountry>(std::move(isFormerKozelCountry)),
            MeanTimeToHappen <tTarget::kCountry>(1.f),
            {Option          <tTarget::kCountry>(std::move(switchBack), id)},

            {"0"},
            "greatpowers"
        );

        ++id;

        TriggeredCountryEvent(id, "Смена места работы", "Пора поуправлять другой страной.", eventOutput,
                              localizationOutput,
                              {Option<tTarget::kCountry>({RemoveFlag(tCountryFlag::not_ai), SwitchTo(tag)}, id)},
                              {"Да!"}, "Celebration");

        ++id;
    }
}
