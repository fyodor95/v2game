#include <bits/stdc++.h>
using namespace std;
int main() {
    char tag[4], musor[100];
    int s = 1472073;
    FILE *f, *c;
    c = fopen("countries.txt", "r");
    f = fopen("..\\events\\change_tag.txt", "w");

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
    fprintf(f, "		total_pops = 100000\n");
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

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 0.1\n");
    fprintf(f, "			government = proletarian_dictatorship\n");
    fprintf(f, "		}\n");

    fprintf(f, "		modifier = {\n");
    fprintf(f, "			factor = 10\n");
    fprintf(f, "			government = fascist_dictatorship\n");
    fprintf(f, "		}\n");
    fprintf(f, "	}\n");
    fprintf(f, "	option = {\n");
    fprintf(f, "		name = \"0\"\n");
    fprintf(f, "		set_country_flag = not_ai\n");
    fprintf(f, "		random_country = {\n");
    fprintf(f, "			limit = {\n");
    fprintf(f, "				AI = no\n");
    fprintf(f, "				war = no\n");
    fprintf(f, "			}\n");
    fprintf(f, "			country_event = { id=%d days=0 }\n", s + 1);
    fprintf(f, "		}\n");
    fprintf(f, "	}\n");
    fprintf(f, "}\n\n");



    fprintf(f, "country_event = {\n");
    fprintf(f, "	id = %d\n", s);
    fprintf(f, "	title = \"0\"\n");
    fprintf(f, "	desc = \"0\"\n");
    fprintf(f, "	picture = \"greatpowers\"\n");
    fprintf(f, "	trigger = {\n");
    fprintf(f, "		has_country_flag = not_ai\n");
    fprintf(f, "		tag = %s\n", tag);
    fprintf(f, "		has_global_flag = kozelam_net\n");
    fprintf(f, "		AI = yes\n");
    fprintf(f, "	}\n");
    fprintf(f, "	mean_time_to_happen = {\n");
    fprintf(f, "		months = 1\n");
    fprintf(f, "	}\n");
    fprintf(f, "	option = {\n");
    fprintf(f, "		name = \"0\"\n");
    fprintf(f, "		random_country = {\n");
    fprintf(f, "			limit = {\n");
    fprintf(f, "				AI = no\n");
    fprintf(f, "				NOT = { has_country_flag = not_ai }\n");
    fprintf(f, "			}\n");
    fprintf(f, "			country_event = { id=%d days=0 }\n", ++s);
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
    fprintf(f, "		clr_country_flag = not_ai\n");
    fprintf(f, "		change_tag_no_core_switch = %s\n", tag);
    fprintf(f, "	}\n");
    fprintf(f, "}\n\n");

    fgets(musor, 100, c);

    }



    fclose(f);
}
