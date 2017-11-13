#include <fstream>
#include <iostream>
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
    std::vector<std::string> files, tags;
    FindFiles(files, "C:\\V2HoD3.03_Rus\\history\\countries\\*");
    tags.reserve(files.size() - 2);
    for (int i = 2; i < files.size(); ++i) tags.push_back(files[i].substr(0, 3));
    for (auto & tag : tags) std::cout << tag << '\n';
}
