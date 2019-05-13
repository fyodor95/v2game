#include "decisions.h"

tDesicionOutput::tDesicionOutput(char const * const file)
    : std::ofstream(file)
{
    *this << "political_decisions = {\n";
}

tDesicionOutput::~tDesicionOutput()
{
    *this << "}\n";
}

void Desicion(std::string && name, char const * const title, char const * const desc,
              tDesicionOutput & desicionOutput, std::ostream & localizationOutput,
              tConditions<tTarget::kCountry> && potential,
              tConditions<tTarget::kCountry> && allow,
              tEffects   <tTarget::kCountry> && effects)
{
    assertion(title && desc);
    tComands body
    {
        {"potential", std::move(CastToComands(potential))},
        {"allow"    , std::move(CastToComands(allow    ))},
        {"effect"   , std::move(CastToComands(effects  ))}
    };
    localizationOutput << name << "_title;" << title << LocLineEnd();
    localizationOutput << name << "_desc;"  << desc  << LocLineEnd();
    desicionOutput << tComand(std::move(name), std::move(body));
}
