#ifndef XIPHOS_SQLITE_MODULE_MANAGER_H
#define XIPHOS_SQLITE_MODULE_MANAGER_H

#include <string>
#include <vector>
#include "backend/bible_types.h"
#include "backend/usfm_importer.h"

struct SqliteManagedModule {
    BibleModuleInfo info;
    BibleModuleCapabilities capabilities;
    std::string name;
    std::string path;
};

std::string sqliteModuleDirectory();
// Migrate valid legacy modules without replacing any installed module.
bool migrateLegacySqliteModules(std::string &error,
                               const std::string &dataDirectory = {});
std::vector<SqliteManagedModule> listSqliteModules(const std::string &directory = {});
bool validateSqliteModuleFile(const std::string &file, SqliteManagedModule &module,
                              std::string &error);
bool installSqliteModule(const std::string &file, std::string &error,
                         SqliteManagedModule *installed = nullptr,
                         const std::string &directory = {});
// Replaces an installed module with the same ID atomically (rename over it).
bool replaceSqliteModule(const std::string &file, std::string &error,
                         const std::string &directory = {});
// A value of a module's metadata table ("source", "content_version", ...);
// empty when absent.
std::string sqliteModuleMetadata(const std::string &path, const std::string &key);
bool importUsfmModule(const std::vector<std::string> &inputs,
                      const UsfmImportOptions &options, UsfmImportStats &stats,
                      std::string &error, const std::string &directory = {});
bool removeSqliteModule(const std::string &moduleId, std::string &error,
                        const std::string &directory = {});

#endif
