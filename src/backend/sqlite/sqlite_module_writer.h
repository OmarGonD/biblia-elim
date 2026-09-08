#ifndef XIPHOS_SQLITE_MODULE_WRITER_H
#define XIPHOS_SQLITE_MODULE_WRITER_H

#include <map>
#include <string>
#include <vector>

#include "backend/bible_types.h"

struct SqliteModuleMetadata {
    std::string moduleId, name, language, versification;
    std::string abbreviation, description, license, publisher, source, contentVersion;
    std::string sourceFormat = "usfm";
};

struct SqliteImportBook {
    int id = 0, testament = 0, position = 0;
    std::string osis, name, shortName;
};

struct SqliteImportVerse {
    BibleReference reference;
    std::string text;
    bool paragraphBreak = false;
    std::vector<BibleHeading> headings;
    std::vector<BibleTextSpan> spans;
    std::vector<BibleWordInfo> words;
    std::vector<BibleFootnote> footnotes;
    std::vector<BibleCrossReference> crossReferences;
};

class SqliteModuleWriter {
public:
    bool write(const SqliteModuleMetadata &metadata,
               const std::vector<SqliteImportBook> &books,
               const std::vector<SqliteImportVerse> &verses,
               const std::string &output, std::string &error) const;
};

#endif
