#include <algorithm>
#include <cmath>
#include <fstream>

float f(int i) {
    return -40.0 / i + 1;
}

int main() {
    std::ofstream out("num_of_pops_modifiers.txt");
    std::ifstream in("num_of_pops_modifiers_text_template.txt");
    std::ofstream localisation("num_of_pops_modifiers_text.csv");
    std::string name, pop, mln, little;
    std::getline(in, name);
    std::getline(in, pop);
    std::getline(in, mln);
    std::getline(in, little);
    out.precision(2);
    out << "num_of_pops_modifier_200 = {\n";
    out << "	trigger = {\n";
    out << "		total_pops = 20000000\n";
    out << "		civilized = yes\n";
    out << "	}\n";
    out << "	global_immigrant_attract = " << f(200) << '\n';
    out << "}\n";
    localisation << name << 200 << pop << 20 << mln << '\n';
    int i_old = 200;
    for (int i = 190; i > 1; i -= std::max(1, std::min(10, i / 10))) {
        out << "num_of_pops_modifier_" << i << " = {\n";
        out << "	trigger = {\n";
        out << "		total_pops = " << i << "00000\n";
        out << "		NOT = { total_pops = " << i_old << "00000 }\n";
        out << "		civilized = yes\n";
        out << "	}\n";
        out << "	global_immigrant_attract = ";
        float rate = f(i);
        out << (rate < 0.1 ? round(rate * 100) / 100 : rate) << '\n';
        out << "}\n";
        localisation << name << i << pop << i / 10.0 << mln << '\n';
        i_old = i;
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
