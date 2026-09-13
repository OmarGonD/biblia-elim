#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include <swmodule.h>
#include <versekey.h>

#include "backend/sword/sword_backend.h"
#include "main/settings.h"

SETTINGS settings = {};
char *sword_locale = nullptr;

extern "C" void main_dialog_search_percent_update(char, void *) {}
extern "C" void main_sidebar_search_percent_update(char, void *) {}
extern "C" void main_index_percent_update(char, void *) {}
extern "C" void main_setup_displays(void) {}
extern "C" void main_clear_abbreviations(void) {}
extern "C" void main_add_abbreviation(const char *, const char *) {}
extern "C" int main_is_module(char *) { return 0; }
extern "C" void gui_generic_warning(const char *) {}
extern "C" const char *main_get_language_map(const char *language)
{
	return language;
}
extern "C" char *main_get_mod_config_file(const char *, const char *)
{
	return nullptr;
}
extern "C" char *main_format_number(int value)
{
	return g_strdup_printf("%d", value);
}
extern "C" gchar *XI_g_strdup_printf(const char *, int, const gchar *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	gchar *result = g_strdup_vprintf(format, arguments);
	va_end(arguments);
	return result;
}

int main()
{
	SwordBackend backend;
	std::string module_id;
	BibleKeyInfo key_info;
	if (backend.hasModule("SpaRV1909") &&
	    backend.resolveKey("SpaRV1909", "Genesis 1:1", key_info)) {
		module_id = "SpaRV1909";
	} else {
		for (const BibleModuleInfo &module : backend.listModules()) {
			if (module.type == BibleModuleType::Bible &&
			    backend.resolveKey(module.id, "Genesis 1:1", key_info)) {
				module_id = module.id;
				break;
			}
		}
	}
	if (module_id.empty()) {
		std::puts("sword_backend_key_lifecycle_skipped=no-bible-module");
		return 0;
	}

	sword::SWModule *module = backend.get_SWModule(module_id.c_str());
	g_assert_nonnull(module);
	module->setKeyText(key_info.key.c_str());
	sword::SWKey *const original_key = module->getKey();
	g_assert_nonnull(original_key);

	const std::string before_reads = module->getKeyText();
	for (int iteration = 0; iteration < 1000; ++iteration) {
		BibleReference reference = key_info.reference;
		reference.verse = 1 + (iteration % 5);
		BibleVerseContent content = backend.getVerseContent(module_id, reference);
		g_assert_true(content.valid);
		g_assert_true(module->getKey() == original_key);
		g_assert_cmpstr(module->getKeyText(), ==, before_reads.c_str());
	}

	const std::string before_chapter = module->getKeyText();
	(void)backend.getChapter(module_id, key_info.reference, false);
	g_assert_true(module->getKey() == original_key);
	g_assert_cmpstr(module->getKeyText(), ==, before_chapter.c_str());

	auto *final_key = dynamic_cast<sword::VerseKey *>(module->getKey());
	g_assert_nonnull(final_key);
	g_assert_cmpint(final_key->getTestament(), ==, key_info.reference.testament);
	g_assert_cmpint(final_key->getBook(), ==, key_info.reference.book);
	g_assert_cmpint(final_key->getChapter(), ==, key_info.reference.chapter);
	g_assert_cmpint(final_key->getVerse(), ==, key_info.reference.verse);
	std::printf("sword_backend_key_lifecycle_failures=0 module=%s reads=1000\n",
		module_id.c_str());
	return 0;
}
