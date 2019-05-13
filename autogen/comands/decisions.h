#pragma once

#include "conditions.h"
#include "effects.h"

class tDesicionOutput : public std::ofstream
{
public:
    tDesicionOutput(char const * file);

    ~tDesicionOutput();
};

void Desicion(std::string && name, char const * title, char const * desc,
              tDesicionOutput & desicionOutput, std::ostream & localizationOutput,
              tConditions<tTarget::kCountry> && potential,
              tConditions<tTarget::kCountry> && allow,
              tEffects   <tTarget::kCountry> &&);
