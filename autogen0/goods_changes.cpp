#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

int id = 200;
std::ofstream out("..\\events\\goods_changes.txt");
std::vector<std::string> market_functionality_techs = {
    "freedom_of_trade",
    "market_structure",
    "business_regulations",
    "market_regulations",
    "economic_responsibility",
    "government_interventionism"
};

void make(const std::unordered_map<std::string, std::unordered_set<std::string>>& products,
          const std::string& type, const std::string& special_product) {
    for (const auto& current_product : products) {
        out << "province_event = {\n";
        out << "	id = " << ++id << "\n";
        out << "	trigger = {\n";
        out << "		controlled_by = owner\n";
        if (current_product.first == "fish") {
            out << "		is_coastal = yes\n";
        }
        if (current_product.first != special_product) {
            out << "		OR = {\n";
            for (const auto& product : products) {
                if (product.first != special_product && product.first != current_product.first) {
                    out << "			trade_goods = " << product.first << "\n";
                }
            }
            out << "		}\n";
        } else {
            out << "		trade_goods = " << (type == "farmers" ? "grain" : "timber") << "\n";
        }
        out << "		unemployment_by_type = {\n";
        out << "			type = " << type << "\n";
        out << "			value = 0.4\n";
        out << "		}\n";
        if (current_product.first == "rubber") {
            out << "		OR = {\n";
            out << "			owner = {\n";
            out << "				part_of_sphere = yes\n";
            out << "				sphere_owner = {\n";
            out << "					OR = {\n";
            out << "						invention = daimlers_automobile\n";
            out << "						invention = edison_light_bulb\n";
            out << "						infiltration = 1\n";
            out << "						aeronautics = 1\n";
            out << "					}\n";
            out << "				}\n";
            out << "			}\n";
            out << "			owner = {\n";
            out << "				OR = {\n";
            out << "					invention = daimlers_automobile\n";
            out << "					invention = edison_light_bulb\n";
            out << "					infiltration = 1\n";
            out << "					aeronautics = 1\n";
            out << "				}\n";
            out << "			}\n";
            out << "			year = 1900\n";
            out << "		}\n";
        } else if (current_product.first == "oil") {
            out << "		OR = {\n";
            out << "			owner = {\n";
            out << "				part_of_sphere = yes\n";
            out << "				sphere_owner = {\n";
            out << "					OR = {\n";
            out << "						invention = daimlers_automobile\n";
            out << "						invention = cracking\n";
            out << "						invention = protected_cruisers\n";
            out << "					}\n";
            out << "				}\n";
            out << "			}\n";
            out << "			owner = {\n";
            out << "				OR = {\n";
            out << "					invention = daimlers_automobile\n";
            out << "					invention = cracking\n";
            out << "					invention = protected_cruisers\n";
            out << "				}\n";
            out << "			}\n";
            out << "		}\n";
        }
        if (current_product.second.size()) {
            if (current_product.second.find("not") != current_product.second.end()) {
                for (const auto& continent : current_product.second) {
                    if (continent != "not") {
                        out << "		NOT = { continent = " << continent << " }\n";
                    }
                }
            } else if (current_product.second.size() == 1) {
                out << "		continent = " << *(current_product.second.begin()) << "\n";
            } else {
                out << "		OR = {\n";
                for (const auto& continent : current_product.second) {
                    out << "			continent = " << continent << "\n";
                }
                out << "		}\n";
            }
        }
        out << "	}\n";
        out << "	mean_time_to_happen = {\n";
        out << "		months = 12000\n";
        for (float i = 0.5; i < 1.05; i += 0.1) {
            out << "		modifier = {\n";
            out << "			factor = 0.5\n";
            out << "			unemployment_by_type = {\n";
            out << "				type = " << type << "\n";
            out << "				value = " << i << "\n";
            out << "			}\n";
            out << "		}\n";
        }
        for (const auto& tech : market_functionality_techs) {
            out << "		modifier = {\n";
            out << "			factor = 0.5\n";
            out << "			owner = {\n";
            out << "				" << tech << " = 1\n";
            out << "			}\n";
            out << "		}\n";
        }
        out << "		modifier = {\n";
        out << "			factor = 4\n";
        out << "			owner = {\n";
        out << "				economic_policy = planned_economy\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 2\n";
        out << "			owner = {\n";
        out << "				economic_policy = state_capitalism\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.5\n";
        out << "			owner = {\n";
        out << "				economic_policy = interventionism\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.25\n";
        out << "			owner = {\n";
        out << "				economic_policy = laissez_faire\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 256\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 64\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = extremely_low_immigration\n";
        out << "			}\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "					OR = {\n";
        out << "						has_province_modifier = lower_immigration\n";
        out << "						has_province_modifier = low_immigration\n";
        out << "						has_province_modifier = medium_immigration\n";
        out << "						has_province_modifier = high_immigration\n";
        out << "						has_province_modifier = higher_immigration\n";
        out << "						has_province_modifier = extremely_high_immigration\n";
        out << "						has_province_modifier = highest_immigration\n";
        out << "					}\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 16\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = lower_immigration\n";
        out << "			}\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "					OR = {\n";
        out << "						has_province_modifier = low_immigration\n";
        out << "						has_province_modifier = medium_immigration\n";
        out << "						has_province_modifier = high_immigration\n";
        out << "						has_province_modifier = higher_immigration\n";
        out << "						has_province_modifier = extremely_high_immigration\n";
        out << "						has_province_modifier = highest_immigration\n";
        out << "					}\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 4\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = low_immigration\n";
        out << "			}\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "					OR = {\n";
        out << "						has_province_modifier = medium_immigration\n";
        out << "						has_province_modifier = high_immigration\n";
        out << "						has_province_modifier = higher_immigration\n";
        out << "						has_province_modifier = extremely_high_immigration\n";
        out << "						has_province_modifier = highest_immigration\n";
        out << "					}\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.5\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = medium_immigration\n";
        out << "			}\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "					OR = {\n";
        out << "						has_province_modifier = high_immigration\n";
        out << "						has_province_modifier = higher_immigration\n";
        out << "						has_province_modifier = extremely_high_immigration\n";
        out << "						has_province_modifier = highest_immigration\n";
        out << "					}\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.25\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = high_immigration\n";
        out << "			}\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "					OR = {\n";
        out << "						has_province_modifier = higher_immigration\n";
        out << "						has_province_modifier = extremely_high_immigration\n";
        out << "						has_province_modifier = highest_immigration\n";
        out << "					}\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.13\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = higher_immigration\n";
        out << "			}\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "					OR = {\n";
        out << "						has_province_modifier = extremely_high_immigration\n";
        out << "						has_province_modifier = highest_immigration\n";
        out << "					}\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.06\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = extremely_high_immigration\n";
        out << "			}\n";
        out << "			NOT = {\n";
        out << "				any_neighbor_province = {\n";
        out << "					trade_goods = " << current_product.first << "\n";
        out << "					has_province_modifier = highest_immigration\n";
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.03\n";
        out << "			any_neighbor_province = {\n";
        out << "				trade_goods = " << current_product.first << "\n";
        out << "				has_province_modifier = highest_immigration\n";
        out << "			}\n";
        out << "		}\n";
        out << "	}\n";
        out << "	title = \"EVTNAME2XX\"\n";
        out << "	desc = \"EVTDESC2XX\"\n";
        out << "	option = {\n";
        out << "		name = \"EVTOPTA2XX\"\n";
        out << "		trade_goods = " << current_product.first << "\n";
        if (current_product.first == special_product && type == "labourers") {
            out << "		any_pop = {\n";
            out << "			limit = {\n";
            out << "				type = labourers\n";
            out << "			}\n";
            out << "			pop_type = farmers\n";
            out << "		}\n";
        }
        out << "	}\n";
        out << "}\n";
    }
}

int main() {
    out.precision(1);
    std::unordered_map<std::string, std::unordered_set<std::string>> farmer_products, labourer_products;
    farmer_products["cattle"] = {};
    farmer_products["fish"] = {};
    farmer_products["fruit"] = {};
    farmer_products["grain"] = {};
    farmer_products["tobacco"] = {};
    farmer_products["tea"] = { "asia", "africa" };
    farmer_products["coffee"] = { "south_america", "africa" };
    farmer_products["opium"] = { "asia" };
    farmer_products["cotton"] = {};
    farmer_products["wool"] = {};
    farmer_products["silk"] = { "asia", "europe" };
    farmer_products["dye"] = { "asia", "north_america" };
    farmer_products["timber"] = {};
    labourer_products["oil"] = { "not", "oceania" };
    labourer_products["rubber"] = { "africa", "asia", "south_america" };
    labourer_products["tropical_wood"] = { "not", "europe", "north_america" };
    labourer_products["timber"] = {};
    labourer_products["iron"] = { "not", "oceania" };
    labourer_products["sulphur"] = { "not", "oceania", "africa" };
    labourer_products["coal"] = {};
    labourer_products["grain"] = {};
    make(farmer_products, "farmers", "timber");
    make(labourer_products, "labourers", "grain");
}
