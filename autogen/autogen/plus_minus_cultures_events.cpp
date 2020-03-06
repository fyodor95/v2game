#include "cultures.h"
#include "events.h"
#include "plus_minus_cultures_events.h"

class tPlusMinusCulture
{
public:
    tPlusMinusCulture(tCulture culture, uint & id, std::ostream & eventOutput, std::ostream & localizationOutput);

private:
    std::ostream & xEventOutput;
    std::ostream & xLocalizationOutput;

    tCulture const * xCurrentCulture;

    uint & xID;
    tCulture culture;

    char const * xCulture() const;
    char const * xLocalization() const;

    std::string xRemoveCultureEventTitle() const;
    std::string xRemoveCultureEventDesc() const;
    tConditions<tTarget::kCountry> xRemoveCultureConditions() const;
    tModifiers<tTarget::kCountry> xRemoveCultureModifiers() const;
    void xRemoveCultureEvent();

    std::string xHighMilitancyRemovingEventTitle() const;
    std::string xHighMilitancyRemovingEventDesc() const;
    tConditions<tTarget::kProvince> xHighMilitancyRemovingConditions() const;
    tModifiers<tTarget::kProvince> xHighMilitancyRemovingModifiers() const;
    void xHighMilitancyRemoving();

    std::string xLowMilitancyAddingEventTitle() const;
    std::string xLowMilitancyAddingEventDesc() const;
    tConditions<tTarget::kProvince> xLowMilitancyAddingConditions() const;
    tModifiers<tTarget::kProvince> xLowMilitancyAddingModifiers() const;
    void xLowMilitancyAdding();
};

tPlusMinusCulture::tPlusMinusCulture(tCulture culture, uint & id, std::ostream & eventOutput, std::ostream & localizationOutput)
    : xEventOutput(eventOutput)
    , xLocalizationOutput(localizationOutput)
    , xID(id)
    , culture(culture)
{
    xRemoveCultureEvent();
    xHighMilitancyRemoving();
    xLowMilitancyAdding();
}

char const * tPlusMinusCulture::xCulture() const
{
    return ToString(culture);
}

char const * tPlusMinusCulture::xLocalization() const
{
    return Localization(culture);
}

tConditions<tTarget::kCountry> tPlusMinusCulture::xRemoveCultureConditions() const
{
    return
    {
        IsAccepted(culture),
        Not(AnyOwnedProvince(MajorityCultureIs(culture)))
    };
}

tModifiers<tTarget::kCountry> tPlusMinusCulture::xRemoveCultureModifiers() const
{
    return
    {
        tModifier(0.10f, CitizenshipPolicyIs(tCitizenshipPolicy::kLimited  )),
        tModifier(0.01f, CitizenshipPolicyIs(tCitizenshipPolicy::kResidency))
    };
}

std::string tPlusMinusCulture::xRemoveCultureEventTitle() const
{
    std::string res = "Кто такие ";
    res += xLocalization();
    res += '?';
    return res;
}

std::string tPlusMinusCulture::xRemoveCultureEventDesc() const
{
    std::string res = xLocalization();
    res += " уже почти не живут в нашей стране. Они перестали считаться признанной культурой.";
    return res;
}

void tPlusMinusCulture::xRemoveCultureEvent()
{
    std::string const title = xRemoveCultureEventTitle();
    std::string const desc = xRemoveCultureEventDesc();
    CountryEvent
        (xID, title.data(), desc.data(), xEventOutput, xLocalizationOutput,
         Trigger          <tTarget::kCountry>(xRemoveCultureConditions()),
         MeanTimeToHappen <tTarget::kCountry>(60.f, xRemoveCultureModifiers()),
         {Option          <tTarget::kCountry>({RemoveAcceptedCulture(culture)}, xID)},
         {"Ок"}, "Administration");
    ++xID;
}

tConditions<tTarget::kProvince> tPlusMinusCulture::xHighMilitancyRemovingConditions() const
{
    return
    {
        MajorityCultureIsPrimary(_No),
        MajorityCultureIsAccepted(),
        MajorityCultureIs(culture),
        PopulationIsGreaterOrEqual(5000u),
        AverageMilitancy<tTarget::kProvince>(4.f),
        ConditionsForOwner
        ({
            Or<tTarget::kCountry>
            ({
                ConditionsForCapital({Not(MajorityCultureIs(culture))}),
                CitizenshipPolicyIs(tCitizenshipPolicy::kResidency)
            })
        })
    };
}

