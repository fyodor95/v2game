#include "reforms.h"

namespace nsReforms
{

    namespace nsType
    {

        template <>
        char const * ToString<tType::political>()
        {
            return "political_reform";
        }

        template <>
        char const * ToString<tType::social>()
        {
            return "social_reform";
        }

    }

    template <>
    char const * ToString<wage_reform, wage_reform::no>()
    {
        return "no_minimum_wage";
    }

    template <>
    char const * ToString<wage_reform, wage_reform::trinket>()
    {
        return "trinket_wage";
    }

    template <>
    char const * ToString<wage_reform, wage_reform::low>()
    {
        return "low_minimum_wage";
    }

    template <>
    char const * ToString<wage_reform, wage_reform::acceptable>()
    {
        return "acceptable_minimum_wage";
    }

    template <>
    char const * ToString<wage_reform, wage_reform::good>()
    {
        return "good_minimum_wage";
    }

    template <>
    char const * ToString<work_hours, work_hours::no>()
    {
        return "no_work_hour_limit";
    }

    template <>
    char const * ToString<work_hours, work_hours::fourteen>()
    {
        return "fourteen_hours";
    }

    template <>
    char const * ToString<work_hours, work_hours::twelve>()
    {
        return "twelve_hours";
    }

    template <>
    char const * ToString<work_hours, work_hours::ten>()
    {
        return "ten_hours";
    }

    template <>
    char const * ToString<work_hours, work_hours::eight>()
    {
        return "eight_hours";
    }

    template <>
    char const * ToString<safety_regulations, safety_regulations::no>()
    {
        return "no_safety";
    }

    template <>
    char const * ToString<safety_regulations, safety_regulations::trinket>()
    {
        return "trinket_safety";
    }

    template <>
    char const * ToString<safety_regulations, safety_regulations::low>()
    {
        return "low_safety";
    }

    template <>
    char const * ToString<safety_regulations, safety_regulations::acceptable>()
    {
        return "acceptable_safety";
    }

    template <>
    char const * ToString<safety_regulations, safety_regulations::good>()
    {
        return "good_safety";
    }

    template <>
    char const * ToString<unemployment_subsidies, unemployment_subsidies::no>()
    {
        return "no_subsidies";
    }

    template <>
    char const * ToString<unemployment_subsidies, unemployment_subsidies::trinket>()
    {
        return "trinket_subsidies";
    }

    template <>
    char const * ToString<unemployment_subsidies, unemployment_subsidies::low>()
    {
        return "low_subsidies";
    }

    template <>
    char const * ToString<unemployment_subsidies, unemployment_subsidies::acceptable>()
    {
        return "acceptable_subsidies";
    }

    template <>
    char const * ToString<unemployment_subsidies, unemployment_subsidies::good>()
    {
        return "good_subsidies";
    }

    template <>
    char const * ToString<pensions, pensions::no>()
    {
        return "no_pensions";
    }

    template <>
    char const * ToString<pensions, pensions::trinket>()
    {
        return "trinket_pensions";
    }

    template <>
    char const * ToString<pensions, pensions::low>()
    {
        return "low_pensions";
    }

    template <>
    char const * ToString<pensions, pensions::acceptable>()
    {
        return "acceptable_pensions";
    }

    template <>
    char const * ToString<pensions, pensions::good>()
    {
        return "good_pensions";
    }

    template <>
    char const * ToString<health_care, health_care::no>()
    {
        return "no_health_care";
    }

    template <>
    char const * ToString<health_care, health_care::trinket>()
    {
        return "trinket_health_care";
    }

    template <>
    char const * ToString<health_care, health_care::low>()
    {
        return "low_health_care";
    }

    template <>
    char const * ToString<health_care, health_care::acceptable>()
    {
        return "acceptable_health_care";
    }

    template <>
    char const * ToString<health_care, health_care::good>()
    {
        return "good_health_care";
    }

    template <>
    char const * ToString<school_reforms, school_reforms::no>()
    {
        return "no_schools";
    }

