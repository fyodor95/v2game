#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

#include <windows.h>

void FindFiles(std::vector<std::string> &mem, const char *path) {
    WIN32_FIND_DATA FindFileData;
    HANDLE hf = FindFirstFile(path, &FindFileData);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            mem.emplace_back(FindFileData.cFileName);
        } while (FindNextFile(hf, &FindFileData));
        FindClose(hf);
    }
}

int main() {
    std::unordered_set<int> what_change = {
        2631,5,4,3,2599,2630,6,7,8,2598,10,9,11,2596,12,43,2595,44,37,38,45,2592,2626,2627,251,2591,2632,2636,
        307,321,338,982,2587,983,984,2588,981,2589,985,986,2699,2688,2687,2614,2684,2682,94,95,96,100,102,2312,
        2314,907,906,909,920,921,922,911,908,910,931,1156,1153,1151,1163,1164,1173,1175,1174,1172,1170,1169,
        1167,934,1144,1148,1214,1215,1594,1598,2607,1488,2503,2496,2495,2507,1756,1757,1759,1758,1760,1761,
        1766,2564,1835,1836,1837,1838,1737,1732,1744,1743,1742,1741,1696,1699,1698,1697,1715,1713,1711,1712,
        1710,1727,1729
    };
    std::string path = "C:\\V2HoD3.03_Rus\\history\\provinces\\",
                new_path = "C:\\V2HoD3.03_Rus\\autogen\\provinces\\";
    std::vector<std::string> dirs;
    FindFiles(dirs, "C:\\V2HoD3.03_Rus\\history\\provinces\\*");
    for (auto & dir : dirs) std::cout << dir << '\n';
    for (int i = 2; i < dirs.size(); ++i) {
        std::vector<std::string> files;
        std::string subpath = path + dirs[i] + "\\*";
        FindFiles(files, subpath.c_str());
        for (auto & file : files) {
            int j;
            for (j = 1; file[j] <= '9' && file[j] >= '0'; ++j);
            if (what_change.find(atoi(file.substr(0, j).c_str())) != what_change.end()) {
                std::cout << file << '\n';
                std::string input_path = path + dirs[i] + "\\" + file,
                            output_path = new_path + dirs[i] + "\\" + file,
                            string;
                std::ifstream input(input_path);
                std::ofstream output(output_path);
                while (std::getline(input, string)) {
                    int begin, end;
                    if (string.find("life_rating") != std::string::npos) {
                        for (begin = 12; string[begin] > '9' || string[begin] < '0'; ++begin);
                        end = begin;
                        for (; end < string.size() && string[end] <= '9' && string[end] >= '0'; ++end);
                        int new_life_rating = atoi(string.substr(begin, end - begin).c_str()) / 2;
                        string = "life_rating = " + std::to_string(std::max(10, new_life_rating));
                    }
                    output << string << '\n';
                }
            }
        }
    }
}
