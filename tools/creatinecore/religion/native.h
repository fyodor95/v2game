#pragma once
#include "calculation.h"
#include <windows.h>
#include <string>
#include <vector>

namespace state_religion_native {
struct GameString {
    union { char inlineText[16]; char* pointer; } data;
    unsigned length;
    unsigned capacity;
};
struct CountryPopulation {
    DWORD country;
    DWORD index;
    DWORD primary;
    DWORD current;
    std::vector<DWORD> religions;
    std::vector<state_religion::Population> pops;
};
bool Read(DWORD address, void* out, unsigned bytes);
DWORD Read32(DWORD address);
std::vector<DWORD> PointerVector(DWORD address, unsigned maximum);
std::string String(DWORD address);
bool Contracts(DWORD base);
bool ExecutableMatches();
DWORD CountryAt(DWORD base, DWORD index, DWORD tag);
DWORD PlayerCountry(DWORD base);
std::vector<DWORD> Religions(DWORD base);
CountryPopulation ReadPopulation(DWORD base, void* context);
bool ApplyReligion(DWORD base, void* context, DWORD religion);
void InitString(GameString* string);
void SetString(DWORD base, GameString* string, const char* value);
void FreeString(DWORD base, GameString* string);
void SetText(DWORD base, void* widget, const char* text);
}
