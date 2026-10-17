#include "main/app_language.h"
#include "main/xml.h"
#include <string.h>

gboolean app_language_save_choice(const char *choice, gchar **current,
				gchar *settings_path)
{
	if (!choice || !current || !settings_path) return FALSE;
	gchar *id = app_language_normalize(choice);
	gchar *previous = app_language_normalize(*current);
	gboolean changed = strcmp(id, previous) != 0;
	g_free(previous);
	if (changed) {
		const char *stored = !strcmp(id, "system") ? "None" : id;
		xml_set_or_create_value("locale", "special", stored);
		xml_save_settings_doc(settings_path);
		g_free(*current);
		*current = g_strdup(stored);
	}
	g_free(id);
	return changed;
}

gchar *app_language_load_or_seed_choice(void)
{
    gchar *saved = xml_get_value("locale", "special");
    if (saved) return saved;
    gchar *choice = app_language_installation_choice();
    const char *stored = !choice || !strcmp(choice, "system") ? "None" : choice;
    xml_set_or_create_value("locale", "special", stored);
    saved = g_strdup(stored);
    g_free(choice);
    return saved;
}
