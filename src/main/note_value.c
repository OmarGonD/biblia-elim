/*
 * Biblia Elim
 * note_value.c - la forma en que se guarda cada nota (ver note_value.h)
 */
#include "main/note_value.h"

#include <stdlib.h>
#include <string.h>

gchar *encode_note_value(const gchar *color, const gchar *text,
			 const gchar *note, gint pos)
{
	gchar *etext = g_uri_escape_string(text ? text : "", NULL, TRUE);
	gchar *enote = g_uri_escape_string(note ? note : "", NULL, TRUE);
	gchar *value = (pos >= 0)
			   ? g_strdup_printf("%s|%s|%s|%d", color ? color : "", etext, enote, pos)
			   : g_strdup_printf("%s|%s|%s|", color ? color : "", etext, enote);
	g_free(etext);
	g_free(enote);
	return value;
}

gboolean decode_note_value(const gchar *value, gchar **color, gchar **text,
			   gchar **note, gint *pos)
{
	gchar **parts = g_strsplit(value, "|", 4);
	if (!parts[0] || !parts[1] || !parts[2]) {
		g_strfreev(parts);
		return FALSE;
	}
	*color = (*parts[0]) ? g_strdup(parts[0]) : NULL;
	*text = g_uri_unescape_string(parts[1], NULL);
	*note = g_uri_unescape_string(parts[2], NULL);
	*pos = (parts[3] && *parts[3]) ? atoi(parts[3]) : -1;
	g_strfreev(parts);
	return TRUE;
}

gboolean note_label_split(const gchar *label, gchar **module,
			  gchar **osisref, gchar **id)
{
	const gchar *space, *hash;

	*module = *osisref = *id = NULL;
	if (!label)
		return FALSE;
	space = strchr(label, ' ');
	if (!space || space == label || !space[1])
		return FALSE;
	hash = strrchr(space + 1, '#');
	if (hash == space + 1)
		return FALSE;
	*module = g_strndup(label, (gsize)(space - label));
	if (hash) {
		*osisref = g_strndup(space + 1, (gsize)(hash - space - 1));
		*id = g_strdup(hash + 1);
	} else {
		*osisref = g_strdup(space + 1);
	}
	return TRUE;
}
