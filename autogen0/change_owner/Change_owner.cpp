#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream inp_num("inp_num_of_provinces.txt");
    ifstream names("names.txt");
    const string tag = "FIN"; // Кому передать провинции
    int s;
    vector<int> num_of_file;
    while (inp_num >> s) num_of_file.push_back(s);
    sort(num_of_file.begin(), num_of_file.end());
    for (int i = 0; i < num_of_file.size(); ++i) {
        vector<string> v;
        string ss = to_string(num_of_file[i]), fn;
        while (getline(names, fn)) if (fn.substr(0, fn.find(" ")) == ss) break;
        ifstream inp(fn);
        string rs;
        while (getline(inp, rs)) v.push_back(rs);
        inp.close();
        ofstream outp(fn);
        outp << "owner = " << tag << '\n';
        outp << "controller = " << tag << '\n';
        for (int i = 2; i < v.size(); ++i) outp << v[i] << '\n';
        outp.close();
    }
    inp_num.close();
    names.close();
}
