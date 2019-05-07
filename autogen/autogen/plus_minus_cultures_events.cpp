#include "cultures.h"
#include "events.h"
#include "plus_minus_cultures_events.h"

class tPlusMinusCultures
{
public:
    tPlusMinusCultures(std::ostream &);

private:
    std::ostream & xOut;

    char const * xCulture;

    uint xID = 500u;

    tConditions<tTarget::kCountry> xRemoveCultureConditions() const;
    void xProcessCulture(char const * culture);
    void xRemoveCultureEvent();
};

tPlusMinusCultures::tPlusMinusCultures(std::ostream & out)
    : xOut(out)
{
    for (char const * const culture : Cultures())
        xProcessCulture(culture);
}

void tPlusMinusCultures::xProcessCulture(char const * const culture)
{
    xCulture = culture;
    xRemoveCultureEvent();
}

tConditions<tTarget::kCountry> tPlusMinusCultures::xRemoveCultureConditions() const
{
    return
    {
        AcceptedCulture(xCulture),
        Not(AnyOwnedProvince(MajorityCultureIs(xCulture)))
    };
}

void tPlusMinusCultures::xRemoveCultureEvent()
{
    xOut << CountryEvent(xID, "Administration",
                         Trigger          <tTarget::kCountry>(xRemoveCultureConditions()),
                         MeanTimeToHappen <tTarget::kCountry>(60.f, {}),
                         {Option          <tTarget::kCountry>({RemoveAcceptedCulture(xCulture)}, xID)});
    ++xID;
}

void AddingAndRemovingCultures()
{
    std::ofstream out("events\\plus_minus_culture_events.txt");
    tPlusMinusCultures(static_cast<std::ostream &>(out));
}
