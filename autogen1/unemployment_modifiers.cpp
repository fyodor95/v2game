#include <algorithm>
#include <cmath>
#include <fstream>

float f(int i) {
    return -4 * cbrt(i * i / 10000.0) + 0.4;
}

int main() {
    std::ofstream out("unemployment_modifiers.txt");
    std::ifstream in("unemployment_modifiers_text_template.txt");
    std::ofstream localisation("unemployment_modifiers_text.csv");
    std::string name, unemployment, percent, all_employment;
    std::getline(in, name);
    std::getline(in, unemployment);
    std::getline(in, percent);
    std::getline(in, all_employment);
    out.precision(2);
    out << "unemployment_modifier_100 = {\n";
    out << "	trigger = {\n";
    out << "		unemployment = 1\n";
    out << "		civilized = yes\n";
    out << "	}\n";
    out << "	global_immigrant_attract = " << f(100) << '\n';
    out << "}\n";
    localisation << name << 100 << unemployment << 100 << percent << '\n';
    int i_old = 100;
    for (int i = 96; i > 0; i -= std::max(1, std::min(4, i / 10))) {
        out << "unemployment_modifier_" << i << " = {\n";
        out << "	trigger = {\n";
        out << "		unemployment = " << i / 100.0 << '\n';
        out << "		NOT = { unemployment = " << i_old / 100.0 << " }\n";
        out << "		civilized = yes\n";
        out << "	}\n";
        out << "	global_immigrant_attract = ";
        float rate = f(i);
        out << (rate < 0.1 && rate > -0.1 ? round(rate * 100) / 100 : rate) << '\n';
        out << "}\n";
        localisation << name << i << unemployment << i << percent << '\n';
        i_old = i;
    }
    out << "unemployment_modifier_0 = {\n";
    out << "	trigger = {\n";
    out << "		NOT = { unemployment = 0.01 }\n";
    out << "		civilized = yes\n";
    out << "	}\n";
    out << "	global_immigrant_attract = " << f(0) << '\n';
    out << "}\n";
    localisation << name << 0 << all_employment << '\n';
}
