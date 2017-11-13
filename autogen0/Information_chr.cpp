#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream c("hist_countries_chr.txt");
    ofstream f("countries_information_chr.txt");
    const string prim_cult = "primary_culture", way = "c:";
    string s, s_old, tag, what_prim_cult;
    vector<string> init;
    unordered_map<string, vector<string>> dict;
    unordered_map<string, int> dict_sizes;
    while (getline(c, s)) {
        if (s.substr(0, 2) == way) {
            //cout << "TRUE";
            tag = s.substr(83, 3);
        }
        if (s.substr(0, 15) == prim_cult) {
            what_prim_cult = s.substr(18);
            //++dict_sizes[what_prim_cult];
            //if (dict_sizes[what_prim_cult] == 1) dict[what_prim_cult] = init;
            dict[what_prim_cult].push_back(tag);
            //f << tag << '\n';
        }
    }
    for (auto it = dict.begin(); it != dict.end(); ++it) {
        f << it->first << ":\n";
        for (int i = 0; i < (it->second).size(); ++i) {
            f << '\t' << (it->second)[i] << '\n';
        }
    }
    c.close();
    f.close();
    /*char tag[4], musor[100], put[100];
    int s = 1472073;
    FILE *f, *c;
    c = fopen("hist_countries.txt", "r");
    f = fopen("countries_information.txt", "w");

    while (fgets(tag, 4, c)) {

    fprintf(f, "country_event = {\n");
    fprintf(f, "	id = %d\n", s);
    ++s;
    fprintf(f, "	title = \"0\"\n");
    fprintf(f, "	desc = \"0\"\n");
    fprintf(f, "	picture = \"greatpowers\"\n");
    fprintf(f, "	trigger = {\n");
    fprintf(f, "		tag = %s\n", tag);
    fprintf(f, "		AI = yes\n");
    fprintf(f, "		OR = {\n");
    fprintf(f, "			total_pops = 1000000\n");
    fprintf(f, "			AND = {\n");
    fprintf(f, "				military_score = 1\n");
    fprintf(f, "				is_independant = yes\n");
    fprintf(f, "				rank = 100\n");
    fprintf(f, "			}\n");
    fprintf(f, "		}\n");
    fprintf(f, "	}\n");
    fprintf(f, "	mean_time_to_happen = {\n");
    fprintf(f, "		months = 1200\n");

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 0.9\n");
    fprintf(f, "			OR = {\n");
    fprintf(f, "				tag = RUS\n");
    fprintf(f, "				tag = SOV\n");
    fprintf(f, "			}\n");
    fprintf(f, "		}\n");

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 0.95\n");
    fprintf(f, "			civilized = yes\n");
    fprintf(f, "		}\n");

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 0.95\n");
    fprintf(f, "			rank = 16\n");
    fprintf(f, "		}\n");

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 0.95\n");
    fprintf(f, "			is_greater_power = yes\n");
    fprintf(f, "		}\n");

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 0.95\n");
    fprintf(f, "			industrial_score = 100\n");
    fprintf(f, "		}\n");

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 0.95\n");
    fprintf(f, "			industrial_score = 1000\n");
    fprintf(f, "		}\n");
    fprintf(f, "	}\n");
    fprintf(f, "	option = {\n");
    fprintf(f, "		name = \"0\"\n");
    fprintf(f, "		random_country = {\n");
    fprintf(f, "			limit = {\n");
    fprintf(f, "				AI = no\n");
    fprintf(f, "			}\n");
    fprintf(f, "			country_event = { id=%d days=0 }\n", s);
    fprintf(f, "		}\n");
    fprintf(f, "	}\n");
    fprintf(f, "}\n\n");




    fprintf(f, "country_event = {\n");
    fprintf(f, "	id = %d\n", s);
    ++s;
    fprintf(f, "	title = \"CHANGE_YOUR_WORK\"\n");
    fprintf(f, "	desc = \"CHANGE_YOUR_WORK_desk\"\n");
    fprintf(f, "	picture = \"Celebration\"\n");

    fprintf(f, "	is_triggered_only = yes\n");

    fprintf(f, "	option = {\n");
    fprintf(f, "		name = \"CHANGE_YOUR_WORK_ok\"\n");
    fprintf(f, "		change_tag_no_core_switch = %s\n", tag);
    fprintf(f, "	}\n");
    fprintf(f, "}\n\n");

    fgets(musor, 100, c);

    }



    fclose(f);*/
}
