#include "events.h"
#include "simple_events.h"

// Событие, возникающее в странах с пролетарской диктатурой. Оно позволяет превратить форму правления в
// республиканскую.
// Чтобы событие возникло, необходимо иметь все социальные реформы, которые коммунисты проводят при захвате власти
// (8-часовой рабочий день, максимальные пособия по безработице, максимальные пенсии и максимальная система
// образования). Также необходимо иметь, как минимум следующие социальные реформы: wage_reform::low (низкая зарплата),
// safety_regulations::low (низкое обеспечение) и health_care::low (низкий уровень).
// Если проведены все социальные реформы, то возникает раз в 90 месяцев (7,5 лет). Иначе каждая непроведённая
// социальная реформа уменьшает скорость срабатывания вдвое.
static void CommunistPoliticalReforms(std::ofstream & eventOutput, std::ofstream & localizationOutput)
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
        (id, "Политические реформы", desc, eventOutput, localizationOutput,
         Trigger(std::move(conditions)), MeanTimeToHappen(90.f, std::move(modefiers)),
         {Option(std::move(effects), id), Option<tTarget::kCountry>({}, id, 2u)},
         {"Давайте сделаем это!", "Пока ещё рано"}, "Administration");
}

void GenerateSimpleEvents()
{
    std::ofstream         eventOutput     ("events\\simple_events.txt");
    std::ofstream localizationOutput("localisation\\simple_events.csv");
    CommunistPoliticalReforms(eventOutput, localizationOutput);
}
