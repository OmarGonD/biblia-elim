#include "backend/sword_bible_conversion.h"
#include "backend/osis_importer.h"
#include "backend/sqlite_module_manager.h"
#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <sqlite3.h>
#include <algorithm>
#include <fstream>
#include <memory>

namespace {
std::string config(sword::SWModule &m, const char *key, const char *fallback = "") {
    const char *value = m.getConfigEntry(key);
    return value && *value ? value : fallback;
}
bool supported(sword::SWModule &m) {
    return std::string(m.getType()) == "Biblical Texts" &&
        config(m, "SourceType") == "OSIS" &&
        (config(m, "CipherKey").size() || !m.getConfigEntry("CipherKey"));
}
SwordBibleConversionInfo info(sword::SWModule &m) {
    return {m.getName(), m.getDescription(), config(m, "Lang", "und"),
            config(m, "Versification", "KJV"), config(m, "Version")};
}
std::string escaped(const std::string &s) {
    gchar *text = g_markup_escape_text(s.c_str(), s.size());
    std::string result(text); g_free(text); return result;
}
// Bumped when the converter improves (handles more damaged modules, keeps
// more of the text's markup). A failure recorded by an older revision is
// retried, and a copy converted by one is converted again, once,
// automatically. Stored in the copy's metadata as "conversion_revision".
// 3: words of Christ, added words, divine name, paragraphs, variants.
// 4: the edition's italics and bold (<hi type="italic|bold">).
const char *const kConverterRevision = "4";
// The installed SQLite copy converted from SWORD Bible `id`, if any.
const SqliteManagedModule *convertedCopy(const std::vector<SqliteManagedModule> &installed,
                                         const std::string &id) {
    for (const auto &m : installed)
        if (m.info.id == id)
            return sqliteModuleMetadata(m.path, "source") == "SWORD:" + id ? &m : nullptr;
    return nullptr;
}
// A converted copy made from another SWORD version or by an older converter.
bool outdated(const SqliteManagedModule &copy, const std::string &swordVersion) {
    return sqliteModuleMetadata(copy.path, "content_version") != swordVersion ||
        sqliteModuleMetadata(copy.path, "conversion_revision") != kConverterRevision;
}
bool installedWithId(const std::vector<SqliteManagedModule> &installed, const std::string &id) {
    for (const auto &m : installed) if (m.info.id == id) return true;
    return false;
}
std::string failedConversionsFile(const std::string &directory) {
    return directory + "/.sword-conversion-failed";
}
std::vector<std::string> failedConversions(const std::string &directory) {
    std::vector<std::string> ids;
    std::ifstream in(failedConversionsFile(directory));
    const std::string suffix = std::string("\t") + kConverterRevision;
    for (std::string line; std::getline(in, line);)
        if (line.size() > suffix.size() &&
            !line.compare(line.size() - suffix.size(), suffix.size(), suffix))
            ids.push_back(line.substr(0, line.size() - suffix.size()));
    return ids;
}
bool wellFormed(const std::string &entry) {
    const std::string document = "<verse>" + entry + "</verse>";
    xmlDocPtr doc = xmlReadMemory(document.data(), static_cast<int>(document.size()), "entry.xml",
                                  "UTF-8", XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) return false;
    xmlFreeDoc(doc);
    return true;
}
bool asciiSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
// Drops an incomplete multibyte sequence at the very end; false when the
// invalid bytes are anything else.
bool dropTruncatedUtf8(std::string &s) {
    const gchar *end = nullptr;
    if (g_utf8_validate(s.data(), s.size(), &end)) return true;
    const size_t at = end - s.data(), rest = s.size() - at;
    const unsigned char lead = s[at];
    const size_t needed = lead >= 0xF0 && lead <= 0xF4 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC2 ? 2 : 0;
    if (!needed || lead > 0xF4 || rest >= needed) return false;
    for (size_t i = at + 1; i < s.size(); ++i)
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) return false;
    s.resize(at);
    return true;
}
struct TagScan {
    std::vector<std::string> open;
    size_t unterminated = std::string::npos; // '<' of a final tag without '>'
    size_t lastTextGreater = std::string::npos;
    bool mismatched = false;
};
TagScan scanTags(const std::string &s) {
    TagScan scan;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '>') { scan.lastTextGreater = i; continue; }
        if (s[i] != '<') continue;
        if (!s.compare(i, 4, "<!--")) {
            const size_t close = s.find("-->", i + 4);
            if (close == std::string::npos) { scan.unterminated = i; break; }
            i = close + 2; continue;
        }
        size_t j = i + 1; char quote = 0;
        for (; j < s.size(); ++j) {
            if (quote) { if (s[j] == quote) quote = 0; }
            else if (s[j] == '"' || s[j] == '\'') quote = s[j];
            else if (s[j] == '>') break;
        }
        if (j >= s.size()) { scan.unterminated = i; break; }
        const std::string tag = s.substr(i + 1, j - i - 1);
        i = j;
        if (tag.empty() || tag[0] == '?' || tag[0] == '!' || tag.back() == '/') continue;
        const bool closing = tag[0] == '/';
        const size_t nameStart = closing ? 1 : 0;
        size_t nameEnd = nameStart;
        while (nameEnd < tag.size() && !asciiSpace(tag[nameEnd])) ++nameEnd;
        const std::string name = tag.substr(nameStart, nameEnd - nameStart);
        if (!closing) scan.open.push_back(name);
        else if (!scan.open.empty() && scan.open.back() == name) scan.open.pop_back();
        else { scan.mismatched = true; break; }
    }
    return scan;
}
struct Temporary {
    gchar *directory = g_dir_make_tmp("biblia-sword-convert-XXXXXX", nullptr);
    ~Temporary() {
        if (!directory) return;
        g_remove((std::string(directory) + "/source.xml").c_str());
        g_remove((std::string(directory) + "/module.sqlite").c_str());
        g_rmdir(directory); g_free(directory);
    }
};
}

