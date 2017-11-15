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
    std::ofstream out("..\\events\\seting_D_cores.txt");
    std::vector<std::string> files, d_tags;
    FindFiles(files, "..\\history\\countries\\*");
	int id = 300;
    for (int i = 2; i < files.size(); ++i) {
        std::string tag = files[i].substr(0, 3);
        if (tag[0] == 'D' && '0' <= tag[1] && tag[1] < '6') d_tags.push_back(tag);
    }
    for (auto & d_tag : d_tags) {
        out << "province_event = {\n";
        out << "	id = " << ++id << "\n";
        out << "	title = \"EVTNAME3XX\"\n";
        out << "	desc = \"EVTDESC3XX\"\n";
        out << "	trigger = {\n";
        out << "		owner = {\n";
        out << "			" << d_tag << " = { exists = no }\n";
        out << "		}\n";
        out << "		controlled_by_rebels = yes\n";
        out << "		OR = {\n";
        out << "			NOT = {\n";
        out << "				owner = {\n";
        out << "					any_owned_province = {\n";
        out << "						is_core = " << d_tag << "\n";
        out << "					}\n";
        out << "				}\n";
        out << "			}\n";
        out << "			any_neighbor_province = {\n";
        out << "				controlled_by_rebels = yes\n";
        out << "				is_core = " << d_tag << "\n";
        out << "			}\n";
        out << "		}\n";
        out << "	}\n";
        out << "	mean_time_to_happen = {\n";
        out << "		months = 1200\n";
        out << "		modifier = {\n";
        out << "			factor = 0.001\n";
        out << "			any_neighbor_province = {\n";
        out << "				controlled_by_rebels = yes\n";
        out << "				is_core = " << d_tag << "\n";
        out << "			}\n";
        out << "		}\n";
        out << "		modifier = {\n";
        out << "			factor = 0.025\n";
        out << "			NOT = {\n";
        out << "				OR = {\n";
        for (int i = 2; i < files.size(); ++i) {
            out << "					is_core = " << files[i].substr(0, 3) << "\n";
        }
        out << "				}\n";
        out << "			}\n";
        out << "		}\n";
        out << "	}\n";
        out << "	option = {\n";
        out << "		name = \"EVTOPTA3XX\"\n";
        out << "		add_core = " << d_tag << "\n";
        out << "		any_neighbor_province = {\n";
        out << "			add_core = " << d_tag << "\n";
        out << "		}\n";
        out << "	}\n";
        out << "}\n";
    }
}
