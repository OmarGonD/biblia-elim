#include "backend/sqlite_module_manager.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <cassert>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const std::string source = "/tmp/rv1909-strong-enabled/rv1909.sqlite";
    gchar *dir = g_dir_make_tmp("biblia-elim-manager-test-XXXXXX", nullptr);
    assert(dir);
    std::string error;
    SqliteManagedModule module;
    assert(validateSqliteModuleFile(source, module, error));
    assert(module.info.id == "rv1909");
    assert(installSqliteModule(source, error, &module, dir));
    assert(!installSqliteModule(source, error, nullptr, dir));
    assert(listSqliteModules(dir).size() == 1);
    assert(removeSqliteModule("rv1909", error, dir));
    assert(listSqliteModules(dir).empty());
    std::string invalid = std::string(dir) + "/invalid.sqlite";
    { std::ofstream out(invalid); out << "not sqlite"; }
    assert(!validateSqliteModuleFile(invalid, module, error));

    UsfmImportOptions options;
    options.moduleId = "fixture";
    options.name = "Fixture";
    options.language = "es";
    options.versification = "custom";
    UsfmImportStats stats;
    std::vector<std::string> inputs = {std::string(SRCDIR) + "/tests/fixtures/sqlite/GEN.usfm",
                                       std::string(SRCDIR) + "/tests/fixtures/sqlite/JHN.usfm"};
    assert(importUsfmModule(inputs, options, stats, error, dir));
    assert(listSqliteModules(dir).size() == 1);
    assert(removeSqliteModule("fixture", error, dir));
    g_remove(invalid.c_str());
    g_rmdir(dir); g_free(dir);
    return 0;
}
