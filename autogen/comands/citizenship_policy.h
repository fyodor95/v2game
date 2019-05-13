#pragma once

enum class tCitizenshipPolicy : uint8_t
{
    kResidency, // вид на жительство
    kLimited,   // ограниченное гражданство
    kFull       // полное гражданство
};

namespace nsCitizenshipPolicy
{
    template <tCitizenshipPolicy>
    char const * ToString();
}
