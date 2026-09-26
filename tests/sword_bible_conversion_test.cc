#include "backend/sword_bible_conversion.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/sqlite/sqlite_module_writer.h"
#include "backend/sqlite_module_manager.h"
#include <sqlite3.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

static int failures;
static void check(bool ok, const std::string &message) {
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", message.c_str()); }
}
// This category removes only whitespace and SWORD's '*' note markers.
static std::string relaxed(const std::string &s) {
    std::string result;
    for (const char *p = s.c_str(); *p; p = g_utf8_next_char(p))
        if (*p != '*' && !g_unichar_isspace(g_utf8_get_char(p)))
            result.append(p, g_utf8_next_char(p) - p);
    return result;
}
// SWORD's plain output upper-cases the divine name (LORD); SQLite keeps it as
// written and renders it in small capitals.
static std::string divineNameUpper(const BibleVerseContent &c) {
    std::string text = c.plainText;
    for (const auto &span : c.spans) {
        if (span.style != BibleTextStyle::DivineName || span.start + span.length > text.size()) continue;
        gchar *upper = g_utf8_strup(text.c_str() + span.start, span.length);
        if (strlen(upper) == span.length) text.replace(span.start, span.length, upper);
        g_free(upper);
    }
    return text;
}
// SWORD hides Hebrew cantillation by default ("Hebrew Cantillation" off);
// SQLite keeps the text with its accents. The extraordinary points (upper
// and lower dots) are dropped on both sides: SWORD shows WLC's upper dots in
// Ps 27:13 as lower ones.
static std::string withoutCantillation(const std::string &s) {
    std::string result;
    for (const char *p = s.c_str(); *p; p = g_utf8_next_char(p)) {
        const gunichar c = g_utf8_get_char(p);
        if ((c >= 0x0591 && c <= 0x05AF) || c == 0x05C4 || c == 0x05C5) continue;
        result.append(p, g_utf8_next_char(p) - p);
    }
    return result;
}
// SWORD's plain output prints a <w> variant.Qere attribute as "<Gform>"; it is
// not verse text.
static std::string withoutQereAnnotations(const std::string &s) {
    std::string result;
    for (size_t i = 0; i < s.size();) {
        const size_t at = s.find("<G", i);
        const size_t close = at == std::string::npos ? at : s.find('>', at);
        if (close == std::string::npos) { result += s.substr(i); break; }
        result += s.substr(i, at - i);
        i = close + 1;
    }
    return result;
}
// Render parity: per feature, how many verses carry it in SWORD's OSIS and
// how many keep it in the SQLite copy.
struct Features { long jesus = 0, footnotes = 0, xrefs = 0, strongWords = 0, morphWords = 0, titles = 0, paragraphs = 0, italics = 0; };
static void countOsis(xmlNode *node, Features &f, bool &jesus, bool &paragraph) {
    for (; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE) continue;
        const std::string name = reinterpret_cast<const char *>(node->name);
        auto attr = [&](const char *a) {
            xmlChar *v = xmlGetProp(node, reinterpret_cast<const xmlChar *>(a));
            std::string r = v ? reinterpret_cast<const char *>(v) : "";
            xmlFree(v); return r;
        };
        if (name == "q" && attr("who") == "Jesus") jesus = true;
        if (name == "note") { if (attr("type") == "crossReference") ++f.xrefs; else ++f.footnotes; continue; }
        if (name == "w") {
            std::string lemma = attr("lemma");
            for (char &ch : lemma) ch = g_ascii_tolower(ch);
            if (lemma.find("strong:") != std::string::npos) ++f.strongWords;
            if (!attr("morph").empty()) ++f.morphWords;
        }
        if (name == "title") ++f.titles;
        if (name == "hi" && attr("type") == "italic") ++f.italics;
        if (name == "p" || (name == "milestone" && attr("type") == "x-p") || (name == "div" && attr("type") == "paragraph")) paragraph = true;
        countOsis(node->children, f, jesus, paragraph);
    }
}
static void renderParity(const char *id, const char *directory) {
    sword::SWMgr mgr;
    auto *module = mgr.getModule(id);
    SqliteBibleBackend sqlite(directory);
    if (!module || !sqlite.hasModule(id)) return;
    std::unique_ptr<sword::SWKey> owned(module->createKey());
    auto *key = dynamic_cast<sword::VerseKey *>(owned.get());
    key->setIntros(false);
    Features sw, sq;
    for (key->setPosition(sword::TOP); !key->popError(); key->increment()) {
        module->setKey(key);
        std::string raw = module->getRawEntry();
        if (raw.empty() || repairSwordOsisEntry(raw) == SwordEntryRepair::Invalid) continue;
        const std::string doc = "<verse>" + raw + "</verse>";
        xmlDocPtr x = xmlReadMemory(doc.data(), doc.size(), "v", "UTF-8", XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
        if (!x) continue;
        bool jesus = false, paragraph = raw.find("\xc2\xb6") != std::string::npos;
        countOsis(xmlDocGetRootElement(x)->children, sw, jesus, paragraph);
        xmlFreeDoc(x);
        sw.jesus += jesus; sw.paragraphs += paragraph;
        BibleKeyInfo target;
        if (!sqlite.resolveKey(id, std::string(key->getOSISBookName()) + " " + std::to_string(key->getChapter()) + ":" + std::to_string(key->getVerse()), target)) continue;
        const auto c = sqlite.getVerseContent(id, target.reference, true);
        sq.footnotes += c.footnotes.size(); sq.xrefs += c.crossReferences.size();
        sq.titles += c.headings.size(); sq.paragraphs += c.paragraphBreak;
        for (const auto &w : c.words) { sq.strongWords += !w.strongs.empty() || !w.strong.empty(); sq.morphWords += !w.morphologyTags.empty() || !w.morphology.empty(); }
        for (const auto &span : c.spans) if (span.style == BibleTextStyle::WordsOfChrist) { ++sq.jesus; break; }
        for (const auto &span : c.spans) sq.italics += span.style == BibleTextStyle::Italic;
    }
    std::printf("PARITY %s jesus_verses=%ld/%ld footnotes=%ld/%ld xrefs=%ld/%ld strong_words=%ld/%ld morph_words=%ld/%ld titles=%ld/%ld paragraph_verses=%ld/%ld italics=%ld/%ld (sqlite/sword)\n",
                id, sq.jesus, sw.jesus, sq.footnotes, sw.footnotes, sq.xrefs, sw.xrefs, sq.strongWords, sw.strongWords,
                sq.morphWords, sw.morphWords, sq.titles, sw.titles, sq.paragraphs, sw.paragraphs, sq.italics, sw.italics);
}

static void compare(const char *id, const char *directory) {
    sword::SWMgr mgr;
    auto *module = mgr.getModule(id);
    check(module != nullptr, std::string(id) + " installed");
    if (!module) return;
    SqliteBibleBackend sqlite(directory);
    check(sqlite.hasModule(id), std::string(id) + " identical module ID");
    if (!sqlite.hasModule(id)) return;
    const char *lang = module->getConfigEntry("Lang");
    const char *v11n = module->getConfigEntry("Versification");
    check(sqlite.moduleLanguage(id) == (lang ? lang : "und"), "Lang preserved");
    check(sqlite.versification(id) == (v11n ? v11n : "KJV"), "Versification preserved");
    std::unique_ptr<sword::SWKey> owned(module->createKey());
    auto *key = dynamic_cast<sword::VerseKey *>(owned.get());
    check(key != nullptr, "verse key"); if (!key) return;
    key->setIntros(false);
    long repaired = 0, total = 0, identical = 0, spacing = 0, headings = 0, qere = 0, divine = 0, cantillation = 0, other = 0, missing = 0, words = 0;
    for (key->setPosition(sword::TOP); !key->popError(); key->increment()) {
        module->setKey(key);
        if (module->getRawEntryBuf().size() == 0) continue;
        ++total;
        const std::string osis = key->getOSISRef();
        BibleKeyInfo target;
        if (!sqlite.resolveKey(id, std::string(key->getOSISBookName()) + " " + std::to_string(key->getChapter()) + ":" + std::to_string(key->getVerse()), target)) { ++missing; continue; }
        const auto content = sqlite.getVerseContent(id, target.reference, true);
        check(content.valid, osis + " valid");
        std::string raw = module->getRawEntry();
        const bool wasRepaired = repairSwordOsisEntry(raw) == SwordEntryRepair::Repaired;
        if (wasRepaired) ++repaired;
        // A repaired entry is compared with SWORD's own rendering of the repair.
        const std::string swordText = wasRepaired ? module->stripText(raw.c_str()) : module->stripText();
        if (swordText == content.plainText) ++identical;
        else if (relaxed(swordText) == relaxed(content.plainText)) ++spacing;
        else {
            std::string body = relaxed(swordText);
            for (const auto &heading : content.headings) {
                const std::string text = relaxed(heading.text);
                // A quoted title («…») imported as a heading leaves no quotes.
                auto at = body.find("«" + text + "»");
                if (!text.empty() && at != std::string::npos) { body.erase(at, text.size() + 4); continue; }
                at = body.find(text);
                if (!text.empty() && at != std::string::npos) body.erase(at, text.size());
            }
            if (body == relaxed(content.plainText)) ++headings;
            else if (relaxed(withoutQereAnnotations(swordText)) == relaxed(content.plainText)) ++qere;
            else if (relaxed(swordText) == relaxed(divineNameUpper(content))) ++divine;
            else if (relaxed(withoutCantillation(swordText)) == relaxed(withoutCantillation(content.plainText))) ++cantillation;
            else {
                ++other;
                std::printf("OTHER %s %s\nSWORD: %s\nSQLITE: %s\n", id, osis.c_str(), swordText.c_str(), content.plainText.c_str());
            }
        }
        for (const auto &word : content.words) {
            ++words;
            check(word.start <= content.plainText.size() && word.length <= content.plainText.size() - word.start &&
                  content.plainText.substr(word.start, word.length) == word.text, osis + " word range");
        }
        for (const auto &span : content.spans)
            check(span.start <= content.plainText.size() && span.length <= content.plainText.size() - span.start, osis + " span range");
        for (const auto &note : content.footnotes) check(note.offset <= content.plainText.size(), osis + " footnote offset");
        for (const auto &ref : content.crossReferences) check(ref.offset <= content.plainText.size(), osis + " crossref offset");
    }
    std::printf("COMPARE %s repaired=%ld total=%ld identical=%ld note_or_space=%ld title_to_heading=%ld qere_annotation=%ld divine_name_case=%ld hebrew_cantillation=%ld other=%ld missing=%ld words=%ld\n",
                id,repaired,total,identical,spacing,headings,qere,divine,cantillation,other,missing,words);
    check(missing == 0, std::string(id) + " no missing verses");
    check(other == 0, std::string(id) + " no unexplained differences");
    UsfmImportStats stats;
    std::string error;
    check(!convertSwordBible(id, directory, stats, error) &&
          error.find("already installed") != std::string::npos,
          std::string(id) + " duplicate conversion does not replace module");
}
int main(int argc, char **argv) {
    gchar *tmp = g_dir_make_tmp("sword-conversion-test-XXXXXX", nullptr);
    UsfmImportStats stats;
    std::string error;
    check(!convertSwordBible("__missing_bible__", tmp, stats, error), "missing module rejected");
    GDir *dir = g_dir_open(tmp, 0, nullptr);
    check(dir && !g_dir_read_name(dir), "failed conversion leaves directory empty");
    if (dir) g_dir_close(dir);
    g_rmdir(tmp); g_free(tmp);

    // Damaged SWORD entries: only a truncated tail is repaired.
    struct RepairCase { std::string raw, expected; SwordEntryRepair result; const char *name; };
    const RepairCase repairs[] = {
        {"<q who=\"Jesus\">Ven.</q> dijo", "<q who=\"Jesus\">Ven.</q> dijo", SwordEntryRepair::Intact, "intact entry untouched"},
        {"a &gt; b\n", "a &gt; b\n", SwordEntryRepair::Intact, "intact trailing space untouched"},
        // SpaRVG Matt 9:6: "</q" lost, only its '>' left.
        {"<q who=\"Jesus\">A</q> (dijo): <q who=\"Jesus\">vete a tu casa.>\n",
         "<q who=\"Jesus\">A</q> (dijo): <q who=\"Jesus\">vete a tu casa.</q>", SwordEntryRepair::Repaired, "lost end tag"},
        // Tisch John 8:53: cut inside a start tag and inside a UTF-8 sequence.
        {"<w lemma=\"strong:G3825\">\xcf\x80\xce\xac\xce\xbb\xce\xb9\xce\xbd</w> <w lemma=\"strong:G846 lemma.Strong:\xce\xb1\xcf\n",
         "<w lemma=\"strong:G3825\">\xcf\x80\xce\xac\xce\xbb\xce\xb9\xce\xbd</w>", SwordEntryRepair::Repaired, "truncated tag and UTF-8"},
        {"<q>a > b", "<q>a > b</q>", SwordEntryRepair::Repaired, "text '>' kept when not final"},
        {"a\xcf b", "a\xcf b", SwordEntryRepair::Invalid, "invalid UTF-8 inside the entry"},
        {"<q>a</w>", "<q>a</w>", SwordEntryRepair::Invalid, "mismatched end tag"},
        {"a & b", "a & b", SwordEntryRepair::Invalid, "bare ampersand"},
    };
    for (const auto &c : repairs) {
        std::string entry = c.raw;
        const auto result = repairSwordOsisEntry(entry);
        check(result == c.result && entry == c.expected, std::string("repair: ") + c.name + " -> " + entry);
    }

    // Automatic conversion: every candidate is pending until converted or failed.
    tmp = g_dir_make_tmp("sword-conversion-pending-XXXXXX", nullptr);
    const std::vector<SwordBibleConversionInfo> candidates = {
        {"AlphaBible", "Alpha", "es", "KJV", "1.0"}, {"BetaBible", "Beta", "es", "Vulg", "1.0"}};
    auto pending = pendingSwordBibleConversions(candidates, tmp);
    check(pending.size() == 2 && pending[0].id == "AlphaBible" && pending[1].id == "BetaBible",
          "unconverted SWORD Bibles are pending");
    recordFailedSwordConversion("BetaBible", tmp);
    recordFailedSwordConversion("BetaBible", tmp);
    pending = pendingSwordBibleConversions(candidates, tmp);
    check(pending.size() == 1 && pending[0].id == "AlphaBible",
          "failed conversion is not retried automatically");
    const std::string failedFile = std::string(tmp) + "/.sword-conversion-failed";
    gchar *contents = nullptr;
    check(g_file_get_contents(failedFile.c_str(), &contents, nullptr, nullptr) &&
          std::string(contents) == "BetaBible\t4\n", "failed conversion recorded once");
    g_free(contents);
    // A failure recorded by an older converter revision is retried.
    check(g_file_set_contents(failedFile.c_str(), "AlphaBible\nBetaBible\t1\n", -1, nullptr) &&
          pendingSwordBibleConversions(candidates, tmp).size() == 2,
          "failures from older converter revisions are retried");
    check(SqliteBibleBackend(tmp).listModules().empty(), "failure record is not a module");
    g_remove(failedFile.c_str()); g_rmdir(tmp); g_free(tmp);

    // Copies follow their SWORD module: a copy converted from another version
    // is pending again, and uninstalling the SWORD Bible removes it. Modules
    // that did not come from SWORD are never touched.
    tmp = g_dir_make_tmp("sword-conversion-sync-XXXXXX", nullptr);
    {
        auto write = [&](const std::string &id, const std::string &source, const std::string &version) {
            SqliteModuleMetadata m;
            m.moduleId = id; m.name = id; m.language = "es"; m.versification = "kjv";
            m.source = source; m.contentVersion = version; m.sourceFormat = "osis";
            SqliteImportVerse v; v.reference = {2, 43, 3, 16}; v.text = "Porque de tal manera";
            std::string e;
            const std::string path = std::string(tmp) + "/" + id + ".sqlite";
            check(SqliteModuleWriter().write(m, {{43, 2, 1, "John", "Juan", "Jn"}}, {v}, path, e), "write " + id + " " + e);
            return path;
        };
        auto stamp = [](const std::string &path, const char *revision) {
            sqlite3 *db = nullptr;
            sqlite3_open(path.c_str(), &db);
            sqlite3_exec(db, (std::string("INSERT OR REPLACE INTO metadata VALUES('conversion_revision','") + revision + "')").c_str(),
                         nullptr, nullptr, nullptr);
            sqlite3_close(db);
        };
        const std::string alpha = write("AlphaBible", "SWORD:AlphaBible", "1.0");
        write("BetaBible", "usfm:import", "0.1");
        auto p = pendingSwordBibleConversions(candidates, tmp);
        check(p.size() == 1 && p[0].id == "AlphaBible", "copy from an older converter is pending again");
        stamp(alpha, "4");
        p = pendingSwordBibleConversions(candidates, tmp);
        check(p.empty(), "current converted copy and foreign module are not pending");
        auto updated = candidates;
        updated[0].version = "2.0";
        updated[1].version = "2.0";
        p = pendingSwordBibleConversions(updated, tmp);
        check(p.size() == 1 && p[0].id == "AlphaBible", "copy of an older SWORD version is pending again");
        std::string e;
        check(removeConvertedSwordBible("BetaBible", tmp, e) && e.empty(), "foreign module: nothing to remove");
        check(SqliteBibleBackend(tmp).hasModule("BetaBible"), "foreign module kept");
        check(removeConvertedSwordBible("AlphaBible", tmp, e), "converted copy removed");
        check(!SqliteBibleBackend(tmp).hasModule("AlphaBible"), "converted copy is gone");
        check(removeConvertedSwordBible("AlphaBible", tmp, e), "removing twice is harmless");
        g_remove((std::string(tmp) + "/BetaBible.sqlite").c_str());
    }
    g_rmdir(tmp); g_free(tmp);

    if (argc >= 2) {
        // BIBLIA_ELIM_CONVERSION_IDS="A B C" overrides the default set.
        std::vector<std::string> ids = {"SpaPlatense", "TorresAmat", "NacarColunga", "SpaRV1909", "SpaRVG", "Tisch"};
        if (const char *list = g_getenv("BIBLIA_ELIM_CONVERSION_IDS")) {
            ids.clear();
            gchar **parts = g_strsplit(list, " ", -1);
            for (gchar **part = parts; *part; ++part) if (**part) ids.push_back(*part);
            g_strfreev(parts);
        }
        for (const std::string &name : ids) {
            const char *id = name.c_str();
            if (argc < 3) {
                std::vector<std::string> repaired;
                bool ok = convertSwordBible(id, argv[1], stats, error, &repaired);
                std::string references;
                for (const auto &reference : repaired) references += " " + reference;
                std::printf("%s converted=%d books=%zu verses=%zu headings=%zu strong=%zu repaired=%zu%s error=%s\n",
                            id, ok, stats.books, stats.verses, stats.headingsImported, stats.wordsWithStrong,
                            repaired.size(), references.c_str(), error.c_str());
                check(ok, std::string(id) + " conversion");
                if (!ok) continue;
            }
            compare(id, argv[1]);
            renderParity(id, argv[1]);
        }
    }
    // Real module: a copy converted from another SWORD version is replaced in
    // place (atomic rename), keeping its ID.
    if (argc >= 2 && argc < 3) {
        gchar *dir = g_dir_make_tmp("sword-conversion-replace-XXXXXX", nullptr);
        const std::string path = std::string(dir) + "/TorresAmat.sqlite";
        check(convertSwordBible("TorresAmat", dir, stats, error), "replace: first conversion " + error);
        sqlite3 *db = nullptr;
        sqlite3_open(path.c_str(), &db);
        sqlite3_exec(db, "UPDATE metadata SET value='0-old' WHERE key='content_version'", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        check(pendingSwordBibleConversions(convertibleSwordBibles(), dir).size() >= 1, "replace: outdated copy pending");
        check(convertSwordBible("TorresAmat", dir, stats, error), "replace: reconversion " + error);
        sword::SWMgr mgr;
        const char *version = mgr.getModule("TorresAmat")->getConfigEntry("Version");
        check(sqliteModuleMetadata(path, "content_version") == (version ? version : ""), "replace: version refreshed");
        check(SqliteBibleBackend(dir).listModules().size() == 1, "replace: one module, no temporaries listed");
        // The module and the backend's validation cache; no temporaries.
        GDir *d = g_dir_open(dir, 0, nullptr); int modules = 0, temporaries = 0;
        while (const char *n = g_dir_read_name(d)) {
            const std::string name(n);
            if (name.find(".tmp") != std::string::npos) ++temporaries;
            else if (name.size() > 7 && name.compare(name.size() - 7, 7, ".sqlite") == 0) ++modules;
        }
        g_dir_close(d);
        check(modules == 1 && temporaries == 0, "replace: no temporary files left");
        std::string e;
        check(removeConvertedSwordBible("TorresAmat", dir, e) && !g_file_test(path.c_str(), G_FILE_TEST_EXISTS), "replace: cleanup");
        g_remove((std::string(dir) + "/.validation-cache").c_str());
        g_rmdir(dir); g_free(dir);
    }
    std::printf("sword_conversion_failures=%d\n", failures);
    return failures ? 1 : 0;
}