SwordEntryRepair repairSwordOsisEntry(std::string &entry) {
    if (g_utf8_validate(entry.data(), entry.size(), nullptr) && wellFormed(entry))
        return SwordEntryRepair::Intact;
    std::string s = entry;
    while (!s.empty() && asciiSpace(s.back())) s.pop_back();
    if (!dropTruncatedUtf8(s)) return SwordEntryRepair::Invalid;
    TagScan scan = scanTags(s);
    if (scan.unterminated != std::string::npos) {
        s.resize(scan.unterminated);
        while (!s.empty() && asciiSpace(s.back())) s.pop_back();
        scan = scanTags(s);
    }
    if (scan.mismatched || scan.unterminated != std::string::npos) return SwordEntryRepair::Invalid;
    if (!scan.open.empty() && scan.lastTextGreater + 1 == s.size())
        s.pop_back(); // what is left of the innermost lost end tag
    for (auto name = scan.open.rbegin(); name != scan.open.rend(); ++name)
        s += "</" + *name + ">";
    if (!wellFormed(s)) return SwordEntryRepair::Invalid;
    entry = s;
    return SwordEntryRepair::Repaired;
}

std::vector<SwordBibleConversionInfo> convertibleSwordBibles() {
    sword::SWMgr mgr;
    std::vector<SwordBibleConversionInfo> result;
    for (const auto &entry : mgr.Modules)
        if (supported(*entry.second)) result.push_back(info(*entry.second));
    return result;
}

