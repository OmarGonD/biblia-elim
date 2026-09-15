/*
 * reading_window.c - which chapters the Bible pane lays out
 */

#include "main/reading_window.h"

void
reading_window_bounds(gint chapter, gint chapter_count, gint radius,
		      gint *first, gint *last)
{
	if (chapter_count < 1)
		chapter_count = 1;
	chapter = CLAMP(chapter, 1, chapter_count);
	if (radius <= 0) {
		*first = 1;
		*last = chapter_count;
		return;
	}
	*first = MAX(1, chapter - radius);
	*last = MIN(chapter_count, chapter + radius);
}

gboolean
reading_window_needs_recenter(gint chapter, gint chapter_count, gint first,
			      gint last)
{
	if (chapter < 1 || chapter_count < 1)
		return FALSE;
	if (chapter < first || chapter > last)
		return TRUE;
	if (chapter > 1 && chapter - 1 < first)
		return TRUE;
	if (chapter < chapter_count && chapter + 1 > last)
		return TRUE;
	return FALSE;
}
