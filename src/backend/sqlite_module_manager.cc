#include "backend/sqlite_module_manager.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <cstdio>
#include <fstream>
#include <unistd.h>

namespace {
std::string dirOrDefault(const std::string &d) {
    if (!d.empty()) return d;
    return g_build_filename(g_get_user_data_dir(), "biblia-elim", "modules", nullptr);
}
bool copyAtomic(const std::string &source, const std::string &dest, std::string &error) {
    std::string tmp = dest + ".tmp-XXXXXX";
    gchar *mutablePath = g_strdup(tmp.c_str());
    int fd = g_mkstemp(mutablePath);
    if (fd < 0) { error = "cannot create temporary module"; g_free(mutablePath); return false; }
    close(fd); g_unlink(mutablePath);
    GFile *src = g_file_new_for_path(source.c_str());
    GFile *dst = g_file_new_for_path(mutablePath);
    GError *ge = nullptr;
    bool ok = g_file_copy(src, dst, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &ge);
    g_object_unref(src); g_object_unref(dst);
    if (!ok) { error = ge ? ge->message : "copy failed"; if (ge) g_error_free(ge); g_unlink(mutablePath); g_free(mutablePath); return false; }
    if (g_rename(mutablePath, dest.c_str()) != 0) { error = "cannot atomically install module"; g_unlink(mutablePath); g_free(mutablePath); return false; }
    g_free(mutablePath); return true;
}
}

std::string sqliteModuleDirectory() { return dirOrDefault({}); }

std::vector<SqliteManagedModule> listSqliteModules(const std::string &directory) {
    std::vector<SqliteManagedModule> result;
    const std::string dir = dirOrDefault(directory);
    SqliteBibleBackend backend(dir);
    for (const BibleModuleInfo &info : backend.listModules()) {
        SqliteManagedModule item;
        item.info = info; item.capabilities = backend.moduleCapabilities(info.id);
        item.name = info.description;
        item.path = g_build_filename(dir.c_str(), (info.id + ".sqlite").c_str(), nullptr);
        result.push_back(item);
    }
    return result;
}

bool validateSqliteModuleFile(const std::string &file, SqliteManagedModule &module, std::string &error) {
    gchar *tmp = g_dir_make_tmp("biblia-elim-validate-XXXXXX", nullptr);
    if (!tmp) { error = "cannot create validation directory"; return false; }
    std::string candidate = g_build_filename(tmp, "module.sqlite", nullptr);
    bool copied = copyAtomic(file, candidate, error);
    if (copied) {
        auto modules = listSqliteModules(tmp);
        if (modules.size() != 1) { error = "invalid SQLite module or metadata"; copied = false; }
        else module = modules.front();
    }
    g_remove(candidate.c_str()); g_rmdir(tmp); g_free(tmp);
    return copied;
}

bool installSqliteModule(const std::string &file, std::string &error,
                         SqliteManagedModule *installed, const std::string &directory) {
    SqliteManagedModule module;
    if (!validateSqliteModuleFile(file, module, error)) return false;
    const std::string dir = dirOrDefault(directory);
    if (g_mkdir_with_parents(dir.c_str(), 0755) != 0) { error = "cannot create module directory"; return false; }
    const std::string dest = g_build_filename(dir.c_str(), (module.info.id + ".sqlite").c_str(), nullptr);
    if (g_file_test(dest.c_str(), G_FILE_TEST_EXISTS)) { error = "module_id already installed: " + module.info.id; return false; }
    if (!copyAtomic(file, dest, error)) return false;
    module.path = dest; if (installed) *installed = module; return true;
}

bool importUsfmModule(const std::vector<std::string> &inputs, const UsfmImportOptions &options,
                      UsfmImportStats &stats, std::string &error, const std::string &directory) {
    gchar *tmp = g_dir_make_tmp("biblia-elim-import-XXXXXX", nullptr);
    if (!tmp) { error = "cannot create import directory"; return false; }
    std::string output = g_build_filename(tmp, "module.sqlite", nullptr);
    bool ok = importUsfm(inputs, output, options, stats, error);
    if (ok) ok = installSqliteModule(output, error, nullptr, directory);
    g_remove(output.c_str()); g_rmdir(tmp); g_free(tmp); return ok;
}

bool removeSqliteModule(const std::string &moduleId, std::string &error, const std::string &directory) {
    const std::string dir = dirOrDefault(directory);
    auto modules = listSqliteModules(dir);
    for (const auto &module : modules) if (module.info.id == moduleId) {
        gchar *realDir = g_canonicalize_filename(dir.c_str(), nullptr);
        gchar *realFile = g_canonicalize_filename(module.path.c_str(), nullptr);
        std::string prefix = std::string(realDir) + G_DIR_SEPARATOR;
        bool safe = g_str_has_prefix(realFile, prefix.c_str()) && g_file_test(module.path.c_str(), G_FILE_TEST_IS_REGULAR);
        if (!safe) { error = "module path is outside managed directory"; g_free(realDir); g_free(realFile); return false; }
        bool ok = g_remove(module.path.c_str()) == 0;
        if (!ok) error = "cannot remove module";
        g_free(realDir); g_free(realFile); return ok;
    }
    error = "module not found"; return false;
}
