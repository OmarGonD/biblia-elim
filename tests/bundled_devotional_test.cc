#include "backend/sword/bundled_devotional.h"
#include "main/app_language.h"
#include <glib/gstdio.h>
#include <swmodule.h>
#include <rawld.h>
#include <markupfiltmgr.h>
#include <cstring>
#include <string>
#include <set>
#include <filesystem>

static void checkYear(sword::SWModule *module) {
    g_assert_nonnull(module);
    g_assert_cmpstr(module->getConfigEntry("Feature"), ==, "DailyDevotion");
    g_assert_cmpstr(module->getConfigEntry("Encoding"), ==, "UTF-8");
    const int days[] = {31,29,31,30,31,30,31,31,30,31,30,31};
    int count = 0;
    std::set<std::string> distinct;
    for (int month = 1; month <= 12; ++month)
        for (int day = 1; day <= days[month-1]; ++day) {
            char key[6]; g_snprintf(key, sizeof(key), "%02d.%02d", month, day);
            module->setKey(key);
            // RawLD resolves its stored key lazily when the entry is read.
            std::string text = module->getRawEntry();
            g_assert_cmpstr(module->getKeyText(), ==, key);
            g_assert_true(text.size() > 100);
            g_assert_true(distinct.insert(text).second);
            g_assert_true(g_utf8_validate(text.c_str(), text.size(), NULL));
            g_assert_nonnull(strstr(text.c_str(), "<h3>"));
            g_assert_nonnull(strstr(text.c_str(), "<p><b>"));
            g_assert_true(strlen(module->renderText()) > 80);
            ++count;
        }
    g_assert_cmpint(count, ==, 366);
    auto *raw = dynamic_cast<sword::RawLD *>(module);
    g_assert_nonnull(raw);
    g_assert_cmpint(raw->getEntryCount(), ==, 366);
}

int main(int argc, char **argv) {
    g_assert_cmpint(argc, >=, 2);
    gchar *empty = g_dir_make_tmp("elim-devotional-XXXXXX", NULL);
    std::filesystem::create_directories(std::string(empty) + "/mods.d");
    if (!strcmp(argv[1], "--probe-stage")) {
        g_assert_cmpint(argc, >=, 3);
        app_language_initialize(argc == 5 ? argv[4] : NULL, argc >= 4 ? argv[3] : NULL);
        g_assert_cmpstr(app_language_reader_locale(), ==, argv[2]);
        sword::SWMgr manager(empty, true, new sword::MarkupFilterMgr(sword::FMT_XHTML), false, false);
        loadBundledDevotional(manager, app_language_reader_locale());
        checkYear(manager.getModule(bundledDevotionalId(app_language_reader_locale())));
        g_assert_cmpuint(manager.Modules.size(), ==, 1);
    } else {
        gsize count;
        const AppLanguage *languages = app_languages(&count);
        for (gsize i = 0; i < count; ++i) {
            sword::SWMgr manager(empty, true, new sword::MarkupFilterMgr(sword::FMT_XHTML), false, false);
            loadBundledDevotionalFrom(manager, argv[1], languages[i].id);
            const char *id = bundledDevotionalId(languages[i].id);
            auto *module = manager.getModule(id);
            checkYear(module);
            std::string before = module->getRawEntry();
            loadBundledDevotionalFrom(manager, argv[1], languages[i].id);
            g_assert_true(module == manager.getModule(id));
            g_assert_cmpuint(manager.Modules.size(), ==, 1);
            g_assert_cmpstr(module->getRawEntry(), ==, before.c_str());
        }
        // Selection follows the language for defaults; external choices survive.
        GList *list = NULL;
        list = g_list_append(list, (gpointer)"ElimDailyEN");
        list = g_list_append(list, (gpointer)"ElimDailyES");
        list = g_list_append(list, (gpointer)"UserDaily");
        g_assert_cmpstr(selectDevotional(list, "ElimDailyEN", "es"), ==, "ElimDailyES");
        g_assert_cmpstr(selectDevotional(list, "UserDaily", "es"), ==, "UserDaily");
        g_assert_cmpstr(selectDevotional(list, "missing", "es"), ==, "ElimDailyES");
        g_assert_cmpstr(selectDevotional(list, NULL, "es"), ==, "ElimDailyES");
        g_assert_cmpstr(selectDevotional(list, "ElimDailyES", "fr"), ==, "ElimDailyES");
        g_assert_null(selectDevotional(NULL, "missing", "es"));
        g_list_free(list);
        // An actual user module with the same ID must not be replaced by the package.
        std::filesystem::copy(std::string(argv[1]) + "/es", empty,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
        {
            sword::SWMgr manager(empty, true, nullptr, false, false);
            auto *user = manager.getModule("ElimDailyES");
            g_assert_nonnull(user);
            user->setKey("01.01"); user->setEntry("User's own devotional");
            loadBundledDevotionalFrom(manager, argv[1], "es");
            g_assert_true(user == manager.getModule("ElimDailyES"));
            g_assert_cmpstr(user->getRawEntry(), ==, "User's own devotional");
        }
        g_print("bundled_devotional_test: PASS (%zu languages, 366 dates each; selection and user preservation)\n", count);
    }
    std::filesystem::remove_all(empty);
    g_free(empty);
    return 0;
}
