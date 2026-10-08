#include "backend/sqlite_module_manager.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <cassert>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    gchar *dir = g_dir_make_tmp("biblia-elim-manager-test-XXXXXX", nullptr);
    assert(dir);
    std::string error;
    // Generate our own source, independent of a developer's /tmp artifacts.
    const std::string source = std::string(dir) + "/source.sqlite";
    UsfmImportOptions sourceOptions;
    sourceOptions.moduleId = "rv1909";
    sourceOptions.name = "RV1909 fixture";
    sourceOptions.language = "es";
    sourceOptions.versification = "kjv";
    UsfmImportStats sourceStats;
    assert(importUsfm({std::string(SRCDIR) + "/tests/fixtures/sqlite/GEN.usfm"},
                      source, sourceOptions, sourceStats, error));
    SqliteManagedModule module;
    assert(validateSqliteModuleFile(source, module, error));
    assert(module.info.id == "rv1909");
    const std::string managed = std::string(dir) + "/managed";
    assert(installSqliteModule(source, error, &module, managed));
    assert(!installSqliteModule(source, error, nullptr, managed));
    assert(listSqliteModules(managed).size() == 1);
    assert(removeSqliteModule("rv1909", error, managed));
    assert(listSqliteModules(managed).empty());
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
    assert(importUsfmModule(inputs, options, stats, error, managed));
    assert(listSqliteModules(managed).size() == 1);
    assert(removeSqliteModule("fixture", error, managed));
    const std::string legacy = std::string(dir) + "/biblia_elim/modules";
    const std::string canonical = std::string(dir) + "/biblia-elim/modules";
    assert(migrateLegacySqliteModules(error, dir)); // absent legacy directory
    assert(installSqliteModule(source, error, nullptr, legacy));
    assert(migrateLegacySqliteModules(error, dir));
    assert(listSqliteModules(canonical).size() == 1);
    assert(!g_file_test(legacy.c_str(), G_FILE_TEST_EXISTS));
    assert(migrateLegacySqliteModules(error, dir)); // idempotent
    assert(installSqliteModule(source, error, nullptr, legacy));
    assert(!migrateLegacySqliteModules(error, dir)); // collision: preserve both
    assert(error.find("already installed") != std::string::npos);
    assert(listSqliteModules(legacy).size() == 1);
    assert(listSqliteModules(canonical).size() == 1);
    assert(removeSqliteModule("rv1909", error, legacy));
    assert(removeSqliteModule("rv1909", error, canonical));
    { std::ofstream out(legacy + "/broken.sqlite"); out << "invalid"; }
    assert(!migrateLegacySqliteModules(error, dir));
    assert(g_file_test((legacy + "/broken.sqlite").c_str(), G_FILE_TEST_EXISTS));
    assert(listSqliteModules(canonical).empty());
    g_remove((legacy + "/broken.sqlite").c_str());
    g_rmdir(legacy.c_str()); g_rmdir(canonical.c_str());
    g_rmdir((std::string(dir) + "/biblia_elim").c_str());
    g_rmdir((std::string(dir) + "/biblia-elim").c_str());
    {
        // Biblias del paquete: se siembran una vez, sin reponer lo borrado ni pisar copias.
        const std::string share = std::string(dir) + "/share";
        const std::string seeded = std::string(dir) + "/seeded";
        assert(g_mkdir_with_parents(share.c_str(), 0755) == 0);
        { std::ifstream in(source, std::ios::binary); std::ofstream out(share + "/rv1909.sqlite", std::ios::binary); out << in.rdbuf(); }
        assert(seedBundledSqliteModules(std::string(dir) + "/no-such-dir", error, seeded)); // sin paquete
        assert(seedBundledSqliteModules(share, error, seeded));
        assert(listSqliteModules(seeded).size() == 1);
        assert(removeSqliteModule("rv1909", error, seeded));
        assert(seedBundledSqliteModules(share, error, seeded)); // ya sembrada: no vuelve
        assert(listSqliteModules(seeded).empty());
        g_remove((seeded + "/.bundled-seeded").c_str());
        { std::ofstream mine(seeded + "/rv1909.sqlite"); mine << "usuario"; }
        assert(seedBundledSqliteModules(share, error, seeded)); // copia existente intacta
        std::ifstream check(seeded + "/rv1909.sqlite"); std::string content; check >> content;
        assert(content == "usuario");
        g_remove((seeded + "/rv1909.sqlite").c_str());
        g_remove((seeded + "/.bundled-seeded").c_str());
        g_rmdir(seeded.c_str());
        g_remove((share + "/rv1909.sqlite").c_str());
        g_rmdir(share.c_str());
    }
    g_rmdir(managed.c_str());
    g_remove(source.c_str());
    g_print("sqlite_module_manager migration/install/remove: PASS\n");
    g_remove(invalid.c_str());
    g_rmdir(dir); g_free(dir);
    return 0;
}
