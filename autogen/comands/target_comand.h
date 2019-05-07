#pragma once

#include "comand.h"
#include "target.h"

#define DEFINE_TARGET_CONTAINER(TYPE) DEFINE_TEMPLATE_CONTAINER(TYPE, tTarget)

#define COMAND(NAME)          \
template <tTarget target>      \
class NAME : public tComand     \
{                                \
public:                           \
    using tComand::tComand;        \
};                                  \
                                     \
DEFINE_TARGET_CONTAINER(NAME)
