#pragma once

enum class tCitizenshipPolicy : uint16_t
{
    kResidency, // вид на жительство
    kLimited,   // ограниченное гражданство
    kFull       // полное гражданство
};

char const * ToString(tCitizenshipPolicy);
