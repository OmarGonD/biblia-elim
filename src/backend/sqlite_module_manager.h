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
std::vector<SqliteManagedModule> listSqliteModules(const std::string &directory = {});
bool validateSqliteModuleFile(const std::string &file, SqliteManagedModule &module,
                              std::string &error);
bool installSqliteModule(const std::string &file, std::string &error,
                         SqliteManagedModule *installed = nullptr,
                         const std::string &directory = {});
bool importUsfmModule(const std::vector<std::string> &inputs,
                      const UsfmImportOptions &options, UsfmImportStats &stats,
                      std::string &error, const std::string &directory = {});
bool removeSqliteModule(const std::string &moduleId, std::string &error,
                        const std::string &directory = {});

#endif
