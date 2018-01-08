#include <array>
#include <fstream>
#include <string>

int main() {
    std::ofstream out("..\\events\\ideology_changing.txt");
    const std::array<std::string, 7> ideologies{"liberal", "reactionary", "conservative", "socialist",
        "communist", "fascist", "anarcho_liberal"};
    const std::array<std::string, 7> modifiers{"extremely_low_immigration", "lower_immigration",
        "low_immigration", "high_immigration", "higher_immigration", "extremely_high_immigration",
        "highest_immigration"};
    const std::array<double, 7> coefficients{0.0014, 0.001, 0.0005, 0.0025, 0.005, 0.0075, 0.001};
    out << "country_event = {\n";
    out << "	id = 100001\n";
    out << "	title = \"0\"\n";
    out << "	desc = \"0\"\n";
    out << "	picture = \"greatpowers\"\n";
    out << "	trigger = {\n";
    out << "		AI = yes\n";
    out << "	}\n";
    out << "	mean_time_to_happen = {\n";
    out << "		months = 1\n";
    out << "	}\n";
    out << "	option = {\n";
    out << "		name = \"0\"\n";
    for (int c = 1; c < 2699; c *= 2) {
        for (int id = 0; id < 7; ++id) {
            out << "		random_country = {\n";
            out << "			limit = {\n";
            if (c > 1)
                out << "				num_of_cities = " << c << "\n";
            if (id > 2)
                out << "				is_ideology_enabled = " << ideologies[id] << "\n";
            out << "				NOT = { ruling_party_ideology = " << ideologies[id] << " }\n";
            out << "			}\n";
            for (int mod = 0; mod < 3; ++mod) {
                out << "			random_owned = {\n";
                out << "				limit = {\n";
                out << "					has_province_modifier = " << modifiers[mod] << "\n";
                out << "				}\n";
                out << "				any_pop = {\n";
                out << "					ideology = {\n";
                out << "						value = " << ideologies[id] << "\n";
                out << "						factor = " << coefficients[mod] << "\n";
                out << "					}\n";
                out << "				}\n";
                out << "			}\n";
            }
            out << "		}\n";
        }
        for (int id = 0; id < 7; ++id) {
            out << "		random_country = {\n";
            out << "			limit = {\n";
            if (c > 1)
                out << "				num_of_cities = " << c << "\n";
            out << "				ruling_party_ideology = " << ideologies[id] << "\n";
            out << "			}\n";
            for (int mod = 3; mod < 7; ++mod) {
                out << "			random_owned = {\n";
                out << "				limit = {\n";
                out << "					has_province_modifier = " << modifiers[mod] << "\n";
                out << "				}\n";
                out << "				any_pop = {\n";
                out << "					ideology = {\n";
                out << "						value = " << ideologies[id] << "\n";
                out << "						factor = " << coefficients[mod] << "\n";
                out << "					}\n";
                out << "				}\n";
                out << "			}\n";
            }
            out << "		}\n";
        }
    }
    out << "	}\n";
    out << "}\n";
}