bool convertSwordBible(const std::string &id, const std::string &directory,
                       UsfmImportStats &stats, std::string &error,
                       std::vector<std::string> *repairedEntries) {
    stats = {};
    error.clear();
    if (repairedEntries) repairedEntries->clear();
    sword::SWMgr mgr;
    sword::SWModule *module = mgr.getModule(id.c_str());
    const auto installed = listSqliteModules(directory);
    const SqliteManagedModule *previous = convertedCopy(installed, id);
    const bool replace = module && previous && outdated(*previous, config(*module, "Version"));
    if (installedWithId(installed, id) && !replace) {
        error = "module_id already installed: " + id;
        return false;
    }
    if (!module || !supported(*module)) {
        error = "La conversión requiere una Biblia SWORD OSIS instalada y desbloqueada: " + id;
        return false;
    }
    const auto metadata = info(*module);
    Temporary temp;
    if (!temp.directory) { error = "Cannot create conversion directory"; return false; }
    const std::string source = std::string(temp.directory) + "/source.xml";
    const std::string output = std::string(temp.directory) + "/module.sqlite";
    std::ofstream xml(source);
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><osis><osisText osisIDWork=\""
        << escaped(id) << "\" xml:lang=\"" << escaped(metadata.language) << "\">";
    std::unique_ptr<sword::SWKey> owned(module->createKey());
    auto *key = dynamic_cast<sword::VerseKey *>(owned.get());
    if (!key) { error = "SWORD Bible has no verse key"; return false; }
    key->setIntros(false);
    std::string book;
    int chapter = 0;
    // Walk every native versification slot, including linked verse entries.
    for (key->setPosition(sword::TOP); !key->popError(); key->increment()) {
        module->setKey(key);
        std::string raw = module->getRawEntry();
        if (raw.empty()) continue;
        switch (repairSwordOsisEntry(raw)) {
        case SwordEntryRepair::Intact: break;
        case SwordEntryRepair::Repaired:
            if (repairedEntries) repairedEntries->push_back(key->getOSISRef());
            break;
        case SwordEntryRepair::Invalid:
            error = "Damaged SWORD entry (invalid UTF-8 or OSIS): " + std::string(key->getOSISRef());
            return false;
        }
        if (book != key->getOSISBookName()) {
            if (chapter) { xml << "</chapter>"; chapter = 0; }
            if (!book.empty()) xml << "</div>";
            book = key->getOSISBookName();
            xml << "<div type=\"book\" osisID=\"" << escaped(book) << "\">";
        }
        if (chapter != key->getChapter()) {
            if (chapter) xml << "</chapter>";
            chapter = key->getChapter();
            xml << "<chapter osisID=\"" << escaped(book) << "." << chapter << "\">";
        }
        xml << "<verse osisID=\"" << escaped(key->getOSISRef()) << "\">"
            << raw << "</verse>\n";
    }
    if (chapter) xml << "</chapter>";
    if (!book.empty()) xml << "</div>";
    xml << "</osisText></osis>";
    xml.close();
    if (!xml) { error = "Cannot write OSIS conversion input"; return false; }
    UsfmImportOptions options;
    options.moduleId = metadata.id;
    options.name = metadata.name;
    options.language = metadata.language;
    options.versification = metadata.versification;
    // The writer accepts canonical lower-case names; identity stays untouched.
    for (char &c : options.versification) c = g_ascii_tolower(c);
    options.description = metadata.name;
    options.source = "SWORD:" + metadata.id;
    options.license = config(*module, "DistributionLicense");
    options.contentVersion = config(*module, "Version");
    if (!importOsis(source, output, options, stats, error)) return false;
    {
        sqlite3 *db = nullptr;
        bool stamped = sqlite3_open(output.c_str(), &db) == SQLITE_OK &&
            sqlite3_exec(db, (std::string("INSERT OR REPLACE INTO metadata(key,value) VALUES('conversion_revision','") +
                              kConverterRevision + "')").c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
        sqlite3_close(db);
        if (!stamped) { error = "Cannot record the conversion revision"; return false; }
    }
    return (replace ? replaceSqliteModule(output, error, directory)
                 : installSqliteModule(output, error, nullptr, directory));
}

std::vector<SwordBibleConversionInfo> pendingSwordBibleConversions(
    const std::vector<SwordBibleConversionInfo> &candidates, const std::string &directory) {
    const auto installed = listSqliteModules(directory);
    const auto failed = failedConversions(directory);
    std::vector<SwordBibleConversionInfo> result;
    for (const auto &candidate : candidates) {
        const SqliteManagedModule *copy = convertedCopy(installed, candidate.id);
        const bool current = installedWithId(installed, candidate.id) &&
            (!copy || !outdated(*copy, candidate.version));
        const bool skipped = std::find(failed.begin(), failed.end(), candidate.id) != failed.end();
        if (!current && !skipped) result.push_back(candidate);
    }
    return result;
}

void recordFailedSwordConversion(const std::string &id, const std::string &directory) {
    if (id.empty() || id.find_first_of("\t\n") != std::string::npos) return;
    const auto failed = failedConversions(directory);
    if (std::find(failed.begin(), failed.end(), id) != failed.end()) return;
    if (g_mkdir_with_parents(directory.c_str(), 0755) != 0) return;
    std::ofstream out(failedConversionsFile(directory), std::ios::app);
    out << id << '\t' << kConverterRevision << '\n';
}

bool removeConvertedSwordBible(const std::string &id, const std::string &directory,
                               std::string &error) {
    error.clear();
    if (!convertedCopy(listSqliteModules(directory), id)) return true;
    return removeSqliteModule(id, error, directory);
}
