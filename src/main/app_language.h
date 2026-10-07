#ifndef BIBLIA_ELIM_APP_LANGUAGE_H
#define BIBLIA_ELIM_APP_LANGUAGE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	const char *id;
	const char *native_name;
} AppLanguage;

const AppLanguage *app_languages(gsize *count);
/* Returned strings are owned by the caller. "None"/empty means system. */
gchar *app_language_normalize(const char *language);
gchar *app_language_read_settings(const char *path);
gboolean app_language_parse_args(int *argc, char **argv, gchar **choice,
				GError **error);
/* Call before SWORD, GTK or any translated startup content. */
void app_language_initialize(const char *choice, const char *settings_path);
const char *app_language_reader_locale(void);
/* Uses the settings document already loaded by main/xml.c. */
gboolean app_language_save_choice(const char *choice, gchar **current,
				gchar *settings_path);

G_END_DECLS
#endif
