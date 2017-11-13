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
