#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream c("hist_countries.txt");
    ifstream cult("cultures.txt");
    ofstream pc("..\\events\\after_union.txt");
    ofstream rc("..\\events\\removing_D_cores.txt");
    const string prim_cult = "primary_culture", way = "c:", leader = "leader", unit = "unit";
    string s, tag, what_prim_cult;
    map<string, set<string>> dict, tag_narod;
    set<string> bad_tags = { "SOV" }, union_tags;
    cout << string::npos <<'\n';
    while (getline(cult, s)) {
        if ('a' <= s[0] && s[0] <= 'z') {
            vector<string> cultures;
            while (getline(cult, s)) {
                if (s[0] == '}') break;
                size_t ff = s.find("union");
                if (ff != string::npos) {
                    string union_tag = s.substr(ff + 8, 3);
                    if (union_tag == "UCA") continue;
                    for (int i = 0; i < cultures.size(); ++i) {
                        dict[cultures[i]].insert(union_tag);
                        union_tags.insert(union_tag);
                        tag_narod[union_tag].insert(cultures[i]);
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
    dict["north_german"].insert("NGF");
    union_tags.insert("NGF");
    tag_narod["NGF"].insert("north_german");
    dict["south_german"].insert("SGF");
    union_tags.insert("SGF");
    tag_narod["SGF"].insert("south_german");
    rc << "country_event = {\n";
    rc <<   "\tid = 145000\n";
    rc <<   "\ttitle = \"0\"\n";
    rc <<   "\tdesc = \"0\"\n";
    rc <<   "\tpicture = \"greatpowers\"\n";
    rc <<   "\ttrigger = {\n";
    rc <<     "\t\tAI = yes\n";
    rc <<   "\t}\n";
    rc <<   "\tmean_time_to_happen = {\n";
    rc <<     "\t\tmonths = 120\n";
    rc <<   "\t}\n";
    rc <<   "\toption = {\n";
    rc <<   "\t\tname = \"0\"\n";
    while (getline(c, s)) {
        if (s.substr(0, 2) == way) tag = s.substr(35, 3);
        if (s.substr(0, 15) == prim_cult) {
            what_prim_cult = s.substr(18);
            if (tag[0] == 'D' && '0' <= tag[1] && tag[1] <= '5') {
                rc << "\t\tany_country = {\n";
                rc << "\t\t\tlimit = {\n";
                rc << "\t\t\t\ttag = " << tag << "\n";
                rc << "\t\t\t\texists = no\n";
                rc << "\t\t\t}\n";
                rc << "\t\t\tall_core = {\n";
                rc << "\t\t\t\tremove_core = " << tag << "\n";
                rc << "\t\t\t}\n";
                rc << "\t\t\tprimary_culture = british\n";
                rc << "\t\t}\n";
                continue;
            }
            if (bad_tags.find(tag) != bad_tags.end()) continue;
            if (dict.find(what_prim_cult) != dict.end()) {
                dict[what_prim_cult].insert(tag);
                tag_narod[tag].insert(what_prim_cult);
            }
        }
    }
    rc << "\t}\n";
    rc << "}\n";
    c.close();
    int id = 151000;
    for (auto & union_tag : union_tags) {
        for (auto & narod : tag_narod[union_tag]) {
         if (dict[narod].size() == 1) continue;
         pc << "country_event = {\n";
         pc << "	id = " << ++id << "\n";
         pc << "	title = \"0\"\n";
         pc << "	desc = \"0\"\n";
         pc << "	picture = \"greatpowers\"\n";
         pc << "	trigger = {\n";
         pc << "		" << union_tag << " = {\n";
         pc << "			exists = yes\n";
         pc << "			primary_culture = " << narod << "\n";
         pc << "		}\n";
         pc << "		AI = yes\n";
         pc << "	}\n";
         pc << "	mean_time_to_happen = {\n";
         pc << "		months = 60\n";
         pc << "	}\n";
         pc << "	option = {\n";
         pc << "		name = \"1\"\n";
            for (auto & tag : dict[narod]) {
             if (union_tags.find(tag) != union_tags.end()) continue;
             pc << "		any_country = {\n";
             pc << "			limit = {\n";
             pc << "				tag = " << tag << "\n";
             pc << "				exists = no\n";
             pc << "			}\n";
             pc << "			all_core = {\n";
             pc << "				limit = {\n";
             pc << "					owned_by = " << union_tag << "\n";
             pc << "					is_core = " << union_tag << "\n";
             pc << "				}\n";
             pc << "				remove_core = " << tag << "\n";
             pc << "			}\n";
             pc << "		}\n";
            }
         pc << "	}\n";
         pc << "}\n";
        }
    }
}
