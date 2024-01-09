
d:\v2game\autogen0\Assimilation_of_relative_cultures.cpp
/************************************************************************/
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



d:\v2game\autogen0\Changer.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream c("Kyrgyzstan.txt");
    ofstream f("Tatarstan.txt");
    const string tag1 = "KYR", tag2 = "TTT 	= ";
    string s, s_old, tag, what_prim_cult;
    //string::npos = 999;
    while (getline(c, s)) {
        size_t ff = s.find(tag1);
        if (ff != string::npos) {
            //cout << "TRUE";
            s[ff] = tag2[0];
            s[ff + 1] = tag2[1];
            s[ff + 2] = tag2[2];
        }
        f << s << '\n';
    }
    c.close();
    f.close();
}




d:\v2game\autogen0\CULTURES.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[60];
    FILE *f, *c;
    c = fopen("cultures_orig.txt", "r");
    f = fopen("acceptable_cultures.txt", "w");
    fscanf(f, "political_decisions = {\n\n");
    fprintf(f, "political_decisions = {\n\n");

    while (fgets(culture, 60, c)) {

    for (int i = 0; i < 100; i++) {
        if (culture[i] == '\n' )
        {
            culture[i] = '\0';
            break;
        }
    }

    fprintf(f, "    acceptance_"); fputs(culture, f); fprintf(f, " = {\n\n");
    fprintf(f, "        potential = {\n");
	fprintf(f, "            tag = SOV\n");
    fprintf(f, "            government = proletarian_dictatorship\n");
    fprintf(f, "            NOT = {\n");
    fprintf(f, "                OR = {\n");
    fprintf(f, "                    accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "                    primary_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "                }\n");
    fprintf(f, "            }\n");
    fprintf(f, "            any_owned_province = {\n");
    fprintf(f, "                culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "            }\n");
	fprintf(f, "        }\n\n");

	fprintf(f, "        allow = {\n");
	fprintf(f, "            any_core = {\n");
    fprintf(f, "                culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "                is_colonial = no\n");
	fprintf(f, "            }\n");
	fprintf(f, "        }\n");
	fprintf(f, "        effect = {\n");
	fprintf(f, "            prestige = 1\n");
	fprintf(f, "            add_accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
	fprintf(f, "        }\n");
	fprintf(f, "        ai_will_do = {\n");
	fprintf(f, "            factor = 1\n");
	fprintf(f, "        }\n");
	fprintf(f, "    }\n");


    }
	fprintf(f, "}\n");



    fclose(f);
}



d:\v2game\autogen0\CULTURES_accept_other_contries.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[60];
    FILE *f, *c;
    c = fopen("cultures_orig.txt", "r");
    f = fopen("acceptable_capital_cultures.txt", "w");
    fscanf(f, "political_decisions = {\n\n");
    fprintf(f, "political_decisions = {\n\n");

    while (fgets(culture, 60, c)) {

    for (int i = 0; i < 100; i++) {
        if (culture[i] == '\n' )
        {
            culture[i] = '\0';
            break;
        }
    }

    string sss = string(culture);
    if (sss.find("minor") != string::npos) continue;

    fprintf(f, "    acceptance_capital_"); fputs(culture, f); fprintf(f, " = {\n\n");
    fprintf(f, "        potential = {\n");
    if (culture[0] == 'u' && culture[1] == 'z' && culture[2] == 'b' && culture[3] == 'e' && culture[4] == 'k')
    fprintf(f, "            NOT = { tag = KOK }\n");
    if (culture[0] == 's' && culture[1] == 'o' && culture[2] == 't' && culture[3] == 'h' && culture[4] == 'o') {
    fprintf(f, "            NOT = { tag = TRN }\n");
    fprintf(f, "            NOT = { tag = ORA }\n");
    }
    fprintf(f, "            capital_scope = {\n");
    fprintf(f, "                culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "                NOT = {\n");
    fprintf(f, "                    OR = {\n");
    fprintf(f, "                        continent = north_america\n");
    fprintf(f, "                        continent = south_america\n");
    fprintf(f, "                        continent = oceania\n");
    fprintf(f, "                    }\n");
    fprintf(f, "                }\n");
    fprintf(f, "            }\n");
    fprintf(f, "            NOT = {\n");
    fprintf(f, "                OR = {\n");
    fprintf(f, "                    accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "                    primary_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "                    primary_culture = british\n");
    fprintf(f, "                    tag = SOV\n");
    fprintf(f, "                }\n");
    fprintf(f, "            }\n");
	fprintf(f, "        }\n\n");

	fprintf(f, "        allow = {\n");
    fprintf(f, "            capital_scope = {\n");
    fprintf(f, "                culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "                total_pops = 20000\n");
	fprintf(f, "            }\n");
	fprintf(f, "        }\n");
	fprintf(f, "        effect = {\n");
	fprintf(f, "            prestige = 1\n");
	fprintf(f, "            add_accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
	fprintf(f, "        }\n");
	fprintf(f, "        ai_will_do = {\n");
	fprintf(f, "            factor = 1\n");
	fprintf(f, "        }\n");
	fprintf(f, "    }\n");


    }
	fprintf(f, "}\n");



    fclose(f);
}



d:\v2game\autogen0\CULTURES_accept_other_contries2.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[60];
    FILE *f, *c;
    int s = 1474001;
    c = fopen("cultures.txt", "r");
    f = fopen("accept_not_USSR_cultures.txt", "w");

    while (fgets(culture, 60, c)) {

    for (int i = 0; i < 100; i++) {
        if (culture[i] == '\n' )
        {
            culture[i] = '\0';
            break;
        }
    }
	
    fprintf(f, "country_event = {\n");
    fprintf(f, "	id = %d\n", s);
	++s;
    fprintf(f, "	title = \"EVTNAME1474XXX\"\n");
    fprintf(f, "	desc = \"EVTDESC%d\"\n", s);
	++s;
    fprintf(f, "	picture = \"greatpowers\"\n");
    fprintf(f, "	trigger = {\n");
    fprintf(f, "		capital_scope = { culture = "); fputs(culture, f); fprintf(f, " }\n");
    fprintf(f, "		NOT = {\n");
    fprintf(f, "			OR = {\n");
    fprintf(f, "				accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "				primary_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "			}\n");
    fprintf(f, "		}\n");
    fprintf(f, "	}\n");
    fprintf(f, "	mean_time_to_happen = {\n");
    fprintf(f, "		months = 3\n");
    fprintf(f, "	}\n");
    fprintf(f, "	option = {\n");
    fprintf(f, "		name = \"EVTOPTA%d\"\n", s);
	++s;
    fprintf(f, "		add_accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "		ai_chance = {\n");
    fprintf(f, "			factor = 100\n");
    fprintf(f, "		}\n");
    fprintf(f, "	}\n");
    fprintf(f, "	option = {\n");
    fprintf(f, "		name = \"EVTOPTB%d\"\n", s);
	++s;
    fprintf(f, "	}\n");
    fprintf(f, "}\n");



d:\v2game\autogen0\CULTURES_change_primary.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[60];
    FILE *f, *c;
    int s = 1474001;
    c = fopen("cultures.txt", "r");
    f = fopen("change_primary_cultures.txt", "w");
    fscanf(f, "political_decisions = {\n\n");
    fprintf(f, "political_decisions = {\n\n");

    while (fgets(culture, 60, c)) {

    for (int i = 0; i < 100; i++) {
        if (culture[i] == '\n' )
        {
            culture[i] = '\0';
            break;
        }
    }

    fprintf(f, "    change_primary_"); fputs(culture, f); fprintf(f, " = {\n\n");
    fprintf(f, "        potential = {\n");
    fprintf(f, "            primary_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "            NOT = {\n");
    fprintf(f, "                any_pop = { has_pop_culture = "); fputs(culture, f); fprintf(f, " }\n");
    fprintf(f, "            }\n");
    fprintf(f, "            capital_scope = {\n");
    fprintf(f, "                NOT = {\n");
    fprintf(f, "                    OR = {\n");
    fprintf(f, "                        continent = north_america\n");
    fprintf(f, "                        continent = south_america\n");
    fprintf(f, "                        continent = oceania\n");
    fprintf(f, "                    }\n");
    fprintf(f, "                }\n");
    fprintf(f, "            }\n");
	fprintf(f, "        }\n\n");

	fprintf(f, "        allow = {\n");
    fprintf(f, "            NOT = {\n");
    fprintf(f, "                any_pop = { has_pop_culture = "); fputs(culture, f); fprintf(f, " }\n");
    fprintf(f, "            }\n");
	fprintf(f, "        }\n");
	fprintf(f, "        effect = {\n");
	fprintf(f, "            prestige = -5\n");
	fprintf(f, "            set_country_flag = changing_primary_culture\n", s);
	++s;
	fprintf(f, "        }\n");
	fprintf(f, "        ai_will_do = {\n");
	fprintf(f, "            factor = 1\n");
	fprintf(f, "        }\n");
	fprintf(f, "    }\n");


    }
	fprintf(f, "}\n");



    fclose(f);
}



d:\v2game\autogen0\CULTURES_change_primary_events.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[60];
    FILE *f, *c;
    int s = 1474001;
    c = fopen("cultures.txt", "r");
    f = fopen("change_primary_cultures_events.txt", "w");
    //fscanf(f, "political_decisions = {\n\n");
    //fprintf(f, "political_decisions = {\n\n");

    while (fgets(culture, 60, c)) {

    for (int i = 0; i < 100; i++) {
        if (culture[i] == '\n' )
        {
            culture[i] = '\0';
            break;
        }
    }

    fprintf(f, "country_event = {\n");
    fprintf(f, "    id = %d\n", s);
    fprintf(f, "    title = \"EVTNAME1474001\"\n");
    fprintf(f, "    desc = \"EVTDESC1474001\"\n");
    fprintf(f, "    picture = \"Celebration\"\n");
    fprintf(f, "    trigger = {\n");
    fprintf(f, "        has_country_flag = changing_primary_culture\n");
    fprintf(f, "        capital_scope = { culture = "); fputs(culture, f); fprintf(f, " }\n");
    fprintf(f, "    }\n");
    fprintf(f, "    mean_time_to_happen =  { months = 3 }\n");
    fprintf(f, "    option = {\n");
	fprintf(f, "        name = \"EVTOPTA1474001\"\n");
	fprintf(f, "        primary_culture = "); fputs(culture, f); fprintf(f, "\n");
	fprintf(f, "        clr_country_flag = changing_primary_culture\n");
	fprintf(f, "    }\n");
	fprintf(f, "}\n");

    ++s;

    }
	//fprintf(f, "}\n");



    fclose(f);
}



d:\v2game\autogen0\CULTURES_lacaliz.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[130], rus_culture[130], rus1[70], rus2[70], rus3[250];
    FILE *f, *c, *a;
    a = fopen("rus_text.csv", "r");
    c = fopen("cult_loc_orig.csv", "r");
    f = fopen("acceptable_capital_cultures.csv", "w");

    fgets(rus1, 70, a);
    fgets(rus2, 70, a);
    fgets(rus3, 250, a);

    for (int i = 0; i < 130; i++) {
        if (rus2[i] == '\n')
        {
            rus2[i] = '\0';
            break;
        }
    }

    while (fgets(culture, 130, c)) {

    for (int i = 0; i < 130; i++) {
        if (culture[i] == ';')
        {
            culture[i] = '\0';
            for (int j = i + 1; j < 130; j++) {
                rus_culture[j-i-1] = culture[j];
                if (rus_culture[j-i-1] == ';') {
                    rus_culture[j-i-1] = '\0';
                    break;
                }
            }
            break;
        }
    }

    fprintf(f, "acceptance_capital_"); fputs(culture, f); fprintf(f, "_title;"); fputs(rus_culture, f); fputs(rus1, f);
    fprintf(f, "acceptance_capital_"); fputs(culture, f); fprintf(f, "_desc;"); fputs(rus2, f); fputs(rus_culture, f); fputs(rus3, f);

    }



    fclose(f);
}



d:\v2game\autogen0\CULTURES_lacaliz_change_primary.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[130], rus_culture[130], rus1[70], rus2[70], rus3[250];
    FILE *f, *c, *a;
    a = fopen("rus3_text.csv", "r");
    c = fopen("cult_loc.csv", "r");
    f = fopen("change_primary_cultures.csv", "w");

    fgets(rus1, 70, a);
    fgets(rus2, 70, a);
    fgets(rus3, 250, a);

    for (int i = 0; i < 130; i++) {
        if (rus2[i] == '\n')
        {
            rus2[i] = '\0';
            break;
        }
    }

    while (fgets(culture, 130, c)) {

    for (int i = 0; i < 130; i++) {
        if (culture[i] == ';')
        {
            culture[i] = '\0';
            for (int j = i + 1; j < 130; j++) {
                rus_culture[j-i-1] = culture[j];
                if (rus_culture[j-i-1] == ';') {
                    rus_culture[j-i-1] = '\0';
                    break;
                }
            }
            break;
        }
    }

    fprintf(f, "change_primary_"); fputs(culture, f); fprintf(f, "_title;"); fputs(rus1, f);
    fprintf(f, "change_primary_"); fputs(culture, f); fprintf(f, "_desc;"); fputs(rus2, f); fputs(rus_culture, f); fputs(rus3, f);

    }



    fclose(f);
}



d:\v2game\autogen0\CULTURES_lacaliz_deaccept.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[130], rus_culture[130], rus1[70], rus2[70], rus3[250];
    FILE *f, *c, *a;
    a = fopen("rus2_text.csv", "r");
    c = fopen("cult_loc.csv", "r");
    f = fopen("DEacceptable_cultures.csv", "w");

    fgets(rus1, 70, a);
    fgets(rus2, 70, a);
    fgets(rus3, 250, a);

    for (int i = 0; i < 130; i++) {
        if (rus2[i] == '\n')
        {
            rus2[i] = '\0';
            break;
        }
    }

    while (fgets(culture, 130, c)) {

    for (int i = 0; i < 130; i++) {
        if (culture[i] == ';')
        {
            culture[i] = '\0';
            for (int j = i + 1; j < 130; j++) {
                rus_culture[j-i-1] = culture[j];
                if (rus_culture[j-i-1] == ';') {
                    rus_culture[j-i-1] = '\0';
                    break;
                }
            }
            break;
        }
    }

    fprintf(f, "DEacceptance_"); fputs(culture, f); fprintf(f, "_title;"); fputs(rus_culture, f); fputs(rus1, f);
    fprintf(f, "DEacceptance_"); fputs(culture, f); fprintf(f, "_desc;"); fputs(rus2, f); fputs(rus_culture, f); fputs(rus3, f);

    }



    fclose(f);
}



d:\v2game\autogen0\CULTURES_lacaliz_for_SOV.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[130], rus_culture[130], rus1[70], rus2[70], rus3[250];
    FILE *f, *c, *a;
    a = fopen("rus_text_for_SOV.csv", "r");
    c = fopen("cult_loc_orig.csv", "r");
    f = fopen("acceptable_cultures.csv", "w");

    fgets(rus1, 70, a);
    fgets(rus2, 70, a);
    fgets(rus3, 250, a);

    for (int i = 0; i < 130; i++) {
        if (rus2[i] == '\n')
        {
            rus2[i] = '\0';
            break;
        }
    }

    while (fgets(culture, 130, c)) {

    for (int i = 0; i < 130; i++) {
        if (culture[i] == ';')
        {
            culture[i] = '\0';
            for (int j = i + 1; j < 130; j++) {
                rus_culture[j-i-1] = culture[j];
                if (rus_culture[j-i-1] == ';') {
                    rus_culture[j-i-1] = '\0';
                    break;
                }
            }
            break;
        }
    }

    fprintf(f, "acceptance_"); fputs(culture, f); fprintf(f, "_title;"); fputs(rus_culture, f); fputs(rus1, f);
    fprintf(f, "acceptance_"); fputs(culture, f); fprintf(f, "_desc;"); fputs(rus2, f); fputs(rus_culture, f); fputs(rus3, f);

    }



    fclose(f);
}



d:\v2game\autogen0\CULTURES_remove_cult_des.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[60];
    FILE *f, *c;
    int s = 1474001;
    c = fopen("cultures.txt", "r");
    f = fopen("DEacceptance_cultures.txt", "w");
    fscanf(f, "political_decisions = {\n\n");
    fprintf(f, "political_decisions = {\n\n");

    while (fgets(culture, 60, c)) {

    for (int i = 0; i < 100; i++) {
        if (culture[i] == '\n' )
        {
            culture[i] = '\0';
            break;
        }
    }

    fprintf(f, "    DEacceptance_"); fputs(culture, f); fprintf(f, " = {\n\n");
    fprintf(f, "        potential = {\n");
    fprintf(f, "            accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
    fprintf(f, "            NOT = {\n");
    fprintf(f, "                any_pop = { limit = { has_pop_culture = "); fputs(culture, f); fprintf(f, " }}\n");
    fprintf(f, "            }\n");
    fprintf(f, "            capital_scope = {\n");
    fprintf(f, "                NOT = {\n");
    fprintf(f, "                    OR = {\n");
    fprintf(f, "                        continent = north_america\n");
    fprintf(f, "                        continent = south_america\n");
    fprintf(f, "                        continent = oceania\n");
    fprintf(f, "                    }\n");
    fprintf(f, "                }\n");
    fprintf(f, "            }\n");
	fprintf(f, "        }\n\n");

	fprintf(f, "        allow = {\n");
    fprintf(f, "            NOT = {\n");
    fprintf(f, "                any_pop = { limit = { has_pop_culture = "); fputs(culture, f); fprintf(f, " }}\n");
    fprintf(f, "            }\n");
	fprintf(f, "        }\n");
	fprintf(f, "        effect = {\n");
	fprintf(f, "            prestige = 1\n");
	fprintf(f, "            remove_accepted_culture = "); fputs(culture, f); fprintf(f, "\n");
	++s;
	fprintf(f, "        }\n");
	fprintf(f, "        ai_will_do = {\n");
	fprintf(f, "            factor = 1\n");
	fprintf(f, "        }\n");
	fprintf(f, "    }\n");


    }
	fprintf(f, "}\n");



    fclose(f);
}



d:\v2game\autogen0\CULTUREScapital_lacaliz.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[130], rus_culture[130], rus1[70], rus2[70], rus3[250];
    FILE *f, *c, *a;
    a = fopen("rus_text.csv", "r");
    c = fopen("cult_loc.csv", "r");
    f = fopen("acceptable_capital_cultures.csv", "w");

    fgets(rus1, 70, a);
    fgets(rus2, 70, a);
    fgets(rus3, 250, a);

    for (int i = 0; i < 130; i++) {
        if (rus2[i] == '\n')
        {
            rus2[i] = '\0';
            break;
        }
    }

    while (fgets(culture, 130, c)) {

    for (int i = 0; i < 130; i++) {
        if (culture[i] == ';')
        {
            culture[i] = '\0';
            for (int j = i + 1; j < 130; j++) {
                rus_culture[j-i-1] = culture[j];
                if (rus_culture[j-i-1] == ';') {
                    rus_culture[j-i-1] = '\0';
                    break;
                }
            }
            break;
        }
    }

    fprintf(f, "acceptance_capital_"); fputs(culture, f); fprintf(f, "_title;"); fputs(rus_culture, f); fputs(rus1, f);
    fprintf(f, "acceptance_capital_"); fputs(culture, f); fprintf(f, "_desc;"); fputs(rus2, f); fputs(rus_culture, f); fputs(rus3, f);

    }



    fclose(f);
}



d:\v2game\autogen0\D_primary_culture.cpp
/************************************************************************/
#include <fstream>
#include <string>
#include <vector>

#include <windows.h>

void FindFiles(std::vector<std::string> &mem, const char *path) {
    WIN32_FIND_DATA FindFileData;
    HANDLE hf = FindFirstFile(path, &FindFileData);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            mem.emplace_back(FindFileData.cFileName);
        } while (FindNextFile(hf, &FindFileData));
        FindClose(hf);
    }
}

