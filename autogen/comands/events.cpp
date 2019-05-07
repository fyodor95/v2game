#include "events.h"

enum class tEventPropertyType : uint8_t
{
    kOptionDescription = 0u,
    kEventName = 1u,
    kEventDesc = 2u,
};

struct tEventProperty
{
    tEventProperty(char const * property, char const * localisationReference);

    DELETE_COPY_AND_MOVE_CONSTRUCTORS(tEventProperty);

    char const * const Property;
    char const * const LocalizationReference;
};

tEventProperty::tEventProperty(char const * const property, char const * const localisationReference)
    : Property(property)
    , LocalizationReference(localisationReference)
{
}

using tEventProperties = std::array<tEventProperty, 3u>;

static SINGLETON(tEventProperties, EventProperties)
{
    return
    {
        tEventProperty("name"  , "OPT"  ),
        tEventProperty("title" , "NAME" ),
        tEventProperty("desc"  , "DESC" )
    };
}

static std::string EventLocalizationReference
(tEventPropertyType const type, uint const eventID, uint const optionNum = 0u)
{
    size_t const eventPropertyIndex = static_cast<uint8_t>(type);
    assertion(eventPropertyIndex || optionNum);
    std::string res = "EVT";
    res += EventProperties()[eventPropertyIndex].LocalizationReference;
    if (optionNum)
        res += static_cast<char>('A' + optionNum - 1u);
    res += std::to_string(eventID);
    return res;
}

static tString EventProperty(tEventPropertyType const type, uint const eventID, uint const optionNum = 0u)
{
    std::string name;
    name += EventProperties()[static_cast<uint8_t>(type)].Property;
    name += " = \"";
    name += EventLocalizationReference(type, eventID, optionNum);
    name += '\"';
    return std::move(name);
}

template <tTarget target>
tOption<target> Option(tEffects<target> && effects, uint const eventID, uint const optionNum)
{
    static_assert(target != tTarget::kPop);
    assertion(optionNum && optionNum <= 26u);
    tOption<target> res("option", tComands{});
    res.AddToBody(EventProperty(tEventPropertyType::kOptionDescription, eventID, optionNum));
    res.AddToBody(std::move(CastToComands(effects)), 1u);
    return res;
}

template tOption<tTarget::kCountry > Option(tEffects<tTarget::kCountry > &&, uint, uint);
template tOption<tTarget::kProvince> Option(tEffects<tTarget::kProvince> &&, uint, uint);

static tString Picture(char const * const picture)
{
    std::string res = "picture = \"";
    res += picture;
    res += '\"';
    return std::move(res);
}

static tComands CountryEventHeader(uint const id, char const * const picture)
{
    assertion(id < 1e6);
    return
    {
        Set("id", std::to_string(id)),
        tComand(EventProperty(tEventPropertyType::kEventName, id)),
        tComand(EventProperty(tEventPropertyType::kEventDesc, id)),
        tComand(Picture(picture))
    };
}

void CountryEvent(uint const id, char const * const title, char const * const desc, char const * const picture,
                  std::ostream & eventOutput, std::ostream & localizationOutput,
                  tTrigger          <tTarget::kCountry> && trigger,
                  tMeanTimeToHappen <tTarget::kCountry> && meanTimeToHappen,
                  tOptions          <tTarget::kCountry> && options,
                  tOptionDescriptions const & optionDescriptions)
{
    assertion(title && desc && picture && !options.empty() && options.size() == optionDescriptions.size());
    tComand res("country_event", std::move(CastToComands(options)));
    res.AddToBody({std::move(meanTimeToHappen)}  , 0u);
    res.AddToBody({std::move(         trigger)}  , 0u);
    res.AddToBody(CountryEventHeader(id, picture), 0u);
    eventOutput << res;
    localizationOutput << EventLocalizationReference(tEventPropertyType::kEventName, id) << ';' << title << ";X\n";
    localizationOutput << EventLocalizationReference(tEventPropertyType::kEventDesc, id) << ';' << desc  << ";X\n";
    for (uint i = 0u; i < optionDescriptions.size(); ++i)
        (localizationOutput
         << EventLocalizationReference(tEventPropertyType::kOptionDescription, id, i + 1u)
         << ';' << optionDescriptions[i] << ";X\n");
}
