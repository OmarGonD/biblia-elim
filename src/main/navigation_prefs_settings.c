/*
 * navigation_prefs_settings.c - navigation preferences in settings.xml
 */

#include "main/navigation_prefs.h"
#include "main/xml.h"

NavigationPrefs
navigation_prefs_load_settings(void)
{
	NavigationPrefs prefs = navigation_prefs_parse(
	    xml_get_value("misc", NAVIGATION_PREFS_KEY_WHEEL_PERCENT),
	    xml_get_value("misc", NAVIGATION_PREFS_KEY_FOCUS_MODE));

	navigation_prefs_apply(&prefs);
	return prefs;
}

void
navigation_prefs_save_settings(const NavigationPrefs *prefs,
			       const char *settings_file)
{
	gchar *percent = g_strdup_printf(
	    "%d", navigation_prefs_snap_percent(prefs->wheel_percent));

	xml_set_or_create_value("misc", NAVIGATION_PREFS_KEY_WHEEL_PERCENT,
				percent);
	xml_set_or_create_value("misc", NAVIGATION_PREFS_KEY_FOCUS_MODE,
				navigation_prefs_focus_mode_name(prefs->focus_mode));
	g_free(percent);
	if (settings_file)
		xml_save_settings_doc((char *)settings_file);
}
