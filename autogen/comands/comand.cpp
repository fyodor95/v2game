#include "comand.h"

tString::tString(std::string && str, tShift const shift)
    : xData(std::move(str))
    , xShift(shift)
{
}

void tString::Print(std::ostream & os, uint & tab) const
{
    if (xShift == tShift::kDecrease)
        --tab;
    for (uint i = 0u; i < tab; ++i)
        os << '\t';
    if (xShift == tShift::kIncrease)
        ++tab;
    os << xData << '\n';
}

tString Set(std::string && lhs, std::string_view const & rhs)
{
    lhs.reserve(lhs.size() + rhs.size() + 3u);
    lhs += " = ";
    lhs += rhs;
    return std::move(lhs);
}

static tComandBase ConstructComandBase(std::string && name)
{
    name += " = {";
    return {tString(std::move(name), tShift::kIncrease), tString("}", tShift::kDecrease)};
}

tComand::tComand(std::string && name, tComands && body)
    : tComandBase(ConstructComandBase(std::move(name)))
{
    AddToBody(std::move(body), 0u);
}

tComand::tComand(tString && string)
    : tComandBase{std::move(string)}
{
}

tComand::tComand(std::string && parameterName, float const parameterValue)
    : tComandBase{Set(std::move(parameterName), ToString(parameterValue))}
{
}

void tComand::AddToBody(tString && comand)
{
    this->insert_after(this->begin(), std::move(comand));
}

void tComand::AddToBody(tComands && comands, uint const pos)
{
    auto const placeToAdd = next(this->begin(), pos);
    for (auto it = comands.rbegin(); it != comands.rend(); ++it)
        this->splice_after(placeToAdd, *it);
}

std::ostream & operator<<(std::ostream & os, tComand const & comand)
{
    uint tab = 0u;
    for (tString const & string : comand)
        string.Print(os, tab);
    return os;
}
