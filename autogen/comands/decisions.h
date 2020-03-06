#pragma once

#include "conditions.h"
#include "effects.h"

class tDecisionOutput : public std::ofstream
{
public:
    tDecisionOutput(char const * file = nullptr);

    ~tDecisionOutput();
};

void Decision(std::string && name, char const * title, char const * desc,
              tDecisionOutput & decisionOutput, std::ostream & localizationOutput,
              tConditions<tTarget::kCountry> && potential,
              tConditions<tTarget::kCountry> && allow,
              tEffects   <tTarget::kCountry> &&,
              tConditions<tTarget::kCountry> * aiWillDoItIf = nullptr);
