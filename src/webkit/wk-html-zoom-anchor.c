#include "webkit/wk-html-zoom-anchor.h"

void
wk_html_zoom_anchor_init(WkHtmlZoomAnchor *anchor)
{
	g_return_if_fail(anchor != NULL);
	anchor->name = NULL;
	anchor->viewport_y = 0;
}

void
wk_html_zoom_anchor_clear(WkHtmlZoomAnchor *anchor)
{
	if (!anchor)
		return;
	g_clear_pointer(&anchor->name, g_free);
	anchor->viewport_y = 0;
}

gboolean
wk_html_zoom_anchor_capture(WkHtmlZoomAnchor *anchor, const gchar *name,
			    gint viewport_y)
{
	g_return_val_if_fail(anchor != NULL, FALSE);
	wk_html_zoom_anchor_clear(anchor);
	if (!name || !*name)
		return FALSE;
	anchor->name = g_strdup(name);
	anchor->viewport_y = viewport_y;
	return TRUE;
}

gboolean
wk_html_zoom_anchor_is_set(const WkHtmlZoomAnchor *anchor)
{
	return anchor && anchor->name && *anchor->name;
}

gdouble
wk_html_zoom_anchor_scroll_value(const WkHtmlZoomAnchor *anchor,
				 gint reflowed_anchor_y, gdouble lower,
				 gdouble upper, gdouble page_size)
{
	gdouble maximum;
	gdouble value;

	g_return_val_if_fail(wk_html_zoom_anchor_is_set(anchor), lower);
	maximum = MAX(lower, upper - page_size);
	value = reflowed_anchor_y - anchor->viewport_y;
	return CLAMP(value, lower, maximum);
}
