#include <bits/stdc++.h>
using namespace std;
int main() {
    //ifstream c("hist_countries.txt");
    ifstream cult("cultures.txt");
    //ofstream f("countries_information.txt");
    //ofstream f_obr("tag_narod.txt");
    ofstream arc("Assimilation_of_relative_cultures.txt");
    //ofstream sc("removing_cores.txt");
    const string prim_cult = "primary_culture", way = "c:", leader = "leader", unit = "unit";
    string s, s_old, tag, what_prim_cult, cur_cult_gr;
    vector<string> init;
    unordered_map<string, unordered_set<string>> dict, tag_narod;
    unordered_map<string, int> dict_sizes;
    unordered_map<string, string> cult_union;
    unordered_set<string> bad_tags = {"SOV"}, all_union_cult;//, "RHO", "WIA"
	arc << "province_event = {\n";
	arc << "\n";
	arc << "	id = 1260\n";
	arc << "\n";
	arc << "	trigger = {\n";
	arc << "		is_primary_culture = no\n";
	arc << "		is_accepted_culture = yes\n";
	arc << "		any_neighbor_province = {\n";
	arc << "			owned_by = THIS\n";
	arc << "			is_primary_culture = yes\n";
	arc << "		}\n";
	arc << "		OR = {\n";
    while (getline(cult, s)) {
        if ('a' <= s[0] && s[0] <= 'z') {
            //cur_cult_gr = s.substr(0, s.find(" = "));
            vector<string> cultures;
            while (getline(cult, s)) {
                if (s[0] == '}') break;
                size_t ff = s.find("union");
                if (ff != string::npos) {
                    string union_tag = s.substr(ff + 8, 3);
                    for (int i = 0; i < cultures.size(); ++i) {
                        dict[cultures[i]].insert(union_tag);
                        tag_narod[union_tag].insert(cultures[i]);
                        //all_union_cult.insert(cultures[i]);
                    }
                    if (cultures.size() > 2) {
						arc << "			AND = {\n";
						arc << "				owned_by = " << union_tag << "\n";
						arc << "				OR = {\n";
						for (int i = 0; i < cultures.size(); ++i) {
							arc << "\t\t\t\t	culture = " << cultures[i] << "\n";
						}
						arc << "				}\n";
						arc << "			}\n";
                    }
                } else {
                    if (s[0] == '\t' && 'a' <= s[1] && s[1] <= 'z' &&
                        s.find(leader) == string::npos && s.find(unit) == string::npos &&
                        s.find("is_overseas") == string::npos) {
                        cultures.push_back(s.substr(1, s.find(" = ") - 1));
                    }
                }
            }
        }
    }
	arc << "		}\n";
	arc << "	}\n";
    arc << "\n";
	arc << "	mean_time_to_happen = {\n";
	arc << "		months = 1800\n";
	arc << "		modifier = {\n";
	arc << "			factor = 1.26\n";
	arc << "			total_pops = 100000\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 1.26\n";
	arc << "			total_pops = 200000\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 1.26\n";
	arc << "			total_pops = 400000\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 2\n";
	arc << "			total_pops = 800000\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 2\n";
	arc << "			total_pops = 1600000\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 4\n";
	arc << "			total_pops = 3200000\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 0.9\n";
	arc << "			any_neighbor_province = {\n";
	arc << "				owned_by = THIS\n";
	arc << "				is_primary_culture = yes\n";
	arc << "				total_pops = 100000\n";
	arc << "			}\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 0.9\n";
	arc << "			any_neighbor_province = {\n";
	arc << "				owned_by = THIS\n";
	arc << "				is_primary_culture = yes\n";
	arc << "				total_pops = 200000\n";
	arc << "			}\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 0.9\n";
	arc << "			any_neighbor_province = {\n";
	arc << "				owned_by = THIS\n";
	arc << "				is_primary_culture = yes\n";
	arc << "				total_pops = 400000\n";
	arc << "			}\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 0.8\n";
	arc << "			any_neighbor_province = {\n";
	arc << "				owned_by = THIS\n";
	arc << "				is_primary_culture = yes\n";
	arc << "				total_pops = 800000\n";
	arc << "			}\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 0.8\n";
	arc << "			any_neighbor_province = {\n";
	arc << "				owned_by = THIS\n";
	arc << "				is_primary_culture = yes\n";
	arc << "				total_pops = 1600000\n";
	arc << "			}\n";
	arc << "		}\n";
	arc << "		modifier = {\n";
	arc << "			factor = 0.6\n";
	arc << "			any_neighbor_province = {\n";
	arc << "				owned_by = THIS\n";
	arc << "				is_primary_culture = yes\n";
	arc << "				total_pops = 3200000\n";
	arc << "			}\n";
	arc << "		}\n";
	arc << "	}\n";
    arc << "\n";
	arc << "	title = \"EVTNAME1260\"\n";
	arc << "	desc = \"EVTDESC1260\"\n";
    arc << "\n";
	arc << "	option = {\n";
	arc << "		name = \"EVTOPTA1260\"\n";
	arc << "		assimilate = yes\n";
	arc << "	}\n";
	arc << "}\n";
}