int main() {
    std::ifstream input("cultures_orig.txt");
    std::ofstream out("D_primary_cultures.txt");
    std::vector<std::string> files, d_tags;
    FindFiles(files, "C:\\V2HoD3.03_Rus\\history\\countries\\*");
    for (int i = 2; i < files.size(); ++i) {
        std::string tag = files[i].substr(0, 3);
        if (tag[0] == 'D' && '0' <= tag[1] && tag[1] < '6') d_tags.push_back(tag);
    }
    std::string culture;
    out << "political_decisions = {\n";
    while (std::getline(input, culture)) {
        out << "	D_primary_" << culture << " = {\n";
        out << "		potential = {\n";
        out << "			primary_culture = british\n";
        out << "			capital_scope = { culture = " << culture << " }\n";
        out << "			OR = {\n";
        for (auto & tag : d_tags) {
            out << "				tag = " << tag << "\n";
        }
        out << "			}\n";
        out << "		}\n";
        out << "		allow = {\n";
        out << "			capital_scope = { culture = " << culture << " }\n";
        out << "		}\n";
        out << "		effect = {\n";
        out << "			primary_culture = " << culture << "\n";
        out << "			any_pop = {\n";
        out << "				limit = {\n";
        out << "					OR = {\n";
        out << "						is_accepted_culture = yes\n";
        out << "						is_primary_culture = yes\n";
        out << "					}\n";
        out << "				}\n";
        out << "				militancy = -10\n";
        out << "			}\n";
        out << "		}\n";
        out << "		ai_will_do = {\n";
        out << "			factor = 1\n";
        out << "		}\n";
        out << "	}\n";
    }
    out << "}\n";
}



