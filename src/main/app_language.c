#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "main/app_language.h"

#include <libintl.h>
#include <libxml/parser.h>
#include <locale.h>
#include <string.h>

static const AppLanguage languages[] = {
	{ "en_GB", "English" }, { "es", "Español" }, { "fr", "Français" },
	{ "ko_KR", "한국어" }, { "zh_CN", "简体中文" },
	{ "pt_BR", "Português (Brasil)" }, { "zh_TW", "繁體中文" },
	{ "pt", "Português (Portugal)" }
};
static gchar *reader_locale;

const AppLanguage *app_languages(gsize *count)
{
	if (count) *count = G_N_ELEMENTS(languages);
	return languages;
}

gchar *app_language_normalize(const char *language)
{
	if (!language || !*language || !g_ascii_strcasecmp(language, "None") ||
	    !strcmp(language, "El del sistema") ||
	    !strcmp(language, "system")) return g_strdup("system");
	gchar *name = g_strdup(language);
	g_strstrip(name);
	for (char *p = name; *p; p++) {
		if (*p == '.' || *p == '@' || *p == ':') { *p = '\0'; break; }
		if (*p == '-') *p = '_';
	}
	const char *id = name;
	if (!*name || !g_ascii_strcasecmp(name, "None") || !strcmp(name, "system")) id = "system";
	else if (!g_ascii_strcasecmp(name, "C") || !g_ascii_strcasecmp(name, "POSIX")) id = "en_GB";
	else if (!g_ascii_strncasecmp(name, "en", 2) && (!name[2] || name[2] == '_')) id = "en_GB";
	else if (!g_ascii_strncasecmp(name, "es", 2) && (!name[2] || name[2] == '_')) id = "es";
	else if (!g_ascii_strncasecmp(name, "fr", 2) && (!name[2] || name[2] == '_')) id = "fr";
	else if (!g_ascii_strncasecmp(name, "ko", 2) && (!name[2] || name[2] == '_')) id = "ko_KR";
	else if (!g_ascii_strcasecmp(name, "zh") || !g_ascii_strcasecmp(name, "zh_Hans") ||
		 !g_ascii_strcasecmp(name, "zh_CN") || !g_ascii_strcasecmp(name, "zh_SG")) id = "zh_CN";
	else if (!g_ascii_strcasecmp(name, "zh_TW") || !g_ascii_strcasecmp(name, "zh_Hant") ||
		 !g_ascii_strcasecmp(name, "zh_HK")) id = "zh_TW";
	else if (!g_ascii_strcasecmp(name, "pt_BR")) id = "pt_BR";
	else if (!g_ascii_strcasecmp(name, "pt") || !g_ascii_strcasecmp(name, "pt_PT")) id = "pt";
	gchar *result = g_strdup(id);
	g_free(name);
	return result;
}

gchar *app_language_read_settings(const char *path)
{
	if (!path || !g_file_test(path, G_FILE_TEST_IS_REGULAR)) return NULL;
	xmlDoc *doc = xmlReadFile(path, NULL, XML_PARSE_NONET |
		XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
	if (!doc) return NULL;
	gchar *choice = NULL;
	xmlNode *root = xmlDocGetRootElement(doc);
	if (root && xmlStrEqual(root->name, BAD_CAST "Xiphos")) {
		for (xmlNode *section = root->children; section; section = section->next) {
			if (!xmlStrEqual(section->name, BAD_CAST "locale")) continue;
			for (xmlNode *item = section->children; item; item = item->next) {
				if (!xmlStrEqual(item->name, BAD_CAST "special")) continue;
				xmlChar *value = xmlNodeGetContent(item);
				choice = app_language_normalize((const char *)value);
				xmlFree(value);
				break;
			}
			break;
		}
	}
	xmlFreeDoc(doc);
	return choice;
}

gboolean app_language_parse_args(int *argc, char **argv, gchar **choice,
				GError **error)
{
	for (int i = 1; i < *argc;) {
		if (!g_str_has_prefix(argv[i], "--language=")) { i++; continue; }
		gchar *id = app_language_normalize(argv[i] + strlen("--language="));
		gboolean valid = !strcmp(id, "system");
		for (gsize n = 0; n < G_N_ELEMENTS(languages); n++)
			if (!strcmp(id, languages[n].id)) valid = TRUE;
		if (!valid || !argv[i][strlen("--language=")]) {
			g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
				"Unsupported interface language: %s", argv[i]);
			g_free(id);
			return FALSE;
		}
		g_free(*choice);
		*choice = id;
		memmove(argv + i, argv + i + 1, (*argc - i) * sizeof(*argv));
		--*argc;
	}
	return TRUE;
}

