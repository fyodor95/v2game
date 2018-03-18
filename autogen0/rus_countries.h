#pragma once
#include <fstream>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::string> GetRusCountries() {
    std::unordered_map<std::string, std::string> res;
    std::ifstream inp("..//localisation//all_countries.csv");
    std::string tag, rus_name, unused;
    while (std::getline(inp, tag, ';')) {
        std::getline(inp, rus_name, ';');
        res[tag] = rus_name;
        std::getline(inp, unused);
    }
    return res;
}