    template <>
    char const * ToString<school_reforms, school_reforms::low>()
    {
        return "low_schools";
    }

    template <>
    char const * ToString<school_reforms, school_reforms::acceptable>()
    {
        return "acceptable_schools";
    }

    template <>
    char const * ToString<school_reforms, school_reforms::good>()
    {
        return "good_schools";
    }

    template <>
    char const * ToString<slavery, slavery::yes>()
    {
        return "yes_slavery";
    }

    template <>
    char const * ToString<slavery, slavery::no>()
    {
        return "no_slavery";
    }

    template <>
    char const * ToString<vote_franschise, vote_franschise::none>()
    {
        return "none_voting";
    }

    template <>
    char const * ToString<vote_franschise, vote_franschise::landed>()
    {
        return "landed_voting";
    }

    template <>
    char const * ToString<vote_franschise, vote_franschise::wealth_weighted>()
    {
        return "wealth_weighted_voting";
    }

    template <>
    char const * ToString<vote_franschise, vote_franschise::wealth>()
    {
        return "wealth_voting";
    }

    template <>
    char const * ToString<vote_franschise, vote_franschise::universal_weighted>()
    {
        return "universal_weighted_voting";
    }

    template <>
    char const * ToString<vote_franschise, vote_franschise::universal>()
    {
        return "universal_voting";
    }

    template <>
    char const * ToString<upper_house_composition, upper_house_composition::party_appointed>()
    {
        return "party_appointed";
    }

    template <>
    char const * ToString<upper_house_composition, upper_house_composition::appointed>()
    {
        return "appointed";
    }

    template <>
    char const * ToString<upper_house_composition, upper_house_composition::state_equal_weight>()
    {
        return "state_equal_weight";
    }

    template <>
    char const * ToString<upper_house_composition, upper_house_composition::population_equal_weight>()
    {
        return "population_equal_weight";
    }

    template <>
    char const * ToString<voting_system, voting_system::first_past_the_post>()
    {
        return "first_past_the_post";
    }

    template <>
    char const * ToString<voting_system, voting_system::jefferson_method>()
    {
        return "jefferson_method";
    }

    template <>
    char const * ToString<voting_system, voting_system::proportional_representation>()
    {
        return "proportional_representation";
    }

    template <>
    char const * ToString<public_meetings, public_meetings::no>()
    {
        return "no_meeting";
    }

    template <>
    char const * ToString<public_meetings, public_meetings::yes>()
    {
        return "yes_meeting";
    }

    template <>
    char const * ToString<press_rights, press_rights::state>()
    {
        return "state_press";
    }

    template <>
    char const * ToString<press_rights, press_rights::censored>()
    {
        return "censored_press";
    }

    template <>
    char const * ToString<press_rights, press_rights::free>()
    {
        return "free_press";
    }

    template <>
    char const * ToString<trade_unions, trade_unions::no_trade_unions>()
    {
        return "no_trade_unions";
    }

    template <>
    char const * ToString<trade_unions, trade_unions::state_controlled>()
    {
        return "state_controlled";
    }

    template <>
    char const * ToString<trade_unions, trade_unions::non_socialist>()
    {
        return "non_socialist";
    }

    template <>
    char const * ToString<trade_unions, trade_unions::all_trade_unions>()
    {
        return "all_trade_unions";
    }

    template <>
    char const * ToString<political_parties, political_parties::underground_parties>()
    {
        return "underground_parties";
    }

    template <>
    char const * ToString<political_parties, political_parties::harassment>()
    {
        return "harassment";
    }

    template <>
    char const * ToString<political_parties, political_parties::gerrymandering>()
    {
        return "gerrymandering";
    }

    template <>
    char const * ToString<political_parties, political_parties::non_secret_ballots>()
    {
        return "non_secret_ballots";
    }

    template <>
    char const * ToString<political_parties, political_parties::secret_ballots>()
    {
        return "secret_ballots";
    }

}