std::string tPlusMinusCulture::xHighMilitancyRemovingEventTitle() const
{
    std::string res = "Недовольные ";
    res += xLocalization();
    return res;
}

std::string tPlusMinusCulture::xHighMilitancyRemovingEventDesc() const
{
    std::string res = xLocalization();
    res += " в провинции $PROVINCENAME$ в результате ужасных условий жизни уже не ощущают себя признанной культурой.";
    return res;
}

tModifiers<tTarget::kProvince> tPlusMinusCulture::xHighMilitancyRemovingModifiers() const
{
    return
    {
        tModifier(  0.1f, AverageMilitancy<tTarget::kProvince>(6.f)),
        tModifier(  0.1f, AverageMilitancy<tTarget::kProvince>(8.f)),
        tModifier( 10.0f, ConditionsForOwner({CitizenshipPolicyIs(tCitizenshipPolicy::kLimited)})),
        tModifier(100.0f, ConditionsForOwner({CitizenshipPolicyIs(tCitizenshipPolicy::kFull   )}))
    };
}

void tPlusMinusCulture::xHighMilitancyRemoving()
{
    std::string const title = xHighMilitancyRemovingEventTitle();
    std::string const desc = xHighMilitancyRemovingEventDesc();
    Event(xID, title.data(), desc.data(),
          xEventOutput, xLocalizationOutput,
          Trigger          <tTarget::kProvince>(xHighMilitancyRemovingConditions()),
          MeanTimeToHappen <tTarget::kProvince>(12.f, xHighMilitancyRemovingModifiers()),
          {Option          <tTarget::kProvince>({EffectsForOwner({RemoveAcceptedCulture(culture)})}, xID)},
          {"Ок"});
    ++xID;
}

tConditions<tTarget::kProvince> tPlusMinusCulture::xLowMilitancyAddingConditions() const
{
    return
    {
        MajorityCultureIsAccepted(_No),
        MajorityCultureIs(culture),
        PopulationIsGreaterOrEqual(40000u),
        Not(AverageMilitancy<tTarget::kProvince>(0.4f))
    };
}

std::string tPlusMinusCulture::xLowMilitancyAddingEventTitle() const
{
    std::string res = "Довольные ";
    res += xLocalization();
    return res;
}

std::string tPlusMinusCulture::xLowMilitancyAddingEventDesc() const
{
    std::string res = xLocalization();
    res += " в провинции $PROVINCENAME$ в результате превосходных условий жизни стали ощущать себя признанной культурой.";
    return res;
}

tModifiers<tTarget::kProvince> tPlusMinusCulture::xLowMilitancyAddingModifiers() const
{
    return
    {
        tModifier( 0.5f, Not(AverageMilitancy<tTarget::kProvince>(0.2f))),
        tModifier( 0.5f, Not(AverageMilitancy<tTarget::kProvince>(0.1f))),
        tModifier( 5.0f, ConditionsForOwner({CitizenshipPolicyIs(tCitizenshipPolicy::kLimited  )})),
        tModifier(25.0f, ConditionsForOwner({CitizenshipPolicyIs(tCitizenshipPolicy::kResidency)}))
    };
}

void tPlusMinusCulture::xLowMilitancyAdding()
{
    std::string const title = xLowMilitancyAddingEventTitle();
    std::string const desc = xLowMilitancyAddingEventDesc();
    Event(xID, title.data(), desc.data(),
          xEventOutput, xLocalizationOutput,
          Trigger          <tTarget::kProvince>(xLowMilitancyAddingConditions()),
          MeanTimeToHappen <tTarget::kProvince>(240.f, xLowMilitancyAddingModifiers()),
          {Option          <tTarget::kProvince>({EffectsForOwner({AddAcceptedCulture(culture)})}, xID)},
          {"Ок"});
    ++xID;
}

void AddingAndRemovingCultures()
{
    std::ofstream        eventOutput      ("events\\plus_minus_culture_events.txt");
    std::ofstream localizationOutput("localisation\\plus_minus_culture_events.csv");
    uint id = 5050u;
    for (tCulture const culture : kCultures)
        tPlusMinusCulture(culture, id, eventOutput, localizationOutput);
}
