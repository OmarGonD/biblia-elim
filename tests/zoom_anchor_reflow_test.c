#include "webkit/wk-html-zoom-anchor.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

int
main(void)
{
	WkHtmlZoomAnchor position;
	gdouble scroll;
	gchar *renderer = NULL;

	wk_html_zoom_anchor_init(&position);
	CHECK(!wk_html_zoom_anchor_is_set(&position));

	/* Verse V begins 24 px below the viewport top in the one-line layout. */
	CHECK(wk_html_zoom_anchor_capture(&position, "2024", 24));
	CHECK(!strcmp(position.name, "2024"));

	/* V-1 wraps during zoom-in, moving V down in buffer coordinates. The
	 * restored scroll moves by the same amount, so V remains 24 px into the
	 * viewport instead of the reading position drifting back to V-1. */
	scroll = wk_html_zoom_anchor_scroll_value(&position, 164, 0, 900, 300);
	CHECK(scroll == 140);
	CHECK(164 - scroll == 24);
	CHECK(!strcmp(position.name, "2024"));

	/* Repeated zoom and zoom-out preserve the same logical verse. */
	scroll = wk_html_zoom_anchor_scroll_value(&position, 208, 0, 980, 300);
	CHECK(scroll == 184);
	CHECK(208 - scroll == 24);
	scroll = wk_html_zoom_anchor_scroll_value(&position, 124, 0, 820, 300);
	CHECK(scroll == 100);
	CHECK(124 - scroll == 24);

	/* Bounds are safe at the beginning and end of a document. */
	CHECK(wk_html_zoom_anchor_capture(&position, "1001", 40));
	CHECK(wk_html_zoom_anchor_scroll_value(&position, 20, 0, 500, 200) == 0);
	CHECK(wk_html_zoom_anchor_capture(&position, "2024", 24));
	CHECK(wk_html_zoom_anchor_scroll_value(&position, 790, 0, 800, 250) == 550);
	wk_html_zoom_anchor_clear(&position);
	CHECK(!wk_html_zoom_anchor_is_set(&position));
	CHECK(!wk_html_zoom_anchor_capture(&position, "", 10));

	/* Integration contract: Bible zoom captures before CSS mutation and
	 * restores from the renderer's real post-reflow draw lifecycle. */
	CHECK(g_file_get_contents(SRCDIR "/src/webkit/wk-html.c", &renderer,
				  NULL, NULL));
	CHECK(renderer && strstr(renderer, "capture_zoom_anchor(html);") != NULL);
	CHECK(renderer && strstr(renderer, "restore_zoom_anchor_after_draw") != NULL);
	CHECK(renderer && strstr(renderer, "g_signal_connect_after(priv->view, \"draw\"") != NULL);
	CHECK(renderer && strstr(renderer, "gtk_adjustment_set_value(adjustment, value)") != NULL);
	g_free(renderer);

	printf("zoom_anchor_reflow_failures=%d\n", failures);
	return failures ? 1 : 0;
}
