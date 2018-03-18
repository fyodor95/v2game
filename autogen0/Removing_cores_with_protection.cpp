#include <bits/stdc++.h>
#include "rus_countries.h"
using namespace std;
int main() {
    ifstream c("hist_countries.txt");
    ifstream cult("cultures.txt");
    ifstream rus("rus_text4.csv");
    //ofstream f("countries_information.txt");
    //ofstream f_obr("tag_narod.txt");
    //ofstream rc("removing_cores.txt");
    ofstream pc("..\\events\\removing_cores.txt");
    ofstream loc("..\\localisation\\removing_cores.csv");
    //ofstream sc("removing_cores.txt");
    const string prim_cult = "primary_culture", way = "c:", leader = "leader", unit = "unit";
    string s, s_old, tag, what_prim_cult, cur_cult_gr, loc1, loc2;
    const unordered_map<string, string> rus_countries = GetRusCountries();
    getline(rus, loc1);
    getline(rus, loc2);
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

    int id = 148000;
    for (auto it = tag_narod.begin(); it != tag_narod.end(); ++it) {
		 pc << "province_event = {\n";
		 pc << "	id = " << ++id << "\n";
		 pc << "	title = \"REMOVING_CORE\"\n";
		 pc << "	desc = \"EV" << id << "\"\n";
		 pc << "	trigger = {\n";
		 pc << "		is_core = " << it->first << "\n";
		 const auto writeCultures = [&pc, &it](const string& tab) {
             if ((it->second).size() == 1) {
                 pc << tab << "NOT = { culture = " << (*((it->second).begin())) << " }\n";
             } else {
                 pc << tab << "NOT = {\n";
                 pc << tab << "\tOR = {\n";
                 for (auto i = (it->second).begin(); i != (it->second).end(); ++i) {
                     pc << tab << "\t\tculture = " << (*i) << '\n';
                 }
                 pc << tab << "\t}\n";
                 pc << tab << "}\n";
             }
		 };
		 writeCultures("\t\t");
		 pc << "		" << it->first << " = { exists = no }\n";
		 pc << "		OR = {\n";
		 pc << "			port = yes\n";
		 pc << "			any_neighbor_province = {\n";
		 pc << "				AND = {\n";
		 pc << "					NOT = { is_core = " << it->first << " }\n";
		 writeCultures("\t\t\t\t\t");
		 pc << "				}\n";
		 pc << "			}\n";
		 pc << "		}\n";
		 pc << "	}\n";
		 pc << "	mean_time_to_happen = {\n";
		 pc << "		months = 1200\n";
		 pc << "	}\n";
		 pc << "	option = {\n";
		 pc << "		name = \"OK\"\n";
		 pc << "		remove_core = " << it->first << "\n";
		 pc << "	}\n";
		 pc << "}\n";
		 loc << "EV" << id << ';' << loc1 << "$PROVINCENAME$" << loc2 << rus_countries.at(it->first) << ".;X\n";
    }
}
