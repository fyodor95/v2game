#include "events.h"

static bool CheckCorectness
(char const * const property, char const * const propertyName, uint const optionNum)
{
    using tPropertyAndName = tCouple<std::string_view>;
    static tPropertyAndName const name {"name", "OPT"};
    if (optionNum)
        return name[0] == property && name[1] == propertyName;
    static tCouple<tPropertyAndName> const tittleAndDesc
    {
        tPropertyAndName{"title", "NAME"},
        tPropertyAndName{"desc" , "DESC"}
    };
    for (int const i : {0, 1})
        if (tittleAndDesc[i][0] == property && tittleAndDesc[i][1] == propertyName)
            return true;
    return false;
}

tString EventProperty(char const * const property, char const * const propertyName,
                      uint const eventID, uint const optionNum)
{
    assertion(CheckCorectness(property, propertyName, optionNum));
    std::string name;
    name += property;
    name += " = \"EVT";
    name += propertyName;
    if (optionNum)
        name += static_cast<char>('A' + optionNum - 1u);
    name += std::to_string(eventID);
    name += '\"';
    return std::move(name);
}

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
        tComand(EventProperty("title", "NAME", id)),
        tComand(EventProperty( "desc", "DESC", id)),
        tComand(Picture(picture))
    };
}

tComand CountryEvent(uint const id, char const * const picture,
                     tTrigger          <tTarget::kCountry> && trigger,
                     tMeanTimeToHappen <tTarget::kCountry> && meanTimeToHappen,
                     tOptions          <tTarget::kCountry> && options)
{
    assertion(picture && !options.empty());
    tComand res("country_event", std::move(CastToComands(options)));
    res.AddToBody({std::move(meanTimeToHappen)}  , 0u);
    res.AddToBody({std::move(         trigger)}  , 0u);
    res.AddToBody(CountryEventHeader(id, picture), 0u);
    return res;
}
