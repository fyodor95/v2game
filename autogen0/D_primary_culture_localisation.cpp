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
