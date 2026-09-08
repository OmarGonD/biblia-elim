#ifndef XIPHOS_OSIS_IMPORTER_H
#define XIPHOS_OSIS_IMPORTER_H
#include <string>
#include "backend/usfm_importer.h"
bool importOsis(const std::string &input, const std::string &output,
                const UsfmImportOptions &options, UsfmImportStats &stats,
                std::string &error);
#endif
