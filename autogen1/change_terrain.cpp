#include <array>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "find_files.h"

std::string ReadFile(char const * const fileName) {
    std::ifstream input(fileName);
    std::string str, res;
    while (std::getline(input, str)) {
        res += str;
        res += '\n';
    }
    return res;
}

int main() {
    std::string const chronology = "C:\\mod\\chronology\\";
    std::string const terrain = ReadFile((chronology + "map\\terrain.txt").data());
    std::string const path = chronology + "history\\provinces\\",
                      new_path = "..\\history\\provinces\\";
    std::vector<std::string> dirs;
    FindFiles(dirs, (path + '*').data());
    for (int i = 2; i < dirs.size(); ++i) {
        std::vector<std::string> files;
        std::string const subpath = path + dirs[i] + "\\*";
        FindFiles(files, subpath.c_str());
        for (std::string const & file : files) {
            std::string const input_path = path + dirs[i] + "\\" + file,
                              output_path = new_path + dirs[i] + "\\" + file;
            if (std::ifstream(output_path).fail())
            {
                std::cout << file << '\n';
                continue;
            }
            std::string str;
            std::string res;
            bool changed = false;
            {
                std::ifstream input(input_path);
                while (std::getline(input, str)) {
                    size_t const terrainPos = str.find("terrain");
                    if (terrainPos != str.npos) {
                        changed = true;
                        size_t const cityPos = str.find("city");
                        if (cityPos != str.npos) {
                            size_t const cityPos2 = terrain.find(str.substr(cityPos, str.size() - cityPos));
                            size_t const realTerrainPos = terrain.find("#", cityPos2);
                            assert(realTerrainPos != str.npos);
                            size_t realTerrainEnd = realTerrainPos;
                            for (; !isspace(terrain[realTerrainEnd]); ++realTerrainEnd);
                            res += str.substr(0, terrainPos);
                            res += "terrain = ";
                            res += terrain.substr(realTerrainPos + 1, realTerrainEnd - realTerrainPos - 1);
                        }
                        else {
                            res += str;
                        }
                        res += '\n';
                        continue;
                    }
                }
            }
            if (changed) {
                res += ReadFile(output_path.data());
                std::ofstream(output_path) << res;
            }
        }
    }
}
