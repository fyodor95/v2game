#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "find_files.h"

int main() {
    std::string what_delete;
    std::cin >> what_delete;
    what_delete.insert(0, "add_core = ");
    const std::string path = "..\\history\\provinces\\",
                      new_path = "provinces\\";
    std::vector<std::string> dirs;
    FindFiles(dirs, "..\\history\\provinces\\*");
    for (const std::string& dir : dirs) std::cout << dir << '\n';
    for (int i = 2; i < dirs.size(); ++i) {
        std::vector<std::string> files;
        const std::string subpath = path + dirs[i] + "\\*";
        FindFiles(files, subpath.c_str());
        for (auto & file : files) {
            std::cout << file << '\n';
            const std::string input_path = path + dirs[i] + "\\" + file,
                              output_path = new_path + dirs[i] + "\\" + file;
            std::string str;
            std::ifstream input(input_path);
            std::ofstream output(output_path);
            while (std::getline(input, str)) {
                if (str.find(what_delete) == std::string::npos) {
                    output << str << '\n';
                }
            }
        }
    }
}
