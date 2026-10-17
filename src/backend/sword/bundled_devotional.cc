#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "backend/sword/bundled_devotional.h"
#include <cstring>

namespace {
struct Edition { const char *language; const char *id; };
const Edition editions[] = {
    {"en_GB", "ElimDailyEN"}, {"es", "ElimDailyES"}, {"fr", "ElimDailyFR"},
    {"ko_KR", "ElimDailyKO"}, {"zh_CN", "ElimDailyZHCN"},
    {"pt_BR", "ElimDailyPTBR"}, {"zh_TW", "ElimDailyZHTW"}, {"pt", "ElimDailyPT"}
};
const Edition &editionFor(const char *language) {
    for (const auto &edition : editions)
        if (!g_strcmp0(language, edition.language)) return edition;
    return editions[0];
}
bool isBundled(const char *id) {
    for (const auto &edition : editions)
        if (!g_strcmp0(id, edition.id)) return true;
    return false;
}
}

const char *bundledDevotionalId(const char *language) {
    return editionFor(language).id;
}

void loadBundledDevotionalFrom(sword::SWMgr &manager, const char *directory,
                              const char *language)
{
    const Edition &edition = editionFor(language);
    if (!directory || !*directory || manager.getModule(edition.id)) return;
    gchar *root = g_build_filename(directory, edition.language, NULL);
    gchar *filename = g_ascii_strdown(edition.id, -1);
    gchar *configName = g_strconcat(filename, ".conf", NULL);
    gchar *config = g_build_filename(root, "mods.d", configName, NULL);
    if (g_file_test(config, G_FILE_TEST_IS_REGULAR))
        manager.augmentModules(root);
    g_free(config);
    g_free(configName);
    g_free(filename);
    g_free(root);
}

void loadBundledDevotional(sword::SWMgr &manager, const char *language)
{
    const char *override = g_getenv("BIBLIA_ELIM_BUNDLED_DEVOTIONAL");
    if (override && *override) {
        loadBundledDevotionalFrom(manager, override, language);
        return;
    }
#ifdef G_OS_WIN32
    gchar *root = g_win32_get_package_installation_directory_of_module(NULL);
    gchar *directory = g_build_filename(root, "share", "biblia-elim", "devotional", NULL);
    loadBundledDevotionalFrom(manager, directory, language);
    g_free(directory);
    g_free(root);
#else
    // Both a system prefix and the ~/.local installer are supported.
    gchar *exe = g_file_read_link("/proc/self/exe", NULL);
    if (exe) {
        gchar *bin = g_path_get_dirname(exe);
        gchar *directory = g_build_filename(bin, "..", "share", "biblia-elim", "devotional", NULL);
        loadBundledDevotionalFrom(manager, directory, language);
        g_free(directory);
        g_free(bin);
        g_free(exe);
    }
    loadBundledDevotionalFrom(manager, SHARE_DIR "/devotional", language);
    gchar *directory = g_build_filename(g_get_user_data_dir(), "biblia-elim", "devotional", NULL);
    loadBundledDevotionalFrom(manager, directory, language);
    g_free(directory);
#endif
#ifdef BIBLIA_ELIM_BUILD_DEVOTIONAL
    loadBundledDevotionalFrom(manager, BIBLIA_ELIM_BUILD_DEVOTIONAL, language);
#endif
}

const char *selectDevotional(GList *installed, const char *saved, const char *language)
{
    if (saved && *saved && !isBundled(saved) &&
        g_list_find_custom(installed, saved, (GCompareFunc)g_strcmp0)) return saved;
    GList *preferred = g_list_find_custom(installed, bundledDevotionalId(language),
                                         (GCompareFunc)g_strcmp0);
    if (preferred) return (const char *)preferred->data;
    if (saved && *saved && g_list_find_custom(installed, saved, (GCompareFunc)g_strcmp0))
        return saved;
    return installed ? (const char *)installed->data : NULL;
}
