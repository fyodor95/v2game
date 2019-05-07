#include "cultures.h"
#include "events.h"
#include "plus_minus_cultures_events.h"

class tPlusMinusCultures
{
public:
    tPlusMinusCultures(std::ostream & eventOutput, std::ostream & localizationOutput);

private:
    std::ostream & xEventOutput;
    std::ostream & xLocalizationOutput;

    tCulture const * xCurrentCulture;

    uint xID = 500u;

    char const * xCulture() const;
    std::string xRemoveCultureEventTitle() const;
    std::string xRemoveCultureEventDesc() const;
    tConditions<tTarget::kCountry> xRemoveCultureConditions() const;

    void xProcessCulture(tCulture const & culture);
    void xRemoveCultureEvent();
};

tPlusMinusCultures::tPlusMinusCultures(std::ostream & eventOutput, std::ostream & localizationOutput)
    : xEventOutput(eventOutput)
    , xLocalizationOutput(localizationOutput)
{
    for (tCulture const & culture : Cultures())
        xProcessCulture(culture);
}

char const * tPlusMinusCultures::xCulture() const
{
    return xCurrentCulture->Name;
}

void tPlusMinusCultures::xProcessCulture(tCulture const & culture)
{
    xCurrentCulture = &culture;
    xRemoveCultureEvent();
}

tConditions<tTarget::kCountry> tPlusMinusCultures::xRemoveCultureConditions() const
{
    return
    {
        AcceptedCulture(xCulture()),
        Not(AnyOwnedProvince(MajorityCultureIs(xCulture())))
    };
}

std::string tPlusMinusCultures::xRemoveCultureEventTitle() const
{
    std::string res = "Кто такие ";
    res += xCurrentCulture->Localization;
    res += '?';
    return res;
}

std::string tPlusMinusCultures::xRemoveCultureEventDesc() const
{
    std::string res = xCurrentCulture->Localization;
    res += " уже почти не живут в нашей стране. Они перестали считаться признанной культурой.";
    return res;
}

void tPlusMinusCultures::xRemoveCultureEvent()
{
    std::string const title = xRemoveCultureEventTitle();
    std::string const desc = xRemoveCultureEventDesc();
    Event(xID, title.data(), desc.data(),
          xEventOutput, xLocalizationOutput,
          Trigger          <tTarget::kCountry>(xRemoveCultureConditions()),
          MeanTimeToHappen <tTarget::kCountry>(60.f, {}),
          {Option          <tTarget::kCountry>({RemoveAcceptedCulture(xCulture())}, xID)},
          {"Ок"}, "Administration");
    ++xID;
}

void AddingAndRemovingCultures()
{
    std::ofstream        eventOutput      ("events\\plus_minus_culture_events.txt");
    std::ofstream localizationOutput("localisation\\plus_minus_culture_events.csv");
    tPlusMinusCultures(static_cast<std::ostream &>(       eventOutput),
                       static_cast<std::ostream &>(localizationOutput));
}
