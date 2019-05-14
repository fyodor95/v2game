#include "events_id_checker.h"

static void FindFiles(std::vector<std::string> & mem, char const * const path)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hf = FindFirstFile(path, &FindFileData);
    if (hf == INVALID_HANDLE_VALUE)
        return;
    do
    {
        mem.emplace_back(FindFileData.cFileName);
    }
    while (FindNextFile(hf, &FindFileData));
    FindClose(hf);
}

void CheckEventsID()
{
    std::ofstream output("report.txt");
    std::set<int> ids;
    std::string path = "events\\", new_path = "";
    std::vector<std::string> files;
    FindFiles(files, "events\\*");
    for (size_t i = 2u; i < files.size(); ++i)
    {
        std::string input_path = path + files[i], string;
        std::ifstream input(input_path);
        int scope_level = 0;
        while (std::getline(input, string))
        {
            size_t comment_check = string.find("#");
            if (comment_check != std::string::npos) string = string.substr(0, comment_check);
            size_t id_find = string.find("id");
            if (id_find != std::string::npos && (string[id_find + 2] == '=' || isspace(string[id_find + 2])))
            {
                if (scope_level == 1)
                {
                    size_t pos = id_find + 3;
                    for (; pos < string.size() && !('0' <= string[pos] && string[pos] <= '9'); ++pos);
                    size_t last = pos;
                    for (; last < string.size() && '0' <= string[last] && string[last] <= '9'; ++last);
                    int id = atoi(string.substr(pos, last - pos).c_str());
                    if (ids.find(id) != ids.end())
                    {
                        output << "Error: event id " << id << " occurs several times! The file is \""
                               << files[i] << "\".\n";
                        output.close();
                        assertion(false);
                    }
                    else
                    {
                        ids.insert(id);
                    }
                }
            }
            else
            {
                for (size_t j = 0u; j < string.size(); ++j)
                {
                    if (string[j] == '{') ++scope_level;
                    if (string[j] == '}') --scope_level;
                }
            }
        }
    }
    output << "\nAll ids given below.\n";
    for (auto id : ids) output << id << '\n';
}
