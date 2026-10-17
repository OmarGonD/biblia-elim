/*
 * reading_window.c - which chapters the Bible pane lays out
 */

#include "main/reading_window.h"

static gboolean first_paint_pending = TRUE;

gboolean
reading_window_first_paint_pending(void)
{
	return first_paint_pending;
}

void
reading_window_first_painted(void)
{
	first_paint_pending = FALSE;
}

void
reading_window_render_bounds(gint chapter, gint chapter_count, gint radius,
			     gboolean main_pane, gint *first, gint *last)
{
	reading_window_bounds(chapter, chapter_count, radius, first, last);
	if (main_pane && first_paint_pending)
		*first = *last = CLAMP(chapter, 1, MAX(1, chapter_count));
}

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
