#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream inform("inform.txt");
    ofstream sc("setting_cores.txt");
    const string prim_cult = "primary_culture", way = "c:", leader = "leader", unit = "unit";
    string s, s_old, tag, what_prim_cult, cur_cult_gr;
    vector<string> init;
    unordered_map<string, unordered_set<string>> dict, tag_narod;
    unordered_map<string, int> dict_sizes;
    unordered_map<string, string> cult_union;
    unordered_set<string> bad_tags = {"SOV"}, all_union_cult;//, "RHO", "WIA"


	sc <<  "country_event = {\n";
	sc <<  "	id = 145002\n";
	sc <<  "	title = \"100000\"\n";
	sc <<  "	desc = \"100000\"\n";
	sc <<  "	picture = \"greatpowers\"\n";
	sc <<  "	trigger = {\n";
	sc <<  "		AI = yes\n";
	sc <<  "	}\n";
	sc <<  "	mean_time_to_happen = {\n";
	sc <<  "		months = 60\n";
	sc <<  "	}\n";
	sc <<  "	option = {\n";
	sc <<  "		name = \"10000\"\n";


    bool flag = false;
    if (getline(inform, s)) flag = true;
    while (flag) {
        if (s.size() && 'a' <= s[0] && s[0] <= 'z') {
            string cur_culture = s;
            vector<string> tags;
            flag = false;
            if (getline(inform, s)) flag = true;
            while (flag) {
                if (s[0] != '\t') break;
                tags.push_back(s.substr(1));
                flag = false;
                if (getline(inform, s)) flag = true;
            }


	sc <<  "		any_country = {\n";
	sc <<  "			limit = {\n";
	sc <<  "				is_greater_power = no\n";
	sc <<  "				NOT = { rank = 16 }\n";
	sc <<  "				civilized = yes\n";
	sc <<  "			}\n";
	sc <<  "			any_owned = {\n";
	sc <<  "				limit = {\n";
	sc <<  "					culture = " << cur_culture << "\n";
	sc <<  "					total_pops = 40000\n";
	sc <<  "				}\n";


			for (int i = 0; i < tags.size(); ++i)
	sc <<  "				add_core = " << tags[i] << "\n";


	sc <<  "			}\n";
	sc <<  "		}\n";


        }
    }


	sc <<  "	}\n";
	sc <<  "}\n";


	sc.close();
}
