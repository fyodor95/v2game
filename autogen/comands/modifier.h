#pragma once

#include "conditions.h"
#include "target_comand.h"

template <tTarget target>
class tModifier : public tComand
{
public:
    tModifier(float factor, tCondition<target> &&);
};

DEFINE_TARGET_CONTAINER(tModifier);

template <tTarget target>
tModifier<target>::tModifier(float const factor, tCondition<target> && condition)
    : tComand("modifier", {tComand("factor", factor), std::move(condition)})
{
}
