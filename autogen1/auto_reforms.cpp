#include <fstream>
#include <string>
#include <vector>

int main()
{
    using reform_t = std::pair<std::string, std::vector<std::string>>;
    const std::vector<reform_t> reforms
    {
        reform_t("wage_reform", {"no_minimum_wage", "trinket_wage", "low_minimum_wage", "acceptable_minimum_wage", "good_minimum_wage"}),
        reform_t("work_hours", {"no_work_hour_limit", "fourteen_hours", "twelve_hours", "ten_hours", "eight_hours"}),
        reform_t("safety_regulations", {"no_safety", "trinket_safety", "low_safety", "acceptable_safety", "good_safety"}),
        reform_t("unemployment_subsidies", {"no_subsidies", "trinket_subsidies", "low_subsidies", "acceptable_subsidies", "good_subsidies"}),
        reform_t("pensions", {"no_pensions", "trinket_pensions", "low_pensions", "acceptable_pensions", "good_pensions"}),
        reform_t("health_care", {"no_health_care", "trinket_health_care", "low_health_care", "acceptable_health_care", "good_health_care"}),
        reform_t("school_reforms", {"no_schools", "low_schools", "acceptable_schools", "good_schools"})
    };
    std::ofstream out("..\\events\\auto_reforms.txt");
    int id = 75000;
    const int time = 300;
    for (const reform_t& reform : reforms)
    {
        for (size_t i = 0; i < reform.second.size(); ++i)
        {
            if (i)
            {
                out << "country_event = {\n";
                out << "	id = " << ++id << "\n";
                out << "	title = \"EVTNAME75XXX\"\n";
                out << "	desc = \"EVTDESC75XXX\"\n";
                out << "	picture = \"Administration\"\n";
                out << "	\n";
                out << "	trigger = {\n";
                out << "		civilized = yes\n";
                out << "		NOT = {\n";
                out << "			OR = {\n";
                out << "				ruling_party_ideology = communist\n";
                out << "				ruling_party_ideology = socialist\n";
                out << "			}\n";
                out << "		}\n";
                out << "		" << reform.first << " = " << reform.second[i] << "\n";
                if (i != reform.second.size() - 1)
                    out << "		NOT = { " << reform.first << " = " << reform.second[i + 1] << " }\n";
                out << "	}\n";
                out << "	mean_time_to_happen = {\n";
                out << "		months = " << time << "\n";
                out << "		modifier = {\n";
                out << "			factor = 0.5\n";
                out << "			OR = {\n";
                out << "				ruling_party_ideology = liberal\n";
                out << "				ruling_party_ideology = anarcho_liberal\n";
                out << "			}\n";
                out << "		}\n";
                out << "		modifier = {\n";
                out << "			factor = 0.25\n";
                out << "			ruling_party_ideology = fascist\n";
                out << "		}\n";
                out << "	}\n";
                out << "	option = {\n";
                out << "		name = \"EVTOPTA75XXX\"\n";
                out << "		social_reform = " << reform.second[i - 1] << "\n";
                out << "		poor_strata = {\n";
                out << "			militancy = 2\n";
                out << "			consciousness = 2\n";
                out << "		}\n";
                out << "		middle_strata = {\n";
                out << "			militancy = 1\n";
                out << "			consciousness = 1\n";
                out << "		}\n";
                out << "		rich_strata = {\n";
                out << "			militancy = -2\n";
                out << "			consciousness = -2\n";
                out << "		}\n";
                out << "	}\n";
                out << "}\n";
            }
            if (i != reform.second.size() - 1)
            {
                out << "country_event = {\n";
                out << "	id = " << ++id << "\n";
                out << "	title = \"EVTNAME75XXX2\"\n";
                out << "	desc = \"EVTDESC75XXX2\"\n";
                out << "	picture = \"Administration\"\n";
                out << "	trigger = {\n";
                out << "		civilized = yes\n";
                out << "		ruling_party_ideology = communist\n";
                out << "		" << reform.first << " = " << reform.second[i] << "\n";
                out << "		NOT = { " << reform.first << " = " << reform.second[i + 1] << " }\n";
                out << "	}\n";
                out << "	\n";
                out << "	mean_time_to_happen = {\n";
                out << "		months = " << time << "\n";
                out << "	}\n";
                out << "	option = {\n";
                out << "		name = \"EVTOPTA75XXX2\"\n";
                out << "		social_reform = " << reform.second[i + 1] << "\n";
                out << "	}\n";
                out << "}\n";
            }
        }
    }
}
