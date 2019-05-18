#include <fstream>
#include <string>
#include <vector>

int main()
{
    std::vector<char const *> const locations
    {
        "1490",
        "1557",
        "1600",
        "1465",
        "1576",
        "1618",
        "1614",
        "1565",
        "1611",
        "1601",
        "1578",
        "1581",
        "2062"
    };
    std::ifstream in("save.v2");
    std::ofstream out("save_output.v2");
    std::string s;
    while (std::getline(in, s))
    {
        if (s[1] == 'a' && s.find("\tarmy=") != s.npos)
        {
            std::vector<std::string> ss{s};
            bool toDelete = false;
            while (ss.back()[1] != '}')
            {
                ss.emplace_back();
                std::string & f = ss.back();
                std::getline(in, f);
                if (!toDelete && f.find("location=") != f.npos)
                    for (char const * loc : locations)
                        if (f.find(loc) != f.npos)
                        {
                            toDelete = true;
                            break;
                        }
            }
            if (!toDelete)
                for (std::string const & d : ss)
                    out << d << '\n';
        }
        else
            out << s << '\n';
    }
}