static const char *system_language(void)
{
	const char *variables[] = { "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG" };
	for (gsize i = 0; i < G_N_ELEMENTS(variables); i++) {
		const char *value = g_getenv(variables[i]);
		if (value && *value) return value;
	}
	return "en_GB";
}

static gchar *catalog_directory(const char *id)
{
	const char *override = g_getenv("BIBLIA_ELIM_LOCALE_DIR");
	if (override && *override) return g_strdup(override);
	gchar *exe = g_file_read_link("/proc/self/exe", NULL);
	gchar *base = exe ? g_path_get_dirname(exe) : NULL;
	gchar *candidates[3];
	gsize count = 0;
	if (base) {
		candidates[count++] = g_build_filename(base, "..", "share", "locale", NULL);
		candidates[count++] = g_build_filename(base, "..", "..", "locale", NULL);
	}
	candidates[count++] = g_build_filename(g_get_user_data_dir(), "locale", NULL);
	gchar *chosen = NULL;
	for (gsize i = 0; i < count; i++) {
		gchar *probe = g_build_filename(candidates[i], id, "LC_MESSAGES",
			GETTEXT_PACKAGE ".mo", NULL);
		if (!chosen && g_file_test(probe, G_FILE_TEST_IS_REGULAR))
			chosen = g_strdup(candidates[i]);
		g_free(probe);
		g_free(candidates[i]);
	}
	g_free(exe);
	g_free(base);
	return chosen ? chosen : g_strdup(PACKAGE_LOCALE_DIR);
}

void app_language_initialize(const char *choice, const char *settings_path)
{
	gchar *saved = choice ? app_language_normalize(choice) : app_language_read_settings(settings_path);
	gboolean use_system = !saved || !strcmp(saved, "system");
	gchar *id = use_system ? app_language_normalize(system_language()) : g_strdup(saved);
	/* English must also load a catalog: the fork has Spanish source strings.
	 * pt_BR falls back to the complete inherited Portuguese catalog. */
	gchar *preferences = !strcmp(id, "pt_BR") ? g_strdup("pt_BR:pt") : g_strdup(id);
	const char *configured = g_getenv("LANGUAGE");
	if (use_system && configured && *configured && strcmp(configured, id) &&
	    g_ascii_strcasecmp(configured, "C") && g_ascii_strcasecmp(configured, "POSIX")) {
		gchar *with_fallbacks = g_strconcat(preferences, ":", configured, NULL);
		g_free(preferences);
		preferences = with_fallbacks;
	}
	g_setenv("LANGUAGE", preferences, TRUE);
	setlocale(LC_ALL, "");
	/* GNU gettext also ignores LANGUAGE in C.UTF-8. Use an installed base
	 * message locale; the requested French/Korean/etc. OS locales are not
	 * required. Keep numeric/date conventions from the system. */
	const char *messages = setlocale(LC_MESSAGES, NULL);
	if (!messages || !strcmp(messages, "C") || !strcmp(messages, "POSIX") ||
	    g_str_has_prefix(messages, "C.")) {
		if (!setlocale(LC_MESSAGES, "en_US.UTF-8") &&
		    !setlocale(LC_MESSAGES, "en_US.utf8")) {
			const char *system = g_getenv("LANG");
			if (system && *system) setlocale(LC_MESSAGES, system);
		}
	}
	g_free(reader_locale);
	reader_locale = g_strdup(id);
#ifdef ENABLE_NLS
	gchar *directory = catalog_directory(id);
	bindtextdomain(GETTEXT_PACKAGE, directory);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	g_free(directory);
#endif
	g_free(saved);
	g_free(id);
	g_free(preferences);
}

const char *app_language_reader_locale(void)
{
	return reader_locale ? reader_locale : system_language();
}
