#pragma once

#include <string>
#include <vector>

#include <windows.h>

void FindFiles(std::vector<std::string>& mem, const char* path) {
    WIN32_FIND_DATA FindFileData;
    HANDLE hf = FindFirstFile(path, &FindFileData);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            mem.emplace_back(FindFileData.cFileName);
        } while (FindNextFile(hf, &FindFileData));
        FindClose(hf);
    }
}
