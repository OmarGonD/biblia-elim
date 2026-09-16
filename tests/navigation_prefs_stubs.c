/*
 * What xml.c reaches for beyond the settings document itself: the test
 * only reads and writes <misc>, so these never run.
 */
#include <glib.h>
#include <stdio.h>

#include "main/settings.h"

SETTINGS settings;

void
gui_generic_warning_modal(const char *message)
{
	fprintf(stderr, "unexpected warning: %s\n", message ? message : "");
}

const char *
main_get_osisref_from_key(const char *module_name, const char *key)
{
	(void)module_name;
	return key ? key : "";
}

GList *
get_list(gint type)
{
	(void)type;
	return NULL;
}
