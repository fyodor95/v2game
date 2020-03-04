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
tOption<target> Option(tEffects<target> && effects, uint const eventID, uint const optionNum, uint const aiChance)
{
    static_assert(target != tTarget::kPop);
    assertion(optionNum && optionNum <= 26u);
    tOption<target> res("option", tComands{});
    if (aiChance != UINT_MAX)
    {
        assertion(aiChance <= 100);
        res.AddToBody("ai_chance = { factor = " + std::to_string(aiChance) + " }");
    }
    res.AddToBody(EventProperty(tEventPropertyType::kOptionDescription, eventID, optionNum));
    res.AddToBody(std::move(CastToComands(effects)), 1u);
    return res;
}

template tOption<tTarget::kCountry > Option(tEffects<tTarget::kCountry > &&, uint, uint, uint);
template tOption<tTarget::kProvince> Option(tEffects<tTarget::kProvince> &&, uint, uint, uint);

tString Picture(char const * const eventPicture)
{
    std::string res = "picture = \"";
    res += eventPicture;
    res += '\"';
    return std::move(res);
}

tComands EventHeader(uint const id, char const * const eventPicture)
{
    assertion(id < 1e6);
    tComands res;
    res.reserve(3u + !!eventPicture);
    res =
    {
        Set("id", std::to_string(id)),
        tComand(EventProperty(tEventPropertyType::kEventName, id)),
        tComand(EventProperty(tEventPropertyType::kEventDesc, id))
    };
    if (eventPicture)
        res.emplace_back(Picture(eventPicture));
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
tComand ConstructEventWithoutHeader(tTrigger          <target> * const trigger,
                                    tMeanTimeToHappen <target> * const meanTimeToHappen,
                                    tOptions          <target> && options)
{
    assertion(!options.empty() && !!trigger == !!meanTimeToHappen);
    tComand res(EventScope<target>(), std::move(CastToComands(options)));
    if (trigger)
    {
        res.AddToBody({std::move(* meanTimeToHappen)}  , 0u);
        res.AddToBody({std::move(*          trigger)}  , 0u);
    }
    else
    {
        res.AddToBody(std::string("is_triggered_only = yes"));
    }
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

template <tTarget target>
static void Event(uint const id, char const * const title, char const * const desc,
                  std::ostream & eventOutput, std::ostream & locOutput,
                  tTrigger          <target> * const trigger,
                  tMeanTimeToHappen <target> * const meanTimeToHappen,
                  tOptions          <target> && options,
                  tOptionDescriptions const & optionDescriptions,
                  char const * const eventPicture)
{
    static_assert(target != tTarget::kPop);
    assertion(TargetIsCountry(target) == !!eventPicture);
    assertion(!!trigger == !!meanTimeToHappen);
    assertion(title && desc && options.size() == optionDescriptions.size());
    tComand res = ConstructEventWithoutHeader<target>(std::move(trigger), std::move(meanTimeToHappen),
                                                      std::move(options));
    res.AddToBody(EventHeader(id, eventPicture), 0u);
    PrintEvent<target>(res, id, title, desc, eventOutput, locOutput, optionDescriptions);
}

template <tTarget target>
void Event(uint const id, char const * const title, char const * const desc,
           std::ostream & eventOutput, std::ostream & locOutput,
           tTrigger          <target> && trigger,
           tMeanTimeToHappen <target> && meanTimeToHappen,
           tOptions          <target> && options,
           tOptionDescriptions const & optionDescriptions,
           char const * const eventPicture)
{
    static_assert(target != tTarget::kPop);
    Event<target>(id, title, desc, eventOutput, locOutput, &trigger, &meanTimeToHappen,
                  std::move(options), optionDescriptions, eventPicture);
}

#define EVENT_INSTANTIATION(TARGET)                                \
template void Event<tTarget::k##TARGET>                             \
(uint, char const *, char const *, std::ostream &, std::ostream &,   \
 tTrigger          <tTarget::k##TARGET> &&,                           \
 tMeanTimeToHappen <tTarget::k##TARGET> &&,                            \
 tOptions          <tTarget::k##TARGET> &&,                             \
 tOptionDescriptions const &,                                            \
 char const *)

EVENT_INSTANTIATION(Country);
EVENT_INSTANTIATION(Province);

void TriggeredCountryEvent(uint const id, char const * const title, char const * const desc,
                           std::ostream & eventOutput, std::ostream & locOutput,
                           tOptions           <tTarget::kCountry> && options,
                           tOptionDescriptions const & optionDescriptions,
                           char const * const eventPicture)
{
    Event<tTarget::kCountry>(id, title, desc, eventOutput, locOutput, nullptr, nullptr,
                             std::move(options), optionDescriptions, eventPicture);
}
