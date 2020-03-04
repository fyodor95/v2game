#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "find_files.h"

int main() {
    std::array<const char *, 7u> data{"GXI", "MCK", "XIN", "YNN", "XBI", "MGL", "TIB"};
    const std::string path = "..\\history\\provinces\\",
                      new_path = "..\\history\\provinces\\";
    std::vector<std::string> dirs;
    FindFiles(dirs, "..\\history\\provinces\\*");
    for (int i = 2; i < dirs.size(); ++i) {
        std::vector<std::string> files;
        const std::string subpath = path + dirs[i] + "\\*";
        FindFiles(files, subpath.c_str());
        for (auto & file : files) {
            const std::string input_path = path + dirs[i] + "\\" + file,
                              output_path = new_path + dirs[i] + "\\" + file;
            std::string str;
            std::string res;
            bool changed = false;
            {
                std::ifstream input(input_path);
                while (std::getline(input, str)) {
                    const size_t ownerPos = str.find("owner");
                    if (ownerPos != str.npos) {
                        bool ok = false;
                        for (int i = 0; i < 7; ++i) {
                            const size_t found = str.find(data[i]);
                            if (found != str.npos) {
                                ok = true;
                                break;
                            }
                        }
                        if (ok) {
                            changed = true;
                            res += str.substr(0, ownerPos) + "owner = CHI\n";
                            continue;
                        }
                    }
                    const size_t controllerPos = str.find("controller");
                    if (controllerPos != str.npos) {
                        bool ok = false;
                        for (int i = 0; i < 7; ++i) {
                            const size_t found = str.find(data[i]);
                            if (found != str.npos) {
                                ok = true;
                                break;
                            }
                        }
                        if (ok) {
                            changed = true;
                            res += str.substr(0, controllerPos) + "controller = CHI\n";
                            if (str.find("TIB") != str.npos) {
                                res += str.substr(0, controllerPos) + "add_core = CHI\n";
                            }
                            continue;
                        }
                    }
                    res += str;
                    res += '\n';
                }
            }
            if (changed) {
                std::ofstream output(output_path);
                output << res;
            }
        }
    }
}
