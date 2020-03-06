#pragma once

enum class tShift : uint16_t
{
    kDecrease,
    kTheSame,
    kIncrease
};

class tString
{
public:
    tString(std::string &&, tShift = tShift::kTheSame);
    void Print(std::ostream &, uint & tab) const;

private:
    std::string xData;
    tShift xShift;
};

tString Set(std::string && lhs, std::string_view const & rhs);

using tComandBase = std::forward_list<tString>;

class tComand : private tComandBase
{
public:
    DEFINE_CONTAINER(tComand);

    tComand(std::string && name, tComands && body);
    tComand(tString &&);
    tComand(std::string && parameterName, float parameterValue);

    // Если pos == 0, то добавление будет происходить после нулевого элемента списка.
    // Если pos == 1, то добавление будет происходить после  первого элемента списка, и так далее.
    void AddToBody(tComands &&, uint pos);

    // Добавляет в начало тела.
    void AddToBody(tString &&);

    friend std::ostream & operator<<(std::ostream &, tComand const &);
};

DEFINE_CONTAINER(tComand);

std::ostream & operator<<(std::ostream &, tComand const &);

template <class T>
tComands & CastToComands(std::vector<T> & input)
{
    static_assert(sizeof(T) == sizeof(tComand) && std::is_base_of_v<tComand, T>);
    return *reinterpret_cast<tComands *>(&input);
}
