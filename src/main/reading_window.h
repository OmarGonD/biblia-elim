/*
 * reading_window.h - which chapters the Bible pane lays out
 *
 * The pane holds a window of chapters of one book around the chapter being
 * read: the previous one, the current one and the next one, so reading
 * carries on past a chapter's end without clicking anywhere. When the
 * reading focus moves into a chapter whose neighbour is not laid out, the
 * window moves with it (GTKChapDisp / bibletext.c). Pure arithmetic here,
 * in the module's own chapter numbering.
 */
#ifndef XIPHOS_READING_WINDOW_H
#define XIPHOS_READING_WINDOW_H

#include <glib.h>

G_BEGIN_DECLS

/* Chapters laid out on either side of the current one in the normal pane. */
#define READING_WINDOW_RADIUS 1

/* First and last chapter to lay out around `chapter` in a book of
 * `chapter_count` chapters, `radius` chapters either side, clamped to the
 * book. radius <= 0 lays out the whole book. */
void reading_window_bounds(gint chapter, gint chapter_count, gint radius,
			   gint *first, gint *last);

/* Whether reading `chapter` with [first, last] laid out needs the window
 * moved: a neighbouring chapter that exists in the book is not there, or
 * the chapter itself is not. */
gboolean reading_window_needs_recenter(gint chapter, gint chapter_count,
				       gint first, gint last);

G_END_DECLS

#endif /* XIPHOS_READING_WINDOW_H */
