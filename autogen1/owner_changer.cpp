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
        },
        {
            "CRI",
            {964, 966, 967}
        },
        {
            "ROM",
            {676, 677, 678}
        },
        {
            "LIT",
            {360, 361, 362, 363, 364, 365, 938, 947}
        },
        {
            "LAT",
            {354, 355, 356, 357, 358, 359}
        },
        {
            "EST",
            {349, 350, 351, 352, 353}
        },
        {
            "EST",
            {349, 350, 351, 352, 353}
        },
        {
            "FIN",
            {982}
        },
        {
            "AZB",
            {1102, 1105, 1106, 1107, 1103}
        },
        {
            "ARM",
            {1093, 1098, 1099, 1100, 1101}
        },
        {
            "GEO",
            {1090, 1093, 1091, 1092, 1094}
        },
        {
            "D01",
            {1052, 1053, 1054, 1055, 1104, 1095, 1049, 1051, 1056}
        },
        {
            "D02",
            {1057, 1050}
        },
        {
            "D03",
            {1041}
        },
        {
            "D04",
            {1040, 1036}
        },
        {
            "D05",
            {1039}
        },
        {
            "BYE",
            {944, 945, 946, 948, 949, 718, 941, 942, 719, 936, 937, 939, 940}
        },
        {
            "D06",
            {988, 984}
        },
        {
            "D07",
            {983, 2587}
        },
        {
            "D08",
            {943, 999, 1000, 1007, 994, 996, 997, 1006, 1003, 1004, 1005, 2580, 995}
        },
        {
            "D09",
            {2687, 2688, 2699}
        },
        {
            "D10",
            {2589}
        },
        {
            "D11",
            {2588, 981, 985, 986}
        },
        {
            "D12",
            {1182, 1183, 2610, 2611, 2702, 1184, 1187}
        },
        {
            "D13",
            {1065, 2682, 2690, 2615, 2681, 2646, 2683}
        },
        {
            "D14",
            {1068}
        },
        {
            "D15",
            {2680, 2657, 2693, 2655, 2675, 2694, 2701, 2656, 1070, 2691, 2692, 1076, 2677, 2676, 2645, 2696, 1080, 2641}
        },
        {
            "D16",
            {1081, 2697}
        },
        {
            "D17",
            {2685, 1062, 1058, 1061, 1059, 1064, 2686, 1066, 1077, 1067, 1063, 2698, 1075, 1078, 1079}
        },
        {
            "D18",
            {1072}
        },
        {
            "D19",
            {1069, 2671, 2695, 2650, 2651, 2649, 2672, 2644, 2669, 2648, 2642, 2668, 1071}
        },
        {
            "D20",
            {2590, 2637}
        },
        {
            "D21",
            {1074}
        },
        {
            "D22",
            {2639, 2618, 2638, 1073, 2663, 2634, 2633, 2616}
        },
        {
            "D23",
            {2631, 5, 4}
        },
        {
            "D24",
            {3}
        },
        {
            "D25",
            {1, 2}
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
                    } else if (str.substr(0, 10) == "controller") {
                        res += "controller = ";
                        res += pos->second;
                        res += '\n';
                        res += "add_core = ";
                        res += pos->second;
                        res += '\n';
                    } else if (str != std::string("add_core = ") + pos->second) {
                        res += str;
                        res += '\n';
                    }
                }
            }
            std::ofstream output(output_path);
            output << res;
        }
    }
    std::ofstream out("additional_output.txt");
    for (auto & e : data) {
        out << e.first << " = {\n"
            << "	value = 125\n"
            << "	level = 5\n"
            << "	influence_value = 25\n"
            << "}\n\n";
    }
    for (auto & e : data) {
        out << "substate = {\n"
            << "	first = RUS\n"
            << "	second = " << e.first << "\n"
            << "	start_date = 1809.9.5\n"
            << "	end_date = 1917.12.6\n"
            << "}\n";
    }
}
