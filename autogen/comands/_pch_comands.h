#pragma once

#include <array>
#include <cstdint>
#include <forward_list>
#include <fstream>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

void DynamicAssertion(bool expr, char const * file, int line, char const * exprStr);

#define assertion(EXPR) DynamicAssertion(!!(EXPR), __FILE__, __LINE__, (#EXPR))

#define DebugIf(EXPR) if (EXPR)

using uint = unsigned;

template <class T>
using tCouple = std::array<T, 2>;

#define SINGLETON(TYPE, NAME)                      \
TYPE const & NAME();                                \
static TYPE Construct##NAME();                       \
TYPE const & NAME()                                   \
{                                                      \
    static TYPE const storage = Construct##NAME();      \
    return storage;                                      \
}                                                         \
TYPE Construct##NAME()

#define DELETE_COPY_AND_MOVE_CONSTRUCTORS(CLASS_NAME) \
CLASS_NAME(CLASS_NAME const &) = delete;               \
CLASS_NAME(CLASS_NAME      &&) = delete

#define DEFINE_CONTAINER(TYPE) using TYPE##s = std::vector<TYPE>

#define DEFINE_TEMPLATE_CONTAINER(TYPE, TEMPLATE_ARG) \
template <TEMPLATE_ARG arg>                            \
using TYPE##s = std::vector<TYPE<arg>>

std::string ToString(float);

enum tYesOrNo
{
    _No = 0,
    _Yes = 1
};

char const * YesOrNo(tYesOrNo);

char const * LocLineEnd();
