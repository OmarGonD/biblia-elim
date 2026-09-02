/*
 * Biblia Elim — native GtkTextView HTML pane (replaces WebKitWebView).
 */

#ifndef __WK_HTML_H__
#define __WK_HTML_H__

#undef DATADIR

#include <gtk/gtk.h>
#include "main/module_dialogs.h"

G_BEGIN_DECLS
#define WK_TYPE_HTML (wk_html_get_type())
#define WK_HTML(o) (G_TYPE_CHECK_INSTANCE_CAST((o), WK_TYPE_HTML, WkHtml))
#define WK_HTML_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), WK_TYPE_HTML, WkHtmlClass))
#define WK_HTML_IS_HTML(o) (G_TYPE_CHECK_INSTANCE_TYPE((o), WK_TYPE_HTML))
#define WK_HTML_IS_HTML_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE((k), WK_TYPE_HTML))
typedef struct _WkHtml WkHtml;
typedef struct _WkHtmlClass WkHtmlClass;
typedef struct _WkHtmlPriv WkHtmlPrivate;

struct _WkHtml
{
	GtkBox parent;
	WkHtmlPrivate *priv;
};
struct _WkHtmlPriv
{
	GtkTextView *view;
	GtkTextBuffer *buffer;
	GtkWidget *scroll;
	GtkCssProvider *css;
	GHashTable *anchor_ht;
	GPtrArray *anchor_list;
	GArray *links;		/* Link[]: href por rango de offsets */
	gchar *content;
	gsize content_len;	/* bytes en uso; content_alloc, reservados */
	gsize content_alloc;
	gchar *mime;
	gchar *find_string;
	gboolean initialised;
	gchar *base_uri;
	gchar *anchor;
	gboolean frames_enabled;
	guint timeout;
	guint jump_tries;
	gint pane;
	gboolean is_dialog;
	DIALOG_DATA *dialog;
	gint find_offset;
	gchar *hover_uri;
};
struct _WkHtmlClass
{
	GtkBoxClass parent;

	void (*uri_selected)(WkHtml *view, gchar *uri, gboolean handled);
	gboolean (*frame_selected)(WkHtml *view, gchar *uri, gboolean handled);
	void (*title_changed)(WkHtml *view, const gchar *new_title);
	void (*popupmenu_requested)(WkHtml *view, const gchar *link);
};

GType wk_html_get_type(void);
WkHtml *wk_html_create(void);
WkHtml *wk_html_new(DIALOG_DATA *dialog, gboolean is_dialog, gint pane);
void wk_html_set_base_uri(WkHtml *html, const gchar *uri);
void wk_html_open_stream(WkHtml *html, const gchar *mime);
void wk_html_write(WkHtml *html, const gchar *data, gint len);
void wk_html_printf(WkHtml *html, gchar *format, ...) G_GNUC_PRINTF(2, 3);
void wk_html_close(WkHtml *html);

void wk_html_render_data(WkHtml *html, const char *data, guint32 len);

void wk_html_frames(WkHtml *html, gboolean enable);
gboolean wk_html_find(WkHtml *html, const gchar *find_string);
gboolean wk_html_find_again(WkHtml *html, gboolean forward);
void wk_html_jump_to_anchor(WkHtml *html, gchar *anchor);
void wk_html_ensure_anchor_visible(WkHtml *html, const gchar *anchor);
void wk_html_copy_selection(WkHtml *html);
void wk_html_enable_caret_browsing(WkHtml *html);

void wk_html_select_all(WkHtml *html);

void wk_html_print(WkHtml *html);

gboolean wk_html_initialize(void);
void wk_html_shutdown(void);

GtkTextView *wk_html_get_view(WkHtml *html);
gchar *wk_html_anchor_at(WkHtml *html, const GtkTextIter *iter);
gchar *wk_html_highlight_id_at(const GtkTextIter *iter);
void wk_html_highlight_apply(WkHtml *html, GtkTextIter *start, GtkTextIter *end,
			     const gchar *id, const gchar *color);
void wk_html_highlight_set_color(WkHtml *html, const gchar *id, const gchar *color);
void wk_html_highlight_remove(WkHtml *html, const gchar *id);
gboolean wk_html_highlight_bounds(WkHtml *html, const gchar *id,
				  GtkTextIter *start, GtkTextIter *end);

/* bounds of the verse/anchor named `anchor` (the numeric name Xiphos
 * gives each verse's <a name="..."> when rendering a chapter): start is
 * the anchor's own position, end is wherever the next anchor in the
 * chapter begins (or the end of the buffer, for the last one). */
gboolean wk_html_anchor_bounds(WkHtml *html, const gchar *anchor,
			       GtkTextIter *start, GtkTextIter *end);
/* Moves (or clears, if start/end are NULL) a single reading-position
 * indicator -- a solid background + slightly larger text, no underline,
 * distinct from wk_html_highlight_* above's user-created highlights --
 * so callers can track "the verse currently in view" without
 * re-rendering the pane (which would fight the user's own scrolling). */
void wk_html_reading_focus_set(WkHtml *html, GtkTextIter *start, GtkTextIter *end,
			       const gchar *bg_color, const gchar *fg_color);

G_END_DECLS
#endif /* __WK_HTML_H__ */
