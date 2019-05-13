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

static tComands EventHeader(uint const id, char const * const picture = nullptr)
{
    assertion(id < 1e6);
    tComands res;
    res.reserve(3u + !!picture);
    res =
    {
        Set("id", std::to_string(id)),
        tComand(EventProperty(tEventPropertyType::kEventName, id)),
        tComand(EventProperty(tEventPropertyType::kEventDesc, id))
    };
    if (picture)
        res.emplace_back(Picture(picture));
    return res;
}

template <tTarget>
char const * EventScope();

template <>
char const * EventScope<tTarget::kCountry>()
{
    return "country_event";
}

template <>
char const * EventScope<tTarget::kProvince>()
{
    return "province_event";
}

template <tTarget target>
void Event(uint const id, char const * const title, char const * const desc,
           std::ostream & eventOutput, std::ostream & locOutput,
           tTrigger          <target> && trigger,
           tMeanTimeToHappen <target> && meanTimeToHappen,
           tOptions          <target> && options,
           tOptionDescriptions const & optionDescriptions, char const * const picture)
{
    static_assert(target != tTarget::kPop);
    assertion(title && desc && !options.empty() && options.size() == optionDescriptions.size());
    assertion(TargetIsCountry(target) == !!picture);
    tComand res(EventScope<target>(), std::move(CastToComands(options)));
    res.AddToBody({std::move(meanTimeToHappen)}  , 0u);
    res.AddToBody({std::move(         trigger)}  , 0u);
    res.AddToBody(EventHeader(id, picture)       , 0u);
    eventOutput << res;
    locOutput << EventLocalizationReference(tEventPropertyType::kEventName, id) << ';' << title << LocLineEnd();
    locOutput << EventLocalizationReference(tEventPropertyType::kEventDesc, id) << ';' << desc  << LocLineEnd();
    for (uint i = 0u; i < optionDescriptions.size(); ++i)
        (locOutput
         << EventLocalizationReference(tEventPropertyType::kOptionDescription, id, i + 1u)
         << ';' << optionDescriptions[i] << LocLineEnd());
}

template void Event(uint, char const *, char const *,
                    std::ostream &, std::ostream &,
                    tTrigger          <tTarget::kCountry> &&,
                    tMeanTimeToHappen <tTarget::kCountry> &&,
                    tOptions          <tTarget::kCountry> &&,
                    tOptionDescriptions const &, char const *);

template void Event(uint, char const *, char const *,
                    std::ostream &, std::ostream &,
                    tTrigger          <tTarget::kProvince> &&,
                    tMeanTimeToHappen <tTarget::kProvince> &&,
                    tOptions          <tTarget::kProvince> &&,
                    tOptionDescriptions const &, char const *);