d:\v2game\autogen0\D_primary_culture_localisation.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    char culture[130], rus_culture[130], rus1[70], rus2[70], rus3[250];
    FILE *f, *c, *a;
    a = fopen("rus_text.csv", "r");
    c = fopen("cult_loc.csv", "r");
    f = fopen("D_primary_culture.csv", "w");

    fgets(rus1, 70, a);
    fgets(rus2, 70, a);
    fgets(rus3, 250, a);

    for (int i = 0; i < 130; i++) {
        if (rus2[i] == '\n')
        {
            rus2[i] = '\0';
            break;
        }
    }

    while (fgets(culture, 130, c)) {

    for (int i = 0; i < 130; i++) {
        if (culture[i] == ';')
        {
            culture[i] = '\0';
            for (int j = i + 1; j < 130; j++) {
                rus_culture[j-i-1] = culture[j];
                if (rus_culture[j-i-1] == ';') {
                    rus_culture[j-i-1] = '\0';
                    break;
                }
            }
            break;
        }
    }

    fprintf(f, "D_primary_"); fputs(culture, f); fprintf(f, "_title;"); fputs(rus_culture, f); fputs(rus1, f);
    fprintf(f, "D_primary_"); fputs(culture, f); fprintf(f, "_desc;"); fputs(rus2, f); fputs(rus_culture, f); fputs(rus3, f);

    }



    fclose(f);
}



