// Build the bundled original devotional with SWORD's own writer.
#include <rawld.h>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    std::ifstream input(argv[1]);
    if (!input || sword::RawLD::createModule(argv[2])) return 1;
    sword::RawLD module(argv[2]);
    std::string line;
    int entries = 0;
    while (std::getline(input, line)) {
        auto tab = line.find('\t');
        if (tab != 5 || line.empty()) return 1;
        module.setKey(line.substr(0, tab).c_str());
        module.setEntry(line.substr(tab + 1).c_str());
        ++entries;
    }
    return entries == 366 ? 0 : 1;
}
