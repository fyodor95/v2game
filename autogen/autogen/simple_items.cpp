#include "decisions.h"
#include "events.h"
#include "simple_items.h"

static UNCONST_SINGLETON(EventOutput, std::ofstream)
{
    return std::ofstream("events\\simple_events.txt");
}

static UNCONST_SINGLETON(LocalizationOutput, std::ofstream)
{
    return std::ofstream("localisation\\simple_items.csv");
}

static UNCONST_SINGLETON(DecisionOutput, tDecisionOutput)
{
    return "decisions\\simple_decisions.txt";
}

// Событие, возникающее в странах с пролетарской диктатурой. Оно позволяет превратить форму правления в
// республиканскую.
// Чтобы событие возникло, необходимо иметь все социальные реформы, которые коммунисты проводят при захвате власти
// (8-часовой рабочий день, максимальные пособия по безработице, максимальные пенсии и максимальная система
// образования). Также необходимо иметь, как минимум следующие социальные реформы: wage_reform::low (низкая зарплата),
// safety_regulations::low (низкое обеспечение) и health_care::low (низкий уровень).
// Если проведены все социальные реформы, то возникает раз в 90 месяцев (7,5 лет). Иначе каждая непроведённая
// социальная реформа уменьшает скорость срабатывания вдвое.
static void CommunistPoliticalReforms()
{
    using namespace nsReforms;
    static constexpr uint id = 403u;
    char const * const desc =
        "В последнее время мы провели огромное количество социальных реформ. Пришло время взяться за политические.";
    tConditions<tTarget::kCountry> conditions
    {
        GovernmentIs(tGovernment::proletarian_dictatorship),
        ReformLevelIs<work_hours             , work_hours::eight>(),
        ReformLevelIs<unemployment_subsidies , unemployment_subsidies ::good>(),
        ReformLevelIs<pensions               , pensions               ::good>(),
        ReformLevelIs<school_reforms         , school_reforms         ::good>(),
        ReformLevelIsGreaterOrEaqualThan<wage_reform        , wage_reform        ::low>(),
        ReformLevelIsGreaterOrEaqualThan<safety_regulations , safety_regulations ::low>(),
        ReformLevelIsGreaterOrEaqualThan<health_care        , health_care        ::low>()
    };
    tModifiers<tTarget::kCountry> modefiers
    {
        {2.f, ReformLevelIsLessThan<wage_reform        , wage_reform        ::acceptable>()},
        {2.f, ReformLevelIsLessThan<safety_regulations , safety_regulations ::acceptable>()},
        {2.f, ReformLevelIsLessThan<health_care        , health_care        ::acceptable>()},
        {2.f, Not(ReformLevelIs    <wage_reform        , wage_reform        ::good>())},
        {2.f, Not(ReformLevelIs    <safety_regulations , safety_regulations ::good>())},
        {2.f, Not(ReformLevelIs    <health_care        , health_care        ::good>())}
    };
    tEffects<tTarget::kCountry> effects
    {
        ChangeReformLevel<vote_franschise, vote_franschise::universal>(),
        RandomEffectFrom<tTarget::kCountry>
        ({
            ChangeReformLevel<upper_house_composition, upper_house_composition::      state_equal_weight>(),
            ChangeReformLevel<upper_house_composition, upper_house_composition:: population_equal_weight>()
        }),
        RandomEffectFrom<tTarget::kCountry>
        ({
            ChangeReformLevel<political_parties, political_parties::harassment         >(),
            ChangeReformLevel<political_parties, political_parties::gerrymandering     >(),
            ChangeReformLevel<political_parties, political_parties::non_secret_ballots >(),
            ChangeReformLevel<political_parties, political_parties::secret_ballots     >()
        }),
        ChangeGovernmentType(tGovernment::democracy)
    };
    CountryEvent
        (id, "Политические реформы", desc, EventOutput(), LocalizationOutput(),
         Trigger(std::move(conditions)), MeanTimeToHappen(90.f, std::move(modefiers)),
         {Option(std::move(effects), id), Option<tTarget::kCountry>({}, id, 2u)},
         {"Давайте сделаем это!", "Пока ещё рано"}, "Administration");
}

static tCondition<tTarget::kCountry> IsChineseVassal(tTag const tag)
{
    return And<tTarget::kCountry>({TagIs(tag), ConditionsForSpecifiedCountry(tTag::CHI, {HasVassal(tag)})});
}