d:\v2game\autogen0\goods_changes.cpp
/************************************************************************/
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



d:\v2game\autogen0\Information.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream c("hist_countries.txt");
    ofstream f("countries_information.txt");
    const string prim_cult = "primary_culture", way = "c:";
    string s, s_old, tag, what_prim_cult;
    vector<string> init;
    unordered_map<string, vector<string>> dict;
    unordered_map<string, int> dict_sizes;
    unordered_set<string> bad_tags = {"SOV"};
    while (getline(c, s)) {
        if (s.substr(0, 2) == way) {
            //cout << "TRUE";
            tag = s.substr(35, 3);
        }
        if (s.substr(0, 15) == prim_cult) {
            what_prim_cult = s.substr(18);
            //++dict_sizes[what_prim_cult];
            //if (dict_sizes[what_prim_cult] == 1) dict[what_prim_cult] = init;
            if (tag[0] == 'D' && '0' <= tag[1] && tag[1] <= '5') continue;
            if (bad_tags.find(tag) != bad_tags.end()) continue;
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



d:\v2game\autogen0\Information_1700.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream c("hist_countries_1700.txt");
    ofstream f("countries_information_1700.txt");
    const string prim_cult = "primary_culture", way = "c:";
    string s, s_old, tag, what_prim_cult;
    vector<string> init;
    unordered_map<string, vector<string>> dict;
    unordered_map<string, int> dict_sizes;
    while (getline(c, s)) {
        if (s.substr(0, 2) == way) {
            //cout << "TRUE";
            tag = s.substr(48, 3);
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



d:\v2game\autogen0\Information_chr.cpp
/************************************************************************/
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



d:\v2game\autogen0\Removing__cores.cpp
/************************************************************************/
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



d:\v2game\autogen0\Removing_cores_after_union.cpp
/************************************************************************/
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



d:\v2game\autogen0\Removing_cores_with_protection.cpp
/************************************************************************/
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



d:\v2game\autogen0\Setting_cores.cpp
/************************************************************************/
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



d:\v2game\autogen0\Untitled1.cpp
/************************************************************************/
#include <bits/stdc++.h>
using namespace std;
int main() {
    const string prim_cult = "primary_culture", way = "c:";
    //string s = "c:\v2hod3.03_rus\history\countries\ALB - Albania.txt";
    //if (s.substr(0, 5) == way) cout << 1 << '\n';
    cout << way;
}


