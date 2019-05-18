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

template <tEventPicture picture>
tString Picture()
{
    std::string res = "picture = \"";
    res += nsEventPicture::ToString<picture>();
    res += '\"';
    return std::move(res);
}

template <tEventPicture picture>
tComands EventHeader(uint const id)
{
    assertion(id < 1e6);
    tComands res;
    res.reserve(3u + (picture != tEventPicture::NONE));
    res =
    {
        Set("id", std::to_string(id)),
        tComand(EventProperty(tEventPropertyType::kEventName, id)),
        tComand(EventProperty(tEventPropertyType::kEventDesc, id))
    };
    if constexpr (picture != tEventPicture::NONE)
        res.emplace_back(Picture<picture>());
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
tComand ConstructEventWithoutHeader(tTrigger          <target> && trigger,
                                    tMeanTimeToHappen <target> && meanTimeToHappen,
                                    tOptions          <target> && options)
{
    assertion(!options.empty());
    tComand res(EventScope<target>(), std::move(CastToComands(options)));
    res.AddToBody({std::move(meanTimeToHappen)}  , 0u);
    res.AddToBody({std::move(         trigger)}  , 0u);
    return res;
}

template <tTarget target>
void PrintEvent(tComand const & event, uint const id, char const * const title, char const * const desc,
                std::ostream & eventOutput, std::ostream & locOutput, tOptionDescriptions const & optionDescriptions)
{
    eventOutput << event;
    locOutput << EventLocalizationReference(tEventPropertyType::kEventName, id) << ';' << title << LocLineEnd();
    locOutput << EventLocalizationReference(tEventPropertyType::kEventDesc, id) << ';' << desc  << LocLineEnd();
    for (uint i = 0u; i < optionDescriptions.size(); ++i)
        (locOutput
         << EventLocalizationReference(tEventPropertyType::kOptionDescription, id, i + 1u)
         << ';' << optionDescriptions[i] << LocLineEnd());
}

template <tTarget target, tEventPicture picture>
void Event(uint const id, char const * const title, char const * const desc,
           std::ostream & eventOutput, std::ostream & locOutput,
           tTrigger          <target> && trigger,
           tMeanTimeToHappen <target> && meanTimeToHappen,
           tOptions          <target> && options,
           tOptionDescriptions const & optionDescriptions)
{
    static_assert(target != tTarget::kPop);
    static_assert(TargetIsCountry(target) == (picture != tEventPicture::NONE));
    assertion(title && desc && options.size() == optionDescriptions.size());
    tComand res = ConstructEventWithoutHeader<target>(std::move(trigger), std::move(meanTimeToHappen),
                                                      std::move(options));
    res.AddToBody(EventHeader<picture>(id), 0u);
    PrintEvent<target>(res, id, title, desc, eventOutput, locOutput, optionDescriptions);
}

#define EVENT_INSTANTIATION(TARGET, PICTURE)                       \
template void Event<tTarget::k##TARGET, tEventPicture::PICTURE>     \
(uint, char const *, char const *, std::ostream &, std::ostream &,   \
 tTrigger          <tTarget::k##TARGET> &&,                           \
 tMeanTimeToHappen <tTarget::k##TARGET> &&,                            \
 tOptions          <tTarget::k##TARGET> &&,                             \
 tOptionDescriptions const &)

#define COUNTRY_EVENT_INSTANTIATION(PICTURE) EVENT_INSTANTIATION(Country, PICTURE)

COUNTRY_EVENT_INSTANTIATION(Administration);

EVENT_INSTANTIATION(Province, NONE);
