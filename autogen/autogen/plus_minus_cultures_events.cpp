#include "cultures.h"
#include "events.h"
#include "plus_minus_cultures_events.h"

template <tCulture culture>
class tPlusMinusCulture
{
public:
    tPlusMinusCulture(uint & id, std::ostream & eventOutput, std::ostream & localizationOutput);

private:
    std::ostream & xEventOutput;
    std::ostream & xLocalizationOutput;

    tCulture const * xCurrentCulture;

    uint & xID;

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

template <tCulture culture>
tPlusMinusCulture<culture>::tPlusMinusCulture(uint & id, std::ostream & eventOutput, std::ostream & localizationOutput)
    : xEventOutput(eventOutput)
    , xLocalizationOutput(localizationOutput)
    , xID(id)
{
    xRemoveCultureEvent();
    xHighMilitancyRemoving();
    xLowMilitancyAdding();
}

template <tCulture culture>
char const * tPlusMinusCulture<culture>::xCulture() const
{
    return nsCulture::ToString<culture>();
}

template <tCulture culture>
char const * tPlusMinusCulture<culture>::xLocalization() const
{
    return nsCulture::Localization<culture>();
}

template <tCulture culture>
tConditions<tTarget::kCountry> tPlusMinusCulture<culture>::xRemoveCultureConditions() const
{
    return
    {
        IsAccepted<culture>(),
        Not(AnyOwnedProvince(MajorityCultureIs<culture>()))
    };
}

template <tCulture culture>
tModifiers<tTarget::kCountry> tPlusMinusCulture<culture>::xRemoveCultureModifiers() const
{
    return
    {
        tModifier(0.10f, CitizenshipPolicyIs<tCitizenshipPolicy::kLimited  >()),
        tModifier(0.01f, CitizenshipPolicyIs<tCitizenshipPolicy::kResidency>())
    };
}

template <tCulture culture>
std::string tPlusMinusCulture<culture>::xRemoveCultureEventTitle() const
{
    std::string res = "Кто такие ";
    res += xLocalization();
    res += '?';
    return res;
}

template <tCulture culture>
std::string tPlusMinusCulture<culture>::xRemoveCultureEventDesc() const
{
    std::string res = xLocalization();
    res += " уже почти не живут в нашей стране. Они перестали считаться признанной культурой.";
    return res;
}

template <tCulture culture>
void tPlusMinusCulture<culture>::xRemoveCultureEvent()
{
    std::string const title = xRemoveCultureEventTitle();
    std::string const desc = xRemoveCultureEventDesc();
    CountryEvent<tEventPicture::Administration>
        (xID, title.data(), desc.data(), xEventOutput, xLocalizationOutput,
         Trigger          <tTarget::kCountry>(xRemoveCultureConditions()),
         MeanTimeToHappen <tTarget::kCountry>(60.f, xRemoveCultureModifiers()),
         {Option          <tTarget::kCountry>({RemoveAcceptedCulture<culture>()}, xID)},
         {"Ок"});
    ++xID;
}

template <tCulture culture>
tConditions<tTarget::kProvince> tPlusMinusCulture<culture>::xHighMilitancyRemovingConditions() const
{
    return
    {
        MajorityCultureIsPrimary(_No),
        MajorityCultureIsAccepted(),
        MajorityCultureIs<culture>(),
        PopulationIsGreaterOrEqual(5000u),
        AverageMilitancy<tTarget::kProvince>(4.f),
        ConditionsForOwner
        ({
            Or<tTarget::kCountry>
            ({
                ConditionsForCapital({Not(MajorityCultureIs<culture>())}),
                CitizenshipPolicyIs<tCitizenshipPolicy::kResidency>()
            })
        })
    };
}

template <tCulture culture>
std::string tPlusMinusCulture<culture>::xHighMilitancyRemovingEventTitle() const
{
    std::string res = "Недовольные ";
    res += xLocalization();
    return res;
}

template <tCulture culture>
std::string tPlusMinusCulture<culture>::xHighMilitancyRemovingEventDesc() const
{
    std::string res = xLocalization();
    res += " в провинции $PROVINCENAME$ в результате ужасных условий жизни уже не ощущают себя признанной культурой.";
    return res;
}

template <tCulture culture>
tModifiers<tTarget::kProvince> tPlusMinusCulture<culture>::xHighMilitancyRemovingModifiers() const
{
    return
    {
        tModifier(  0.1f, AverageMilitancy<tTarget::kProvince>(6.f)),
        tModifier(  0.1f, AverageMilitancy<tTarget::kProvince>(8.f)),
        tModifier( 10.0f, ConditionsForOwner({CitizenshipPolicyIs<tCitizenshipPolicy::kLimited>()})),
        tModifier(100.0f, ConditionsForOwner({CitizenshipPolicyIs<tCitizenshipPolicy::kFull   >()}))
    };
}

template <tCulture culture>
void tPlusMinusCulture<culture>::xHighMilitancyRemoving()
{
    std::string const title = xHighMilitancyRemovingEventTitle();
    std::string const desc = xHighMilitancyRemovingEventDesc();
    Event(xID, title.data(), desc.data(),
          xEventOutput, xLocalizationOutput,
          Trigger          <tTarget::kProvince>(xHighMilitancyRemovingConditions()),
          MeanTimeToHappen <tTarget::kProvince>(12.f, xHighMilitancyRemovingModifiers()),
          {Option          <tTarget::kProvince>({EffectsForOwner({RemoveAcceptedCulture<culture>()})}, xID)},
          {"Ок"});
    ++xID;
}

template <tCulture culture>
tConditions<tTarget::kProvince> tPlusMinusCulture<culture>::xLowMilitancyAddingConditions() const
{
    return
    {
        MajorityCultureIsAccepted(_No),
        MajorityCultureIs<culture>(),
        PopulationIsGreaterOrEqual(40000u),
        Not(AverageMilitancy<tTarget::kProvince>(0.4f))
    };
}

template <tCulture culture>
std::string tPlusMinusCulture<culture>::xLowMilitancyAddingEventTitle() const
{
    std::string res = "Довольные ";
    res += xLocalization();
    return res;
}

template <tCulture culture>
std::string tPlusMinusCulture<culture>::xLowMilitancyAddingEventDesc() const
{
    std::string res = xLocalization();
    res += " в провинции $PROVINCENAME$ в результате превосходных условий жизни стали ощущать себя признанной культурой.";
    return res;
}

template <tCulture culture>
tModifiers<tTarget::kProvince> tPlusMinusCulture<culture>::xLowMilitancyAddingModifiers() const
{
    return
    {
        tModifier( 0.5f, Not(AverageMilitancy<tTarget::kProvince>(0.2f))),
        tModifier( 0.5f, Not(AverageMilitancy<tTarget::kProvince>(0.1f))),
        tModifier( 5.0f, ConditionsForOwner({CitizenshipPolicyIs<tCitizenshipPolicy::kLimited  >()})),
        tModifier(25.0f, ConditionsForOwner({CitizenshipPolicyIs<tCitizenshipPolicy::kResidency>()}))
    };
}

template <tCulture culture>
void tPlusMinusCulture<culture>::xLowMilitancyAdding()
{
    std::string const title = xLowMilitancyAddingEventTitle();
    std::string const desc = xLowMilitancyAddingEventDesc();
    Event(xID, title.data(), desc.data(),
          xEventOutput, xLocalizationOutput,
          Trigger          <tTarget::kProvince>(xLowMilitancyAddingConditions()),
          MeanTimeToHappen <tTarget::kProvince>(240.f, xLowMilitancyAddingModifiers()),
          {Option          <tTarget::kProvince>({EffectsForOwner({AddAcceptedCulture<culture>()})}, xID)},
          {"Ок"});
    ++xID;
}

template <size_t i = 0u>
static void ProcessCulture(uint & id, std::ofstream & eventOutput, std::ofstream & localizationOutput)
{
    tPlusMinusCulture<kCultures[i]>(id, eventOutput, localizationOutput);
    ProcessCulture<i + 1u>         (id, eventOutput, localizationOutput);
}

template <>
void ProcessCulture<kCultures.size()>(uint &, std::ofstream &, std::ofstream &)
{
}

void AddingAndRemovingCultures()
{
    std::ofstream        eventOutput      ("events\\plus_minus_culture_events.txt");
    std::ofstream localizationOutput("localisation\\plus_minus_culture_events.csv");
    uint id = 5050u;
    ProcessCulture(id, eventOutput, localizationOutput);
}
