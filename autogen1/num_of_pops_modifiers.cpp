#include <algorithm>
#include <cmath>
#include <fstream>

#include <iostream>

double f(int i) {
    double res = round((0.02 * i + 4) * 100) / 100;
    std::cout << "f(" << i << ") = " << res << '\n';
    return res;
}

int main() {
    std::ofstream out("num_of_pops_modifiers.txt");
    std::ifstream in("num_of_pops_modifiers_text_template.txt");
    std::ofstream localisation("..\\localisation\\num_of_pops_modifiers_text.csv");
    std::string name, pop, mln, little;
    std::getline(in, name);
    std::getline(in, pop);
    std::getline(in, mln);
    std::getline(in, little);
    out.precision(2);
    double old_rate = f(200);
    out << "num_of_pops_modifier_200 = {\n";
    out << "	trigger = {\n";
    out << "		total_pops = 20000000\n";
    out << "		civilized = yes\n";
    out << "	}\n";
    out << "	global_immigrant_attract = " << old_rate << '\n';
    out << "}\n";
    localisation << name << 200 << pop << 20 << mln << '\n';
    int i_old = 200;
    for (int i = 190; i > 1; i -= std::max(1, std::min(10, i / 10))) {
        double rate = f(i);
        if (rate / old_rate > 0.9) continue;
        out << "num_of_pops_modifier_" << i << " = {\n";
        out << "	trigger = {\n";
        out << "		total_pops = " << i << "00000\n";
        out << "		NOT = { total_pops = " << i_old << "00000 }\n";
        out << "		civilized = yes\n";
        out << "	}\n";
        out << "	global_immigrant_attract = ";
        out << rate << '\n';
        out << "}\n";
        localisation << name << i << pop << i / 10.0 << mln << '\n';
        i_old = i;
        old_rate = rate;
    }
    out << "num_of_pops_modifier_1 = {\n";
    out << "	trigger = {\n";
    out << "		NOT = { total_pops = " << i_old << "00000 }\n";
    out << "		civilized = yes\n";
    out << "	}\n";
    out << "	global_immigrant_attract = " << f(1) << '\n';
    out << "}\n";
    localisation << name << 1 << little << '\n';
}
