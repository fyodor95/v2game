#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream c("hist_countries.txt");
    ifstream cult("cultures.txt");
    //ofstream f("countries_information.txt");
    //ofstream f_obr("tag_narod.txt");
    ofstream rc("removing_cores.txt");
    //ofstream sc("removing_cores.txt");
    const string prim_cult = "primary_culture", way = "c:", leader = "leader", unit = "unit";
    string s, s_old, tag, what_prim_cult, cur_cult_gr;
    vector<string> init;
    unordered_map<string, unordered_set<string>> dict, tag_narod;
    unordered_map<string, int> dict_sizes;
    unordered_map<string, string> cult_union;
    unordered_set<string> bad_tags = {"SOV"}, all_union_cult;//, "RHO", "WIA"
    cout << string::npos <<'\n';
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
/*for (auto it = dict.begin(); it != dict.end(); ++it) {
    tf << it->first << ":\n";
    for (auto i = (it->second).begin(); i != (it->second).end(); ++i) {
        tf << '\t' << (*i) << '\n';
    }
}*/
    while (getline(c, s)) {
        if (s.substr(0, 2) == way) {
            //cout << "TRUE";
            tag = s.substr(35, 3);
        }
        if (s.substr(0, 15) == prim_cult) {
            what_prim_cult = s.substr(18);
            ++dict_sizes[what_prim_cult];
            //if (dict_sizes[what_prim_cult] == 1 dict[what_prim_cult] = init;
            if (tag[0] == 'D' && '0' <= tag[1] && tag[1] <= '5') continue;
            if (bad_tags.find(tag) != bad_tags.end()) continue;
            //if (all_union_cult.find(what_prim_cult) != all_union_cult.end()) {
            //    dict[what_prim_cult].insert(cult_union[what_prim_cult]);
            //}
            dict[what_prim_cult].insert(tag);
            tag_narod[tag].insert(what_prim_cult);
            if (tag == string("KUK")) tag_narod[tag].insert("hungarian");
            if (tag == string("SIE")) tag_narod[tag].insert("romanian");
            //f << tag << '\n';
        }
    }
    /*for (auto it = dict.begin(); it != dict.end(); ++it) {
        f << it->first << "\n";
        for (auto i = (it->second).begin(); i != (it->second).end(); ++i) {
            f << '\t' << (*i) << '\n';
        }
    }*/
    /*for (auto it = tag_narod.begin(); it != tag_narod.end(); ++it) {
        f_obr << it->first << ":\n";
        for (auto i = (it->second).begin(); i != (it->second).end(); ++i) {
            f_obr << '\t' << (*i) << '\n';
        }
    }*/
    c.close();
    //f.close();
    //f_obr.close();

    // REMOVING CORES

    rc << "country_event = {\n";
    rc <<   "\tid = 145001\n";
    rc <<   "\ttitle = \"100000\"\n";
    rc <<   "\tdesc = \"100000\"\n";
    rc <<   "\tpicture = \"greatpowers\"\n";
    rc <<   "\ttrigger = {\n";
    rc <<     "\t\tAI = yes\n";
    rc <<   "\t}\n";
    rc <<   "\tmean_time_to_happen = {\n";
    rc <<     "\t\tmonths = 60\n";
    rc <<   "\t}\n";
    rc <<   "\toption = {\n";
    rc <<   "\t\tname = \"10000\"\n";
    for (auto it = tag_narod.begin(); it != tag_narod.end(); ++it) {
        rc << "\t\tany_country = {\n";
        rc << "\t\t\tlimit = {\n";
        rc << "\t\t\t\ttag = " << it->first << "\n";
        rc << "\t\t\t\texists = no\n";
        rc << "\t\t\t}\n";
        rc << "\t\t\tall_core = {\n";
        rc << "\t\t\t\tlimit = {\n";
        if ((it->second).size() == 1) {
            rc << "\t\t\t\t\tNOT = { culture = " << (*((it->second).begin())) << " }\n";
        } else {
            rc << "\t\t\t\t\tNOT = {\n";
            rc << "\t\t\t\t\t\tOR = {\n";
            for (auto i = (it->second).begin(); i != (it->second).end(); ++i) {
                rc << "\t\t\t\t\t\t\tculture = " << (*i) << '\n';
            }
            rc << "\t\t\t\t\t\t}\n";
            rc << "\t\t\t\t\t}\n";
        }
        rc << "\t\t\t\t}\n";
        rc << "\t\t\t\tremove_core = " << it->first << "\n";
        rc << "\t\t\t}\n";
        rc << "\t\t}\n";

    }
    rc << "\t}\n";
    rc << "}\n";

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
