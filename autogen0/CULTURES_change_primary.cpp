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
