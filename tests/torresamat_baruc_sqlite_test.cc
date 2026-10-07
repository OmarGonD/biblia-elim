#include "backend/sqlite/sqlite_bible_backend.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <string>

int main(int argc, char **argv) {
    SqliteBibleBackend backend(argc == 2 ? argv[1] : SRCDIR "/modulos/sqlite");
    const std::string module = "TorresAmat";
    std::map<std::string, std::string> expected;
    std::ifstream fixture(SRCDIR "/scripts/torresamat/baruc_texto.tsv");
    std::string line;
    while (std::getline(fixture, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos) return 2;
        expected.emplace(line.substr(0, tab), line.substr(tab + 1));
    }
    if (expected.size() != 213) return 2;
    int failures = 0;
    auto check = [&](bool ok, const std::string &message) {
        if (!ok) { ++failures; std::fprintf(stderr, "%s\n", message.c_str()); }
    };
    constexpr int counts[] = {22,35,38,37,9,72};
    check(backend.hasModule(module), "TorresAmat missing");
    check(backend.versification(module) == "Vulg", "Vulgate versification changed");
    std::size_t checked = 0;
    for (int c = 1; c <= 6; ++c) {
        BibleKeyInfo first;
        check(backend.resolveKey(module, "Bar " + std::to_string(c) + ":1", first), "chapter missing");
        const auto chapter = backend.getChapter(module, first.reference, false);
        check(chapter.size() == static_cast<std::size_t>(counts[c-1]), "chapter size " + std::to_string(c));
        for (int v = 1; v <= counts[c-1]; ++v) {
            const auto ref = "Bar " + std::to_string(c) + ":" + std::to_string(v);
            BibleKeyInfo key;
            check(backend.resolveKey(module, ref, key), ref);
            const auto content = backend.getVerseContent(module, key.reference);
            check(content.valid && content.plainText == expected.at(ref), ref + " verse text");
            bool found = false;
            for (const auto &entry : chapter)
                if (entry.reference.verse == v && entry.text == expected.at(ref)) found = true;
            check(found, ref + " chapter text");
            ++checked;
        }
    }
    BibleSearchQuery query;
    query.mode = BibleSearchMode::Indexed;
    query.text = "\"Hijos tened buen ánimo\"";
    bool found = false;
    for (const auto &result : backend.search(module, query)) {
        if (result.osisRef == "Bar.4.27") found = true;
        check(result.osisRef != "Bar.3.27", "encouragement still indexed under Bar 3:27");
    }
    check(found, "Bar 4:27 missing from FTS");
    query.text = "ruedas";
    query.limit = 10000;
    for (const auto &result : backend.search(module, query))
        check(result.osisRef.compare(0, 4, "Bar.") != 0, "Ezekiel wheels remain in Baruc");
    BibleKeyInfo last;
    check(backend.resolveKey(module, "Bar 6:72", last), "last verse missing");
    BibleKeyInfo next;
    check(backend.resolveKey(module, backend.navigate(module, last.key, 1), next) &&
          backend.osisRefFromKey(module, next.key) == "Ezek.1.1", "book boundary navigation");
    std::printf("baruc_sqlite_checked=%zu baruc_sqlite_failures=%d\n", checked, failures);
    return failures ? 1 : 0;
}
