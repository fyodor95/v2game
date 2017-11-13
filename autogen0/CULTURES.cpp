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
