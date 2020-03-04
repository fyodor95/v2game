#include "war_policy.h"

namespace nsWarPolicy
{

    template <>
    char const * ToString<tWarPolicy::jingoism>()
    {
        return "jingoism";
    }

    template <>
    char const * ToString<tWarPolicy::pro_military>()
    {
        return "pro_military";
    }

    template <>
    char const * ToString<tWarPolicy::anti_military>()
    {
        return "anti_military";
    }

    template <>
    char const * ToString<tWarPolicy::pacifism>()
    {
        return "pacifism";
    }

}