// Решение для Китая, позволяющее присоединить все автономии и Тибет.
static void RestoreJustice()
{
    tConditions<tTarget::kCountry> potential
    {
        TagIs(tTag::CHI),
        Or<tTarget::kCountry>
        ({
            HasVassal(tTag::GXI),
            HasVassal(tTag::MCK),
            HasVassal(tTag::XBI),
            HasVassal(tTag::YNN),
            HasVassal(tTag::XIN),
            HasVassal(tTag::MGL),
            HasVassal(tTag::TIB)
        }),
        Not(HasFlag(tCountryFlag::justice_was_restored))
    };
    tEffects<tTarget::kCountry> effects
    {
        SetFlag(tCountryFlag::justice_was_restored),
        EffectsForSpecifiedProvince(1469, {MakeNationalFor(tTag::RUS)}),
        EffectsForSpecifiedProvince(1082, {MakeNationalFor(tTag::RUS)}),
        EffectsForSpecifiedProvince(1083, {MakeNationalFor(tTag::RUS)}),
        EffectsForSpecifiedProvince(1084, {MakeNationalFor(tTag::RUS)}),
        EffectsForSpecifiedProvince(1085, {MakeNationalFor(tTag::RUS)}),
        EffectsForSpecifiedProvince(2617, {MakeNationalFor(tTag::RUS)}),
        EffectsForSpecifiedProvince(1081, {MakeNationalFor(tTag::CHI), MakeNationalFor(tTag::MCK)}),
        EffectsForSpecifiedProvince(2697, {MakeNationalFor(tTag::CHI), MakeNationalFor(tTag::MCK)}),
        EffectsForAllCountries
        (
            {
                Or<tTarget::kCountry>
                ({
                    IsChineseVassal(tTag::GXI),
                    IsChineseVassal(tTag::MCK),
                    IsChineseVassal(tTag::XBI),
                    IsChineseVassal(tTag::YNN),
                    IsChineseVassal(tTag::XIN),
                    IsChineseVassal(tTag::MGL),
                    IsChineseVassal(tTag::TIB)
                })
            },
            {EventInCountry(11101u)}
        )
    };
    Decision("chi_restore_justice", "Восстановление игровой справедливости",
             "Нас долго унижали. Пришла пора расправить плечи.", DecisionOutput(),
             LocalizationOutput(), std::move(potential), {}, std::move(effects));
}

// Удаление нецивилизованных модификаторов для цивилизованных стран.
static void RemoveUncivMods()
{
    tConditions<tTarget::kCountry> conditions
    {
        Civilized(),

        Or<tTarget::kCountry>
        ({
            HasCountryModifier("educational_reform"),
            HasCountryModifier("military_reform"),
            HasCountryModifier("western_influences"),
            HasCountryModifier("uncivilized_isolationism"),
            HasCountryModifier("trade_restrictions"),
            HasCountryModifier("sayings_of_the_dutch"),
            HasCountryModifier("extraterritoriality_granted"),
            HasCountryModifier("meiji_restoration_boost"),

            HasProvince
            ({
                Or<tTarget::kProvince>
                ({
                    HasProvinceModifier("western_presence"),
                    HasProvinceModifier("foreign_smugglers"),
                    HasProvinceModifier("foreign_trading_post"),
                    HasProvinceModifier("local_opium_habit"),
                    HasProvinceModifier("european_military_mission"),
                    HasProvinceModifier("legation_quarter"),
                    HasProvinceModifier("kettler_denkmal"),
                })
            })
        })
    };

    tEffects<tTarget::kCountry> effects
    {
        RemoveCountryModifier("educational_reform"),
        RemoveCountryModifier("military_reform"),
        RemoveCountryModifier("western_influences"),
        RemoveCountryModifier("uncivilized_isolationism"),
        RemoveCountryModifier("trade_restrictions"),
        RemoveCountryModifier("sayings_of_the_dutch"),
        RemoveCountryModifier("extraterritoriality_granted"),
        RemoveCountryModifier("meiji_restoration_boost"),

        EffectsForOwnedProvinces
        (
            {},

            {
                RemoveProvinceModifier("western_presence"),
                RemoveProvinceModifier("foreign_smugglers"),
                RemoveProvinceModifier("foreign_trading_post"),
                RemoveProvinceModifier("local_opium_habit"),
                RemoveProvinceModifier("european_military_mission"),
                RemoveProvinceModifier("legation_quarter"),
                RemoveProvinceModifier("kettler_denkmal"),
            }
        )
    };

    char const * const desc =
    (
        "Наша страна находится под интеллектуальным натиском современности и западного мира. В "
        "области современных технологий, промышленности и военной теории старые традиции и "
        "институты больше не могут служить нам, как прежде. Западное влияние распространяется как "
        "лесной пожар по нашему государству, среди нашего народа и в наших душах. Если мы хотим "
        "остаться собой и не потерять свою суть, сейчас самое время взять нашу судьбу в свои "
        "руки. Ведущим представителям нашей интеллектуальной элиты стало очевидно, что "
        "единственная возможность остановить разрушительный напор чудовища с запада — плыть его "
        "курсом. Сунь Цзы говорил: «Тростник гнётся вместе с ветром и этим сопротивляется ему.»."
    );

    uint const id = 160002;

    CountryEvent(id, "Принятие вестернизации", desc, EventOutput(), LocalizationOutput(),
                 Trigger(std::move(conditions)), MeanTimeToHappen<tTarget::kCountry>(1.f),
                 {Option(std::move(effects), id)}, {"В фургоны и на Запад!"}, "Prosper");
}

void GenerateSimpleItems()
{
    CommunistPoliticalReforms();
    RemoveUncivMods();
    RestoreJustice();
}
