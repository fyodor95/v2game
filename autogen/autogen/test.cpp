#include "decisions.h"
#include "test.h"

void Test()
{
    tDecisionOutput   decisionOutput   ("decisions\\test.txt");
    std::ofstream localizationOutput("localisation\\test.csv");
    Decision("test", "Тест", "Это тест.", decisionOutput, localizationOutput, {},
             {},
             {});
}
