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
