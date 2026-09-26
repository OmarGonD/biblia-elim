#ifndef BIBLIA_SWORD_BIBLE_CONVERSION_H
#define BIBLIA_SWORD_BIBLE_CONVERSION_H
#include "backend/usfm_importer.h"
#include <string>
#include <vector>

struct SwordBibleConversionInfo {
    std::string id, name, language, versification;
    std::string version; // the .conf Version, stored as content_version
};
std::vector<SwordBibleConversionInfo> convertibleSwordBibles();
// Uses an independent SWORD manager; never changes the reader's current key/options.
// OSIS -> neutral import model -> SqliteModuleWriter -> atomic installation.
// `repairedEntries`, when given, receives the OSIS references of entries that
// needed repairSwordOsisEntry(). An SQLite module with the same ID is replaced
// only when it was converted from this SWORD module ("SWORD:<id>") at another
// version; any other existing module is left alone ("already installed").
bool convertSwordBible(const std::string &id, const std::string &directory,
                       UsfmImportStats &stats, std::string &error,
                       std::vector<std::string> *repairedEntries = nullptr);

// Removes the SQLite copy converted from SWORD Bible `id`, if there is one;
// modules installed or imported by other means are never touched.
bool removeConvertedSwordBible(const std::string &id, const std::string &directory,
                               std::string &error);

enum class SwordEntryRepair { Intact, Repaired, Invalid };
// A raw OSIS entry that is not valid UTF-8 or not well-formed is repaired only
// when the damage is a truncated tail: an incomplete final UTF-8 sequence, an
// unterminated final tag, and elements left open (a stray final '>' in text
// is the remnant of the lost end tag). Visible text is never invented; any
// other damage is Invalid. Intact entries are left byte-identical.
SwordEntryRepair repairSwordOsisEntry(std::string &entry);
// Convertible SWORD Bibles with no SQLite copy in `directory`, or whose copy
// was converted from an older version of the SWORD module, and no recorded
// failed conversion: the ones to convert automatically after installation.
std::vector<SwordBibleConversionInfo> pendingSwordBibleConversions(
    const std::vector<SwordBibleConversionInfo> &candidates, const std::string &directory);
// Remembers a failed automatic conversion so it is not retried at every
// startup; the SWORD original keeps serving that Bible.
void recordFailedSwordConversion(const std::string &id, const std::string &directory);
#endif
