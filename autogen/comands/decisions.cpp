#include "decisions.h"
#include "modifier.h"

tDecisionOutput::tDecisionOutput(char const * const file)
    : std::ofstream(file)
{
    *this << "political_decisions = {\n";
}

tDecisionOutput::~tDecisionOutput()
{
    *this << "}\n";
}

void Decision(std::string && name, char const * const title, char const * const desc,
              tDecisionOutput & decisionOutput, std::ostream & localizationOutput,
              tConditions<tTarget::kCountry> && potential,
              tConditions<tTarget::kCountry> && allow,
              tEffects   <tTarget::kCountry> && effects,
              tConditions<tTarget::kCountry> * const aiWillDoItIf)
{
    assertion(title && desc);
    tComands body
    {
        {"potential", std::move(CastToComands(potential))},
        {"allow"    , std::move(CastToComands(allow    ))},
        {"effect"   , std::move(CastToComands(effects  ))}
    };
    if (aiWillDoItIf)
    {
        body.emplace_back
        (
            "ai_will_do",
            tComands{tComand("factor", 1.f), tModifier(0.f, Not(And<tTarget::kCountry>(std::move(*aiWillDoItIf))))}
        );
    }
    localizationOutput << name << "_title;" << title << LocLineEnd();
    localizationOutput << name << "_desc;"  << desc  << LocLineEnd();
    decisionOutput << tComand(std::move(name), std::move(body));
}
