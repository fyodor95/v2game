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
