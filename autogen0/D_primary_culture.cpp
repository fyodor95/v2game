#include <fstream>
#include <string>
#include <vector>

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
    std::ifstream input("cultures_orig.txt");
    std::ofstream out("D_primary_cultures.txt");
    std::vector<std::string> files, d_tags;
    FindFiles(files, "C:\\V2HoD3.03_Rus\\history\\countries\\*");
    for (int i = 2; i < files.size(); ++i) {
        std::string tag = files[i].substr(0, 3);
        if (tag[0] == 'D' && '0' <= tag[1] && tag[1] < '6') d_tags.push_back(tag);
    }
    std::string culture;
    out << "political_decisions = {\n";
    while (std::getline(input, culture)) {
        out << "	D_primary_" << culture << " = {\n";
        out << "		potential = {\n";
        out << "			primary_culture = british\n";
        out << "			capital_scope = { culture = " << culture << " }\n";
        out << "			OR = {\n";
        for (auto & tag : d_tags) {
            out << "				tag = " << tag << "\n";
        }
        out << "			}\n";
        out << "		}\n";
        out << "		allow = {\n";
        out << "			capital_scope = { culture = " << culture << " }\n";
        out << "		}\n";
        out << "		effect = {\n";
        out << "			primary_culture = " << culture << "\n";
        out << "			any_pop = {\n";
        out << "				limit = {\n";
        out << "					OR = {\n";
        out << "						is_accepted_culture = yes\n";
        out << "						is_primary_culture = yes\n";
        out << "					}\n";
        out << "				}\n";
        out << "				militancy = -10\n";
        out << "			}\n";
        out << "		}\n";
        out << "		ai_will_do = {\n";
        out << "			factor = 1\n";
        out << "		}\n";
        out << "	}\n";
    }
    out << "}\n";
}
