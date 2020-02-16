#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "find_files.h"

int main() {
    std::vector<std::pair<std::string, std::vector<int>>> data
    {
        {
            "UKR",
            {954, 958, 959, 960, 961, 962, 963, 965, 968, 969, 970, 971, 972, 973, 974, 975, 976, 978, 980, 955, 956, 957, 979, 1047, 1046, 1045, 1048}
        },
        {
            "POL",
            {710, 712, 713, 716, 717, 706, 707, 708, 709, 711, 715}
        }
    };
    std::unordered_map<int, char const *> proc_data;
    for (auto & e : data)
        for (int num : e.second)
            proc_data.emplace(num, e.first.data());
    const std::string path = "..\\history\\provinces\\",
                      new_path = "..\\history\\provinces\\";
    std::vector<std::string> dirs;
    FindFiles(dirs, "..\\history\\provinces\\*");
    for (const std::string& dir : dirs) std::cout << dir << '\n';
    for (int i = 2; i < dirs.size(); ++i) {
        std::vector<std::string> files;
        const std::string subpath = path + dirs[i] + "\\*";
        FindFiles(files, subpath.c_str());
        for (auto & file : files) {
            std::cout << file << '\n';
            size_t end = file.find(' ');
            int num = atoi(file.substr(0, end).data());
            auto pos = proc_data.find(num);
            if (pos == proc_data.end())
                continue;
            const std::string input_path = path + dirs[i] + "\\" + file,
                              output_path = new_path + dirs[i] + "\\" + file;
            std::string str;
            std::string res;
            {
                std::ifstream input(input_path);
                while (std::getline(input, str)) {
                    if (str.substr(0, 5) == "owner") {
                        res += "owner = ";
                        res += pos->second;
                        res += '\n';
                    } else {
                        res += str;
                        res += '\n';
                    }
                }
            }
            std::ofstream output(output_path);
            output << res;
        }
    }
}
