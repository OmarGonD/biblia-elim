/* Logical viewport position retained while a WkHtml surface reflows. */
#ifndef BIBLIA_ELIM_WK_HTML_ZOOM_ANCHOR_H
#define BIBLIA_ELIM_WK_HTML_ZOOM_ANCHOR_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	gchar *name;
	gint viewport_y;
} WkHtmlZoomAnchor;

void wk_html_zoom_anchor_init(WkHtmlZoomAnchor *anchor);
void wk_html_zoom_anchor_clear(WkHtmlZoomAnchor *anchor);
gboolean wk_html_zoom_anchor_capture(WkHtmlZoomAnchor *anchor,
				     const gchar *name, gint viewport_y);
gboolean wk_html_zoom_anchor_is_set(const WkHtmlZoomAnchor *anchor);
gdouble wk_html_zoom_anchor_scroll_value(const WkHtmlZoomAnchor *anchor,
					gint reflowed_anchor_y,
					gdouble lower, gdouble upper,
					gdouble page_size);

G_END_DECLS

#endif
