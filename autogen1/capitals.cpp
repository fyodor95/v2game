#include <fstream>

using uint = unsigned;

int main()
{
    uint constexpr n = 6;
    uint constexpr startScore = 200;
    uint constexpr startPopSize = 200000;
    std::ofstream out("..\\events\\Capitals.txt");
    out << "country_event = {\n";
    out << "	id = 1472049\n";
    out << "	title = \"x\"\n";
    out << "	desc = \"x\"\n";
    out << "	picture = \"greatpowers\"\n";
    out << "	trigger = {\n";
    out << "		AI = yes\n";
    out << "		any_greater_power = {\n";
    out << "			industrial_score = 100\n";
    out << "		}\n";
    out << "	}\n";
    out << "	mean_time_to_happen = {\n";
    out << "		months = 60\n";
    out << "		modifier = {\n";
    out << "			factor = 100\n";
    out << "			NOT = {\n";
    out << "				rank = 100\n";
    out << "			}\n";
    out << "		}\n";
    out << "	}\n";
    out << "	immediate = {\n";
    out << "		any_country = {\n";
    out << "			any_owned = {\n";
    out << "				remove_province_modifier = our_capital\n";
    for (uint i = 1; i <= n; ++i)
        out << "				remove_province_modifier = prom_center_" << i << "\n";
    out << "			}\n";
    out << "		}\n";
    out << "	}\n";
    out << "	option = {\n";
    out << "		name = \"x\"\n";
    out << "		any_greater_power = {\n";
    out << "			limit = {\n";
    out << "				industrial_score = 100\n";
    out << "			}\n";
    out << "			capital_scope = {\n";
    out << "				add_province_modifier = {\n";
    out << "					name = our_capital\n";
    out << "					duration = -1\n";
    out << "				}\n";
    out << "			}\n";
    out << "		}\n";
    for (uint i = 1, curScore = startScore, nextScore = startScore * 2; i <= n;
         ++i, curScore = nextScore, nextScore *= 2)
    {
        out << "		any_country = {\n";
        out << "			limit = {\n";
        out << "				industrial_score = " << curScore << "\n";
        if (i != n)
            out << "				NOT = { industrial_score = " << nextScore << " }\n";
        out << "			}\n";
        uint curPopSize = startPopSize;
        for (uint j = 1, curPopSize = startPopSize, nextPopSize = startPopSize * 2; j <= i;
             ++j, curPopSize = nextPopSize, nextPopSize *= 2)
        {
            out << "			any_owned = {\n";
            out << "				limit = {\n";
            out << "					is_colonial = no\n";
            out << "					total_pops = " << curPopSize << "\n";
            if (i != j)
                out << "					NOT = { total_pops = " << nextPopSize << " }\n";
            out << "					has_pop_type = craftsmen\n";
            out << "					NOT = {\n";
            out << "						unemployment_by_type = {\n";
            out << "							type = craftsmen\n";
            out << "							value = 0.01\n";
            out << "						}\n";
            out << "					}\n";
            out << "				}\n";
            out << "				add_province_modifier = {\n";
            out << "					name = prom_center_" << j << "\n";
            out << "					duration = -1\n";
            out << "				}\n";
            out << "			}\n";
        }
        out << "		}\n";
    }
    out << "	}\n";
    out << "}\n";
}
