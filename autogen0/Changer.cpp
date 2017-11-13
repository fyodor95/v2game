#include <bits/stdc++.h>
using namespace std;
int main() {
    ifstream c("Kyrgyzstan.txt");
    ofstream f("Tatarstan.txt");
    const string tag1 = "KYR", tag2 = "TTT 	= ";
    string s, s_old, tag, what_prim_cult;
    //string::npos = 999;
    while (getline(c, s)) {
        size_t ff = s.find(tag1);
        if (ff != string::npos) {
            //cout << "TRUE";
            s[ff] = tag2[0];
            s[ff + 1] = tag2[1];
            s[ff + 2] = tag2[2];
        }
        f << s << '\n';
    }
    c.close();
    f.close();
}

