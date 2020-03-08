#include "decisions.h"
#include "events.h"
#include "peaceful_settlement.h"

static void Generate(size_t const i, uint const id, tDecisionOutput & decisionOutput,
                     std::ofstream & localizationOutput)
{
    tTag const tag = kTags[i];
    tConditions<tTarget::kCountry> potentialConditions1
    {
        Exists(tag),
        Not(TagIs(tag)),
        IsIndependant(),
        Or<tTarget::kCountry>
        ({
            WarPolicyIs(tWarPolicy::anti_military),
            WarPolicyIs(tWarPolicy::pacifism)
        })
    };
    tConditions<tTarget::kCountry> potentialConditions2 = potentialConditions1;
    potentialConditions1.push_back(AnyOwnedProvince(IsNationalFor(tag)));
    potentialConditions1.push_back(AnyOwnedProvince(Not(IsNationalFor(tag))));
    potentialConditions2.push_back(AllProvinces(IsNationalFor(tag)));
    tConditions<tTarget::kCountry> allowedConditions1
    {
        Not(WarWith(tag)),
        RelationWithCountryIsHigherOrEqual(tag, -100),
        AllProvinces(Or<tTarget::kProvince>({Not(IsNationalFor(tag)), ControlledBy(tTag::kThis)}))
    };
    tConditions<tTarget::kCountry> allowedConditions2
    {
        Not(WarWith(tag)),
        RelationWithCountryIsHigherOrEqual(tag, 100),
        AllProvinces(ControlledBy(tTag::kThis))
    };
    tConditions<tTarget::kCountry> aiWillDoIt1
    {
        IsNeighbor(tag),
        Not(IsPartOfSphere()),
        ConditionsForSpecifiedCountry(tag, {War(_No), IsMobilised(_No)}),
        Or<tTarget::kCountry>
        ({
            And<tTarget::kCountry>
            ({
                NumOfBrigadesNTimesLessThan(tag, 4.f),
                Not(HasMuchBetterArmyTechsThan(tag))
            }),
            And<tTarget::kCountry>
            ({
                NumOfBrigadesNTimesLessThan(tag, 1.f),
                ConditionsForSpecifiedCountry(tag, {HasMuchBetterArmyTechsThan(tTag::kThis)})
            })
        })
    };
    tConditions<tTarget::kCountry> aiWillDoIt2 = aiWillDoIt1;
    std::string name = (std::string("Мирное урегулирование территориальных споров с державой ") +
                        Localization(tag));
    std::string desc = (std::string("Мы решили попробовать договориться с державой ") + Localization(tag) +
                        " цивилизованным путём.");
    Decision(ToString(tag) + std::string("_peaceful_settlement"), name.data(), desc.data(),
             decisionOutput, localizationOutput, std::move(potentialConditions1),
             std::move(allowedConditions1), {EventInCountry(id, tag)}, &aiWillDoIt1);
    name = std::string("Просьба о вхождении в состав державы ") + Localization(tag);
    desc = (std::string("Мы решили попросить державу ") + Localization(tag) + " войти в её состав.");
    Decision(ToString(tag) + std::string("_peaceful_unification"), name.data(), desc.data(),
             decisionOutput, localizationOutput, std::move(potentialConditions2),
             std::move(allowedConditions2), {EventInCountry(id + 3u, tag)}, &aiWillDoIt2);
}

void PeacefulSettlement()
{
    std::ofstream        eventOutput      ("events\\peaceful_settlement.txt");
    tDecisionOutput   decisionOutput   ("decisions\\peaceful_settlement.txt");
    std::ofstream localizationOutput("localisation\\peaceful_settlement.csv");
    uint id = 76000u;
    for (size_t i = 0u; i < kTags.size(); ++i)
        Generate(i, id, decisionOutput, localizationOutput);
    std::string name = std::string("Предложение державы ") + Localization(tTag::kFrom);
    std::string desc =
    (
        std::string("Послы державы ") + Localization(tTag::kFrom) +
        " сегодня обратились к Вам с предложением передать нашей державе все национальные для нас провинции, " +
        "которыми владеет держава " + Localization(tTag::kFrom) + '.'
    );
    tOptions<tTarget::kCountry> options1
    {
        Option<tTarget::kCountry>
        (
            {EventInCountry(id + 1u, tTag::kFrom)},
            id,
            1u,
            100u
        ),
        Option<tTarget::kCountry>
        (
            {
                EffectsForSpecifiedCountry
                (
                    tTag::kFrom,
                    {
                        EffectsForOwnedProvinces
                        (
                            {IsNationalFor(tTag::kThis)},
                            {MakeNationalFor(tTag::kThis, _No)}
                        )
                    }
                ),
                AddPrestige(-5),
                EventInCountry(id + 2u, tTag::kFrom)
            },
            id,
            2u,
            0u
        )
    };
    tOptions<tTarget::kCountry> options11
    {
        Option<tTarget::kCountry>
        (
            {
                EffectsForOwnedProvinces
                (
                    {IsNationalFor(tTag::kFrom)},
                    {ChangeOwner(tTag::kFrom), MakeNationalFor(tTag::kThis, _No)}
                ),
                AddPrestige(-10),
                AddBadboy(-5),
                ChangeRelation(tTag::kFrom, 200)
            },
            id
        )
    };
    TriggeredCountryEvent(id, name.data(), desc.data(), eventOutput, localizationOutput,
                          std::move(options1), {"Да будет так!", "Мы что, дураки?"}, "Administration");
    ++id;
    desc = std::string("Держава ") + Localization(tTag::kFrom) + " решила принять наше предложение.";
    TriggeredCountryEvent(id, "Они согласны", desc.data(), eventOutput, localizationOutput,
                          std::move(options11), {"Отлично!"}, "Administration");
    ++id;
    desc = std::string("Держава ") + Localization(tTag::kFrom) + " отказалась принять наше предложение.";
    TriggeredCountryEvent(id, "Они не согласны", desc.data(), eventOutput, localizationOutput,
                          {Option<tTarget::kCountry>({}, id)}, {"Им же хуже..."}, "Administration");
    desc =
    (
        std::string("Послы державы ") + Localization(tTag::kFrom) +
        " сегодня обратились к Вам с просьбой войти в состав нашей державы."
    );
    ++id;
    tOptions<tTarget::kCountry> options2
    {
        Option<tTarget::kCountry>
        (
            {
                Inherit(tTag::kFrom),
                AddPrestige(10)
            },
            id,
            1u,
            100u
        ),
        Option<tTarget::kCountry>
        (
            {
                EffectsForSpecifiedCountry
                (
                    tTag::kFrom,
                    {EffectsForOwnedProvinces({}, {MakeNationalFor(tTag::kThis, _No)})}
                ),
                AddPrestige(-5),
                EventInCountry(id - 1u, tTag::kFrom)
            },
            id,
            2u,
            0u
        )
    };
    TriggeredCountryEvent(id, name.data(), desc.data(), eventOutput, localizationOutput, std::move(options2),
                          {"Да будет так!", "Мы что, дураки?"}, "Administration");
}
