/*
 * Biblia Elim — native GtkTextView renderer for Sword HTML.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <pango/pangocairo.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

#include "main/url.hh"
#include "main/module_dialogs.h"
#include "main/sword.h"
#include "main/settings.h"

#include "wk-html.h"
#include "marshal.h"

#include "gui/dictlex.h"
#include "gui/interlineal.h"

extern gboolean shift_key_pressed;
extern gboolean in_url;

static gchar *x_uri = NULL;
static gboolean db_click;

enum {
	URI_SELECTED,
	FRAME_SELECTED,
	TITLE_CHANGED,
	POPUPMENU_REQUESTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
static GObjectClass *parent_class = NULL;

typedef struct {
	gchar *name;
	GtkTextMark *mark;
} Anchor;

/* A link's reach in the buffer. Offsets are stable for as long as the
 * document is: load_html() empties the buffer and starts the list over
 * before anything is inserted, and nothing edits the text afterwards.
 * Appended in document order while walking, so a lookup is a binary
 * search. */
typedef struct {
	gint start;
	gint end;
	gchar *href;
} Link;

typedef struct {
	guint bold : 1;
	guint italic : 1;
	guint underline : 1;
	guint strike : 1;
	guint sup : 1;
	guint sub : 1;
	guint small : 1;
	guint big : 1;
	guint center : 1;
	guint right : 1;
	guint ilblock : 1;
	guint illabel : 1;
	guint ilorig : 1;
	guint ilrtl : 1;
	guint para_bg : 1;
	gchar *fg;
	gchar *bg;
	gchar *href;
	gchar *hl_id;
	gdouble scale;
} Style;

typedef struct {
	WkHtml *html;
	GtkTextBuffer *buf;
	Style st;
	gboolean skip;
	gboolean at_line_start;
	gchar *body_bg;
	gchar *body_fg;
	gboolean in_repair;
} ParseCtx;

/* Where the verse being read sits in the viewport: high enough to have
 * the rest of the passage below it, low enough to keep a couple of
 * verses of context above rather than pinning it to the top edge. */
#define READING_FOCUS_YALIGN 0.20

static void walk_node(ParseCtx *ctx, xmlNode *node);
static gchar *iter_href(WkHtml *html, const GtkTextIter *iter);
static void insert_verse_tools_chip(ParseCtx *ctx, const char *key, gboolean has_note);

static void wk_html_class_init(WkHtmlClass *klass);
static void wk_html_init(WkHtml *html);

G_DEFINE_TYPE_WITH_PRIVATE(WkHtml, wk_html, GTK_TYPE_BOX)

static void
style_clear(Style *s)
{
	g_free(s->fg);
	g_free(s->bg);
	g_free(s->href);
	g_free(s->hl_id);
	memset(s, 0, sizeof(*s));
	s->scale = 1.0;
}

static Style
style_copy(const Style *s)
{
	Style d = *s;
	d.fg = g_strdup(s->fg);
	d.bg = g_strdup(s->bg);
	d.href = g_strdup(s->href);
	d.hl_id = g_strdup(s->hl_id);
	return d;
}

static gboolean
class_has(const char *klass, const char *token)
{
	const char *p;
	int n;

	if (!klass || !token)
		return FALSE;
	n = (int)strlen(token);
	for (p = klass; *p; p++) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		if (strncmp(p, token, n) == 0 &&
		    (p[n] == '\0' || p[n] == ' ' || p[n] == '\t'))
			return TRUE;
		while (*p && *p != ' ' && *p != '\t')
			p++;
		if (!*p)
			break;
	}
	return FALSE;
}

static void
parse_style_attr(const char *css, Style *st)
{
	gchar **parts;
	int i;

	if (!css)
		return;
	parts = g_strsplit(css, ";", -1);
	for (i = 0; parts && parts[i]; i++) {
		gchar *k, *v, *colon;
		g_strstrip(parts[i]);
		colon = strchr(parts[i], ':');
		if (!colon)
			continue;
		*colon = '\0';
		k = parts[i];
		v = colon + 1;
		g_strstrip(k);
		g_strstrip(v);
		if (!g_ascii_strcasecmp(k, "color")) {
			g_free(st->fg);
			st->fg = g_strdup(v);
		} else if (!g_ascii_strcasecmp(k, "background-color") ||
			   !g_ascii_strcasecmp(k, "background")) {
			g_free(st->bg);
			st->bg = g_strdup(v);
		} else if (!g_ascii_strcasecmp(k, "text-decoration") &&
			   strstr(v, "underline")) {
			st->underline = TRUE;
		} else if (!g_ascii_strcasecmp(k, "font-weight") &&
			   (strstr(v, "bold") || atoi(v) >= 600)) {
			st->bold = TRUE;
		} else if (!g_ascii_strcasecmp(k, "font-style") &&
			   strstr(v, "italic")) {
			st->italic = TRUE;
		} else if (!g_ascii_strcasecmp(k, "text-align") &&
			   strstr(v, "center")) {
			st->center = TRUE;
		} else if (!g_ascii_strcasecmp(k, "text-align") &&
			   strstr(v, "right")) {
			st->right = TRUE;
		}
	}
	g_strfreev(parts);
}

static GtkTextTag *
color_tag(GtkTextBuffer *buf, const char *prefix, const char *color, const char *prop)
{
	gchar *name;
	GtkTextTag *tag;
	GtkTextTagTable *table;

	if (!color || !*color)
		return NULL;
	name = g_strdup_printf("%s:%s", prefix, color);
	table = gtk_text_buffer_get_tag_table(buf);
	tag = gtk_text_tag_table_lookup(table, name);
	if (!tag)
		tag = gtk_text_buffer_create_tag(buf, name, prop, color, NULL);
	g_free(name);
	return tag;
}

static void
ensure_il_tags(GtkTextBuffer *buf)
{
	GtkTextTagTable *t = gtk_text_buffer_get_tag_table(buf);

	if (!gtk_text_tag_table_lookup(t, "ilblock"))
		gtk_text_buffer_create_tag(buf, "ilblock",
					   "left-margin", 18,
					   "right-margin", 10,
					   "pixels-above-lines", 7,
					   "pixels-below-lines", 7,
					   "pixels-inside-wrap", 4,
					   NULL);
	if (!gtk_text_tag_table_lookup(t, "illabel"))
		gtk_text_buffer_create_tag(buf, "illabel",
					   "family", "Liberation Serif",
					   "variant", PANGO_VARIANT_SMALL_CAPS,
					   "style", PANGO_STYLE_ITALIC,
					   "scale", PANGO_SCALE_SMALL,
					   "weight", PANGO_WEIGHT_MEDIUM,
					   NULL);
	if (!gtk_text_tag_table_lookup(t, "ilorig"))
		gtk_text_buffer_create_tag(buf, "ilorig",
					   "family", "Noto Serif",
					   "scale", 1.22,
					   "weight", PANGO_WEIGHT_NORMAL,
					   "letter-spacing", 1024,
					   NULL);
	if (!gtk_text_tag_table_lookup(t, "ilorig-he"))
		gtk_text_buffer_create_tag(buf, "ilorig-he",
					   "family", "Noto Serif Hebrew",
					   "scale", 1.28,
					   "weight", PANGO_WEIGHT_NORMAL,
					   "letter-spacing", 512,
					   NULL);
}

static void
ensure_stock_tags(GtkTextBuffer *buf)
{
	GtkTextTagTable *t = gtk_text_buffer_get_tag_table(buf);
	ensure_il_tags(buf);
	if (gtk_text_tag_table_lookup(t, "bold"))
		return;
	gtk_text_buffer_create_tag(buf, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(buf, "italic", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(buf, "underline", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(buf, "strike", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag(buf, "sup", "rise", 4000, "scale", PANGO_SCALE_SMALL, NULL);
	gtk_text_buffer_create_tag(buf, "sub", "rise", -3000, "scale", PANGO_SCALE_SMALL, NULL);
	gtk_text_buffer_create_tag(buf, "small", "scale", PANGO_SCALE_SMALL, NULL);
	gtk_text_buffer_create_tag(buf, "big", "scale", PANGO_SCALE_LARGE, NULL);
	gtk_text_buffer_create_tag(buf, "st",
				   "foreground", "#6B2D8B",
				   "rise", 4500,
				   "scale", 0.72,
				   "weight", PANGO_WEIGHT_SEMIBOLD,
				   NULL);
	gtk_text_buffer_create_tag(buf, "center", "justification", GTK_JUSTIFY_CENTER, NULL);
	gtk_text_buffer_create_tag(buf, "right", "justification", GTK_JUSTIFY_RIGHT, NULL);
	gtk_text_buffer_create_tag(buf, "find-hl",
				   "background", "#ffe08a",
				   NULL);
}

static void
apply_style_tags(ParseCtx *ctx, GtkTextIter *s, GtkTextIter *e)
{
	GtkTextBuffer *buf = ctx->buf;
	Style *st = &ctx->st;
	GtkTextTag *tag;

	if (st->bold)
		gtk_text_buffer_apply_tag_by_name(buf, "bold", s, e);
	if (st->italic)
		gtk_text_buffer_apply_tag_by_name(buf, "italic", s, e);
	if (st->underline)
		gtk_text_buffer_apply_tag_by_name(buf, "underline", s, e);
	if (st->strike)
		gtk_text_buffer_apply_tag_by_name(buf, "strike", s, e);
	if (st->sup)
		gtk_text_buffer_apply_tag_by_name(buf, "sup", s, e);
	if (st->sub)
		gtk_text_buffer_apply_tag_by_name(buf, "sub", s, e);
	if (st->small)
		gtk_text_buffer_apply_tag_by_name(buf, "small", s, e);
	if (st->big)
		gtk_text_buffer_apply_tag_by_name(buf, "big", s, e);
	if (st->center)
		gtk_text_buffer_apply_tag_by_name(buf, "center", s, e);
	if (st->right)
		gtk_text_buffer_apply_tag_by_name(buf, "right", s, e);
	if (st->ilblock)
		gtk_text_buffer_apply_tag_by_name(buf, "ilblock", s, e);
	if (st->illabel)
		gtk_text_buffer_apply_tag_by_name(buf, "illabel", s, e);
	if (st->ilorig)
		gtk_text_buffer_apply_tag_by_name(buf,
						 st->ilrtl ? "ilorig-he" : "ilorig",
						 s, e);
	if (st->hl_id)
		wk_html_highlight_apply(ctx->html, s, e, st->hl_id,
					st->bg ? st->bg : "#FFEB3B");
	if (st->href) {
		/* The href lives in a side index keyed by buffer offset,
		 * not in a GtkTextTag of its own. One tag per distinct link
		 * meant ~5000 of them for a whole book, and taking them
		 * back out at the next load cost 548 ms -- more than
		 * parsing the document and filling the buffer combined.
		 * Appearance still comes from a tag, but the shared
		 * colour one, which color_tag() already dedupes. */
		Link lk;
		const char *link_fg = st->fg ? st->fg : "#2C4A6E";

		lk.start = gtk_text_iter_get_offset(s);
		lk.end = gtk_text_iter_get_offset(e);
		lk.href = g_strdup(st->href);
		g_array_append_val(ctx->html->priv->links, lk);

		tag = color_tag(buf, "fg", link_fg, "foreground");
		if (tag)
			gtk_text_buffer_apply_tag(buf, tag, s, e);
	}
	if (st->fg) {
		tag = color_tag(buf, "fg", st->fg, "foreground");
		if (tag)
			gtk_text_buffer_apply_tag(buf, tag, s, e);
	}
	if (st->bg) {
		tag = color_tag(buf, "bg", st->bg, "background");
		if (tag)
			gtk_text_buffer_apply_tag(buf, tag, s, e);
		if (st->para_bg) {
			tag = color_tag(buf, "pbg", st->bg, "paragraph-background");
			if (tag)
				gtk_text_buffer_apply_tag(buf, tag, s, e);
		}
	}
}

static gboolean
looks_like_markup(const char *t)
{
	const char *p;

	if (!t)
		return FALSE;
	for (p = t; (p = strchr(p, '<')) != NULL; p++) {
		char n = p[1];
		if (g_ascii_isalpha((guchar)n) || n == '/' || n == '!')
			return TRUE;
	}
	return FALSE;
}

static void
insert_text(ParseCtx *ctx, const char *text)
{
	GtkTextIter start, end;
	gint off;
	const gchar *p, *run;
	GString *norm;

	if (!text || !*text || ctx->skip)
		return;

	if (!ctx->in_repair && looks_like_markup(text)) {
		htmlDocPtr doc;
		gchar *wrap = g_strdup_printf("<div>%s</div>", text);

		ctx->in_repair = TRUE;
		doc = htmlReadMemory(wrap, (int)strlen(wrap), "file://", "UTF-8",
				     HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
					 HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
		g_free(wrap);
		if (doc) {
			walk_node(ctx, xmlDocGetRootElement(doc));
			xmlFreeDoc(doc);
			ctx->in_repair = FALSE;
			return;
		}
		ctx->in_repair = FALSE;
	}

	/* Drop '\r', turn each '\n'/'\t' into one space, copy everything
	 * else verbatim -- in runs rather than a character at a time. The
	 * three bytes we split on are ASCII, and no UTF-8 continuation byte
	 * is ever < 0x80, so scanning bytewise cannot land inside a
	 * multi-byte character. Output is never longer than the input, so
	 * sizing the buffer up front makes this a single allocation. */
	norm = g_string_sized_new(strlen(text) + 1);
	for (p = text; *p;) {
		run = p;
		while (*p && *p != '\r' && *p != '\n' && *p != '\t')
			p++;
		if (p > run)
			g_string_append_len(norm, run, p - run);
		for (; *p == '\r' || *p == '\n' || *p == '\t'; p++) {
			if (*p != '\r')
				g_string_append_c(norm, ' ');
		}
	}
	if (!norm->len) {
		g_string_free(norm, TRUE);
		return;
	}
	/* collapse leading spaces at line start */
	if (ctx->at_line_start) {
		gchar *s = norm->str;
		while (*s == ' ')
			s++;
		if (s != norm->str)
			g_string_erase(norm, 0, s - norm->str);
		if (!norm->len) {
			g_string_free(norm, TRUE);
			return;
		}
	}

	gtk_text_buffer_get_end_iter(ctx->buf, &end);
	off = gtk_text_iter_get_offset(&end);
	gtk_text_buffer_insert(ctx->buf, &end, norm->str, -1);
	gtk_text_buffer_get_iter_at_offset(ctx->buf, &start, off);
	gtk_text_buffer_get_end_iter(ctx->buf, &end);
	apply_style_tags(ctx, &start, &end);
	ctx->at_line_start = FALSE;
	g_string_free(norm, TRUE);
}

static void
insert_break(ParseCtx *ctx, gboolean para)
{
	GtkTextIter end;
	if (ctx->skip)
		return;
	gtk_text_buffer_get_end_iter(ctx->buf, &end);
	if (para && !ctx->at_line_start)
		gtk_text_buffer_insert(ctx->buf, &end, "\n\n", 2);
	else if (!ctx->at_line_start)
		gtk_text_buffer_insert(ctx->buf, &end, "\n", 1);
	ctx->at_line_start = TRUE;
}

static void
place_anchor(ParseCtx *ctx, const char *name)
{
	Anchor *a;
	GtkTextIter end;
	gchar *key;

	if (!name || !*name)
		return;
	if (g_hash_table_lookup(ctx->html->priv->anchor_ht, name))
		return;
	gtk_text_buffer_get_end_iter(ctx->buf, &end);
	key = g_strdup_printf("m:%s", name);
	a = g_new0(Anchor, 1);
	a->name = g_strdup(name);
	a->mark = gtk_text_buffer_create_mark(ctx->buf, key, &end, TRUE);
	g_ptr_array_add(ctx->html->priv->anchor_list, a);
	g_hash_table_insert(ctx->html->priv->anchor_ht, g_strdup(name), a);
	g_free(key);
}

static const char *
el_name(xmlNode *n)
{
	return n && n->name ? (const char *)n->name : "";
}

static char *
el_prop(xmlNode *n, const char *p)
{
	xmlChar *v = xmlGetProp(n, (const xmlChar *)p);
	char *s;
	if (!v)
		return NULL;
	s = g_strdup((const char *)v);
	xmlFree(v);
	return s;
}

static gboolean
is_block(const char *n)
{
	return !g_ascii_strcasecmp(n, "p") || !g_ascii_strcasecmp(n, "div") ||
	       !g_ascii_strcasecmp(n, "h1") || !g_ascii_strcasecmp(n, "h2") ||
	       !g_ascii_strcasecmp(n, "h3") || !g_ascii_strcasecmp(n, "h4") ||
	       !g_ascii_strcasecmp(n, "h5") || !g_ascii_strcasecmp(n, "h6") ||
	       !g_ascii_strcasecmp(n, "tr") || !g_ascii_strcasecmp(n, "li") ||
	       !g_ascii_strcasecmp(n, "blockquote") || !g_ascii_strcasecmp(n, "center");
}

/* The tools affordance next to each verse used to be a real GtkButton
 * holding a GtkDrawingArea that painted the icon, anchored into the
 * buffer -- two widgets per verse, rebuilt from scratch on every
 * render. A long psalm meant ~350 of them and whole-book mode several
 * thousand, each realized and negotiated into the text view's line
 * layout, which dominated the cost of opening a chapter.
 *
 * It is now a tagged glyph carrying the very URL the button's
 * "clicked" handler used to build, so the click is served by
 * on_button_press()'s existing href path, the hand cursor by
 * on_motion(), and nothing is allocated per verse beyond the link tag
 * itself (which load_html() now reclaims -- see clear_load_tags()).
 * Colors match what on_vtools_icon_draw() painted: muted by default,
 * gold where the margin stripe used to mark "this verse has a note". */
static void
insert_verse_tools_chip(ParseCtx *ctx, const char *key, gboolean has_note)
{
	gchar *saved_href, *saved_fg;
	guint saved_small;

	if (!key || !*key)
		return;

	saved_href = ctx->st.href;
	saved_fg = ctx->st.fg;
	saved_small = ctx->st.small;

	ctx->st.href = g_strdup_printf(
	    "passagestudy.jsp?action=verseTools&value=%s", key);
	ctx->st.fg = g_strdup(has_note
				  ? (settings.darktheme ? "#E6C989" : "#8A6D1E")
				  : (settings.darktheme ? "#8A8378" : "#7A736A"));
	ctx->st.small = TRUE;

	insert_text(ctx, "\xE2\x98\xB0");	/* U+2630 TRIGRAM FOR HEAVEN */

	g_free(ctx->st.href);
	g_free(ctx->st.fg);
	ctx->st.href = saved_href;
	ctx->st.fg = saved_fg;
	ctx->st.small = saved_small;

	insert_text(ctx, " ");
}

/* Width actually available to a child anchored in the text: the
 * allocation minus the view's own margins. Sizing children from the
 * bare allocation makes them overflow whenever the margins are wide --
 * reading mode centres a column with margins of ~490px a side, so a
 * separator sized to the full 1718px allocation reached 2183px, the
 * buffer grew wider than the viewport, and the horizontal scrollbar it
 * created dragged the whole text leftwards as soon as anything
 * scrolled it. Whole-book rendering made it constant: one <hr> after
 * every chapter. */
static gint
child_avail_width(GtkWidget *view, GdkRectangle *alloc)
{
	gint w = alloc->width;

	if (GTK_IS_TEXT_VIEW(view))
		w -= gtk_text_view_get_left_margin(GTK_TEXT_VIEW(view)) +
		     gtk_text_view_get_right_margin(GTK_TEXT_VIEW(view));
	return w;
}

static void
il_table_fit(GtkWidget *view, GdkRectangle *alloc, GtkWidget *box)
{
	gint w, cur = -1;

	w = child_avail_width(view, alloc) - 52;
	if (w < 300)
		w = 300;
	gtk_widget_get_size_request(box, &cur, NULL);
	if (cur != w)
		gtk_widget_set_size_request(box, w, -1);
}

static void
insert_il_table(ParseCtx *ctx, const char *key)
{
	GtkTextIter iter;
	GtkTextChildAnchor *anchor;
	GtkWidget *box;
	GtkAllocation alloc;

	if (!ctx->html || !ctx->html->priv->view || !key || !*key)
		return;
	insert_break(ctx, TRUE);
	gtk_text_buffer_get_end_iter(ctx->buf, &iter);
	anchor = gtk_text_buffer_create_child_anchor(ctx->buf, &iter);
	box = gui_interlineal_tabla_widget(key);
	if (!box)
		return;
	gtk_text_view_add_child_at_anchor(ctx->html->priv->view, box, anchor);
	g_signal_connect_object(GTK_WIDGET(ctx->html->priv->view), "size-allocate",
				G_CALLBACK(il_table_fit), box, 0);
	gtk_widget_get_allocation(GTK_WIDGET(ctx->html->priv->view), &alloc);
	if (alloc.width > 80)
		il_table_fit(GTK_WIDGET(ctx->html->priv->view), &alloc, box);
	gtk_widget_show_all(box);
	ctx->at_line_start = FALSE;
	insert_break(ctx, TRUE);
}

/* <hr> keeps the width of an inserted GtkSeparator synced to the text
 * view -- a child-anchor widget doesn't stretch on its own the way a
 * block element would (same trick insert_il_table() uses above). */
static void
hr_fit(GtkWidget *view, GdkRectangle *alloc, GtkWidget *sep)
{
	gint w = child_avail_width(view, alloc) - 28;

	if (w < 20)
		w = 20;
	gtk_widget_set_size_request(sep, w, -1);
}

/* Shared by insert_hr() below and build_hr_cell() further down (a bare
 * <hr> that is a <table> cell's only content gets the same lean
 * separator, not a whole nested page -- see the comment on
 * cell_is_bare_hr()). */
static void
style_hr_separator(GtkWidget *sep, const char *color)
{
	gtk_widget_set_valign(sep, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top(sep, 3);
	gtk_widget_set_margin_bottom(sep, 3);
	if (color && *color) {
		GtkCssProvider *css = gtk_css_provider_new();
		gchar *rule = g_strdup_printf(
		    "separator { background-color: %s; min-height: 1px; }",
		    color);
		gtk_css_provider_load_from_data(css, rule, -1, NULL);
		gtk_style_context_add_provider(
		    gtk_widget_get_style_context(sep),
		    GTK_STYLE_PROVIDER(css),
		    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_free(rule);
		g_object_unref(css);
	}
}

/* Antes, <hr> se trataba como un salto de línea sin trazo (ver
 * insert_break() más arriba): el HTML que arma la app para tablas
 * comparativas no tenía forma de dibujar una raya real entre bloques.
 * Un GtkSeparator insertado como child-anchor sí la dibuja. Si el
 * <hr> trae color/bgcolor explícito (o, si no, el color de texto
 * heredado del contexto) se usa como color de la raya vía un
 * GtkCssProvider de ese widget puntual; si no hay ninguno, se deja el
 * separador con el estilo por defecto del tema. */
static void
insert_hr(ParseCtx *ctx, const char *color_attr)
{
	GtkTextIter iter;
	GtkTextChildAnchor *anchor;
	GtkWidget *sep;
	GtkAllocation alloc;
	const char *color = (color_attr && *color_attr) ? color_attr : ctx->st.fg;

	if (ctx->skip)
		return;
	if (!ctx->html || !ctx->html->priv->view)
		return;

	insert_break(ctx, TRUE);
	gtk_text_buffer_get_end_iter(ctx->buf, &iter);
	anchor = gtk_text_buffer_create_child_anchor(ctx->buf, &iter);

	sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	style_hr_separator(sep, color);

	gtk_text_view_add_child_at_anchor(ctx->html->priv->view, sep, anchor);
	g_signal_connect_object(GTK_WIDGET(ctx->html->priv->view), "size-allocate",
				G_CALLBACK(hr_fit), sep, 0);
	gtk_widget_get_allocation(GTK_WIDGET(ctx->html->priv->view), &alloc);
	if (alloc.width > 28)
		hr_fit(GTK_WIDGET(ctx->html->priv->view), &alloc, sep);
	gtk_widget_show(sep);
	ctx->at_line_start = FALSE;
	insert_break(ctx, TRUE);
}

/* Real <table> support: unlike every other tag, a table's subtree is NOT
 * walked by the normal recursive walk_node() -- it needs actual columns,
 * which a linear GtkTextBuffer can't give it. Each <td>/<th> becomes its
 * own nested WkHtml instance (own buffer, own tag table, own click
 * handling -- on_button_press()/on_motion() already call
 * main_url_handler() directly, so links inside cells work with zero
 * extra wiring) laid out in a GtkGrid, which is then anchored into the
 * parent buffer exactly like insert_hr()/insert_il_table() above. */

static void
collect_rows(xmlNode *node, GPtrArray *rows)
{
	xmlNode *c;

	for (c = node ? node->children : NULL; c; c = c->next) {
		if (c->type != XML_ELEMENT_NODE)
			continue;
		if (!g_ascii_strcasecmp(el_name(c), "tr"))
			g_ptr_array_add(rows, c);
		else if (!g_ascii_strcasecmp(el_name(c), "thead") ||
			 !g_ascii_strcasecmp(el_name(c), "tbody") ||
			 !g_ascii_strcasecmp(el_name(c), "tfoot"))
			collect_rows(c, rows);
	}
}

static void
row_cells(xmlNode *row, GPtrArray *cells)
{
	xmlNode *c;

	for (c = row ? row->children : NULL; c; c = c->next) {
		if (c->type != XML_ELEMENT_NODE)
			continue;
		if (!g_ascii_strcasecmp(el_name(c), "td") ||
		    !g_ascii_strcasecmp(el_name(c), "th"))
			g_ptr_array_add(cells, c);
	}
}

/* Re-serialize a cell's children back to an HTML fragment so it can be
 * fed to a fresh WkHtml through the normal open/write/close stream --
 * this reuses the *entire* existing parser (colors, bold, links, RTL
 * spans...) for cell content instead of duplicating any of it. */
static gchar *
serialize_children(xmlNode *cell)
{
	xmlBufferPtr buf = xmlBufferCreate();
	GString *out = g_string_new(NULL);
	xmlNode *c;
	gchar *result;

	for (c = cell ? cell->children : NULL; c; c = c->next) {
		xmlBufferEmpty(buf);
		xmlNodeDump(buf, cell->doc, c, 0, 0);
		if (xmlBufferLength(buf) > 0)
			g_string_append(out, (const char *)xmlBufferContent(buf));
	}
	xmlBufferFree(buf);
	result = g_string_free(out, FALSE);
	return result;
}

/* wk_html_init() always wraps priv->view in its own GtkScrolledWindow --
 * fine for a full page, wrong for a table cell, which should size to its
 * content like any other child-anchor widget (same reasoning as
 * hr_fit()/il_table_fit() above). Unwrap it once, right after
 * construction. */
static void
make_cell_inline(WkHtml *html)
{
	WkHtmlPrivate *priv = html->priv;
	GtkWidget *view = GTK_WIDGET(priv->view);

	g_object_ref(view);
	gtk_container_remove(GTK_CONTAINER(priv->scroll), view);
	gtk_widget_destroy(priv->scroll);
	priv->scroll = NULL;
	gtk_box_pack_start(GTK_BOX(html), view, TRUE, TRUE, 0);
	g_object_unref(view);
	gtk_widget_set_vexpand(view, FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(html)),
				    "elim-table-cell");
}

/* A <tr><td><hr></td></tr>-only row (how the app draws a divider between
 * stacked blocks inside a single-column table, e.g. lectura_sync.cc/
 * parallel_view.cc between two compared versions) has no real content --
 * wrapping it in a whole nested WkHtml page would stack that page's own
 * view margins (8px top+bottom) with insert_hr()'s own paragraph breaks
 * around the separator, ballooning "a thin line" into a large blank gap.
 * Detect that case and hand back the bare <hr> node so the caller can
 * build a lean separator instead of a full cell. */
static gboolean
cell_is_bare_hr(xmlNode *cell_node, xmlNode **hr_out)
{
	xmlNode *only = NULL;
	xmlNode *c;

	for (c = cell_node ? cell_node->children : NULL; c; c = c->next) {
		if (c->type == XML_TEXT_NODE || c->type == XML_CDATA_SECTION_NODE) {
			const char *p = (const char *)c->content;
			while (p && *p && g_ascii_isspace((guchar)*p))
				p++;
			if (p && *p)
				return FALSE;
			continue;
		}
		if (c->type != XML_ELEMENT_NODE)
			continue;
		if (only || g_ascii_strcasecmp(el_name(c), "hr"))
			return FALSE;
		only = c;
	}
	if (!only)
		return FALSE;
	*hr_out = only;
	return TRUE;
}

static GtkWidget *
build_hr_cell(xmlNode *hr_node)
{
	GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	char *color = el_prop(hr_node, "color");

	if (!color)
		color = el_prop(hr_node, "bgcolor");
	style_hr_separator(sep, color);
	g_free(color);
	return sep;
}

/* fallback_bg is the color a cell falls back to when neither it nor its
 * <tr> specify one -- normally the page's own <body bgcolor>, so a cell
 * matches the surrounding pane instead of the raw GTK theme background. */
static GtkWidget *
build_table_cell(xmlNode *cell_node, const char *fallback_bg)
{
	WkHtml *cell;
	gchar *inner, *aligned = NULL, *body_open, *wrapped;
	char *own_bg, *align;
	const char *bg;

	cell = wk_html_new(NULL, FALSE, -1);
	make_cell_inline(cell);

	own_bg = el_prop(cell_node, "bgcolor");
	bg = (own_bg && *own_bg) ? own_bg : fallback_bg;
	align = el_prop(cell_node, "align");

	inner = serialize_children(cell_node);
	if (align && (!g_ascii_strcasecmp(align, "right") ||
		     !g_ascii_strcasecmp(align, "center"))) {
		aligned = g_strdup_printf("<div style=\"text-align:%s\">%s</div>",
					 align, inner);
	}
	body_open = (bg && *bg) ? g_strdup_printf("<body bgcolor=\"%s\">", bg)
			       : g_strdup("<body>");
	wrapped = g_strdup_printf(
	    "<html><head><meta charset=\"utf-8\"/></head>%s%s</body></html>",
	    body_open, aligned ? aligned : inner);

	wk_html_open_stream(cell, "text/html");
	wk_html_write(cell, wrapped, (gint)strlen(wrapped));
	wk_html_close(cell);

	g_free(wrapped);
	g_free(body_open);
	g_free(aligned);
	g_free(inner);
	g_free(own_bg);
	g_free(align);

	gtk_widget_set_valign(GTK_WIDGET(cell), GTK_ALIGN_START);
	return GTK_WIDGET(cell);
}

/* Same width-sync trick as hr_fit()/il_table_fit(): a child-anchor grid
 * doesn't stretch on its own, so column widths (from % width="" on cells,
 * evenly split otherwise) are recomputed on every resize of the page. */
static void
table_fit(GtkWidget *view, GdkRectangle *alloc, GtkWidget *grid)
{
	gdouble *pct;
	gint ncols, avail, col;
	GList *children, *l;

	pct = g_object_get_data(G_OBJECT(grid), "elim-col-pct");
	ncols = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(grid), "elim-ncols"));
	if (!pct || ncols <= 0)
		return;
	avail = child_avail_width(view, alloc) - 52 - (ncols - 1) * 6;
	if (avail < ncols * 40)
		avail = ncols * 40;
	children = gtk_container_get_children(GTK_CONTAINER(grid));
	for (l = children; l; l = l->next) {
		GtkWidget *cell = GTK_WIDGET(l->data);
		gint w;
		col = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cell), "elim-col"));
		if (col < 0 || col >= ncols)
			continue;
		w = (gint)(avail * pct[col] / 100.0);
		if (w < 30)
			w = 30;
		gtk_widget_set_size_request(cell, w, -1);
	}
	g_list_free(children);
}

static void
insert_table(ParseCtx *ctx, xmlNode *table_node)
{
	GPtrArray *rows;
	GtkTextIter iter;
	GtkTextChildAnchor *anchor;
	GtkWidget *grid;
	GtkAllocation alloc;
	gdouble *pct;
	gint ncols = 0, r;

	if (ctx->skip)
		return;
	if (!ctx->html || !ctx->html->priv->view)
		return;

	rows = g_ptr_array_new();
	collect_rows(table_node, rows);
	if (rows->len == 0) {
		g_ptr_array_free(rows, TRUE);
		return;
	}

	for (r = 0; r < (gint)rows->len; r++) {
		GPtrArray *cells = g_ptr_array_new();
		row_cells(g_ptr_array_index(rows, r), cells);
		if ((gint)cells->len > ncols)
			ncols = (gint)cells->len;
		g_ptr_array_free(cells, TRUE);
	}
	if (ncols == 0) {
		g_ptr_array_free(rows, TRUE);
		return;
	}

	pct = g_new(gdouble, ncols);
	for (r = 0; r < ncols; r++)
		pct[r] = 100.0 / ncols;
	for (r = 0; r < (gint)rows->len; r++) {
		GPtrArray *cells = g_ptr_array_new();
		gint c;
		row_cells(g_ptr_array_index(rows, r), cells);
		for (c = 0; c < (gint)cells->len; c++) {
			char *w = el_prop(g_ptr_array_index(cells, c), "width");
			if (w && *w) {
				gdouble v = g_ascii_strtod(w, NULL);
				if (v > 0)
					pct[c] = v;
			}
			g_free(w);
		}
		g_ptr_array_free(cells, TRUE);
	}

	insert_break(ctx, TRUE);

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_widget_set_hexpand(grid, TRUE);
	g_object_set_data(G_OBJECT(grid), "elim-ncols", GINT_TO_POINTER(ncols));
	g_object_set_data_full(G_OBJECT(grid), "elim-col-pct", pct, g_free);

	for (r = 0; r < (gint)rows->len; r++) {
		xmlNode *row_node = g_ptr_array_index(rows, r);
		char *row_bg = el_prop(row_node, "bgcolor");
		const char *fallback_bg = (row_bg && *row_bg) ? row_bg : ctx->body_bg;
		GPtrArray *cells = g_ptr_array_new();
		gint c;

		row_cells(row_node, cells);
		for (c = 0; c < (gint)cells->len; c++) {
			xmlNode *cell_node = g_ptr_array_index(cells, c);
			xmlNode *hr_node = NULL;
			GtkWidget *cell_w = cell_is_bare_hr(cell_node, &hr_node)
						? build_hr_cell(hr_node)
						: build_table_cell(cell_node, fallback_bg);
			g_object_set_data(G_OBJECT(cell_w), "elim-col", GINT_TO_POINTER(c));
			gtk_grid_attach(GTK_GRID(grid), cell_w, c, r, 1, 1);
		}
		g_ptr_array_free(cells, TRUE);
		g_free(row_bg);
	}
	g_ptr_array_free(rows, TRUE);

	gtk_text_buffer_get_end_iter(ctx->buf, &iter);
	anchor = gtk_text_buffer_create_child_anchor(ctx->buf, &iter);
	gtk_text_view_add_child_at_anchor(ctx->html->priv->view, grid, anchor);
	g_signal_connect_object(GTK_WIDGET(ctx->html->priv->view), "size-allocate",
				G_CALLBACK(table_fit), grid, 0);
	gtk_widget_get_allocation(GTK_WIDGET(ctx->html->priv->view), &alloc);
	if (alloc.width > 80)
		table_fit(GTK_WIDGET(ctx->html->priv->view), &alloc, grid);
	gtk_widget_show_all(grid);
	ctx->at_line_start = FALSE;
	insert_break(ctx, TRUE);
}

static void
walk_element(ParseCtx *ctx, xmlNode *node)
{
	const char *name = el_name(node);
	Style saved;
	char *href = NULL, *aname = NULL, *klass = NULL, *style = NULL;
	char *color = NULL, *bgcolor = NULL, *dir = NULL, *hlid = NULL;
	char *size = NULL;
	gboolean skip_saved = ctx->skip;

	if (!g_ascii_strcasecmp(name, "script") ||
	    !g_ascii_strcasecmp(name, "style") ||
	    !g_ascii_strcasecmp(name, "head") ||
	    !g_ascii_strcasecmp(name, "meta") ||
	    !g_ascii_strcasecmp(name, "title") ||
	    !g_ascii_strcasecmp(name, "link")) {
		return;
	}
	if (!g_ascii_strcasecmp(name, "br")) {
		insert_break(ctx, FALSE);
		return;
	}
	if (!g_ascii_strcasecmp(name, "hr")) {
		char *hr_color = el_prop(node, "color");
		if (!hr_color)
			hr_color = el_prop(node, "bgcolor");
		insert_hr(ctx, hr_color);
		g_free(hr_color);
		return;
	}
	if (!g_ascii_strcasecmp(name, "table")) {
		insert_table(ctx, node);
		return;
	}

	saved = style_copy(&ctx->st);
	href = el_prop(node, "href");
	aname = el_prop(node, "name");
	klass = el_prop(node, "class");
	style = el_prop(node, "style");
	color = el_prop(node, "color");
	bgcolor = el_prop(node, "bgcolor");
	dir = el_prop(node, "dir");
	hlid = el_prop(node, "data-hl-id");
	size = el_prop(node, "size");

	if (aname)
		place_anchor(ctx, aname);
	if (href) {
		g_free(ctx->st.href);
		ctx->st.href = g_strdup(href);
	}
	if (style)
		parse_style_attr(style, &ctx->st);
	if (color) {
		g_free(ctx->st.fg);
		ctx->st.fg = g_strdup(color);
	}
	if (bgcolor) {
		g_free(ctx->st.bg);
		ctx->st.bg = g_strdup(bgcolor);
	}
	if (hlid) {
		g_free(ctx->st.hl_id);
		ctx->st.hl_id = g_strdup(hlid);
	}
	if (class_has(klass, "st") || class_has(klass, "strongs") ||
	    class_has(klass, "gl")) {
		if (!settings.show_interlineal) {
			ctx->skip = TRUE;
		} else {
			ctx->st.sup = TRUE;
			if (!ctx->st.fg)
				ctx->st.fg = g_strdup("#6B2D8B");
		}
	}
	if (class_has(klass, "ilblock") || class_has(klass, "illabel") ||
	    class_has(klass, "ilorig") || class_has(klass, "ilw"))
		ctx->st.ilblock = TRUE;
	if (class_has(klass, "illabel"))
		ctx->st.illabel = TRUE;
	if (class_has(klass, "modh") || class_has(klass, "cur") ||
	    class_has(klass, "miss"))
		ctx->st.para_bg = TRUE;
	if (class_has(klass, "ilorig") || class_has(klass, "ilw")) {
		ctx->st.ilorig = TRUE;
		if (dir && !g_ascii_strcasecmp(dir, "rtl"))
			ctx->st.ilrtl = TRUE;
	}
	if (class_has(klass, "xiphos-hl") && hlid) {
		g_free(ctx->st.hl_id);
		ctx->st.hl_id = g_strdup(hlid);
		ctx->st.underline = TRUE;
	}
	if (class_has(klass, "orig") || class_has(klass, "num"))
		ctx->st.big = TRUE, ctx->st.bold = TRUE;
	if (class_has(klass, "tr") || class_has(klass, "introMaterial"))
		ctx->st.italic = TRUE;
	if (!g_ascii_strcasecmp(name, "b") || !g_ascii_strcasecmp(name, "strong"))
		ctx->st.bold = TRUE;
	if (!g_ascii_strcasecmp(name, "i") || !g_ascii_strcasecmp(name, "em"))
		ctx->st.italic = TRUE;
	if (!g_ascii_strcasecmp(name, "u"))
		ctx->st.underline = TRUE;
	if (!g_ascii_strcasecmp(name, "s") || !g_ascii_strcasecmp(name, "strike"))
		ctx->st.strike = TRUE;
	if (!g_ascii_strcasecmp(name, "sup"))
		ctx->st.sup = TRUE;
	if (!g_ascii_strcasecmp(name, "sub"))
		ctx->st.sub = TRUE;
	if (!g_ascii_strcasecmp(name, "small"))
		ctx->st.small = TRUE;
	if (!g_ascii_strcasecmp(name, "big") ||
	    !g_ascii_strcasecmp(name, "h1") || !g_ascii_strcasecmp(name, "h2"))
		ctx->st.big = TRUE, ctx->st.bold = TRUE;
	if (!g_ascii_strcasecmp(name, "h3") || !g_ascii_strcasecmp(name, "h4"))
		ctx->st.bold = TRUE;
	if (!g_ascii_strcasecmp(name, "center"))
		ctx->st.center = TRUE;
	if (size) {
		int n = atoi(size);
		if (n >= 5 || (size[0] == '+' && n >= 2))
			ctx->st.big = TRUE;
		if (n > 0 && n <= 2)
			ctx->st.small = TRUE;
	}

	if (!g_ascii_strcasecmp(name, "body")) {
		if (ctx->st.bg) {
			g_free(ctx->body_bg);
			ctx->body_bg = g_strdup(ctx->st.bg);
		} else if (bgcolor) {
			g_free(ctx->body_bg);
			ctx->body_bg = g_strdup(bgcolor);
		}
		if (ctx->st.fg) {
			g_free(ctx->body_fg);
			ctx->body_fg = g_strdup(ctx->st.fg);
		}
		if (dir && !g_ascii_strcasecmp(dir, "rtl"))
			gtk_widget_set_direction(GTK_WIDGET(ctx->html->priv->view),
						 GTK_TEXT_DIR_RTL);
	}

	if (is_block(name))
		insert_break(ctx, !g_ascii_strcasecmp(name, "p") ||
				      g_str_has_prefix(name, "h"));
	if (!g_ascii_strcasecmp(name, "td") || !g_ascii_strcasecmp(name, "th"))
		insert_text(ctx, "  ");
	if (!g_ascii_strcasecmp(name, "li"))
		insert_text(ctx, "• ");

	if (class_has(klass, "iltable")) {
		char *key = el_prop(node, "data-key");
		insert_il_table(ctx, key);
		g_free(key);
	} else if (class_has(klass, "vtools")) {
		char *key = el_prop(node, "data-key");
		char *has_note = el_prop(node, "data-has-note");
		insert_verse_tools_chip(ctx, key, has_note && *has_note);
		g_free(key);
		g_free(has_note);
	} else {
		gboolean rtl_span = dir && !g_ascii_strcasecmp(dir, "rtl") &&
				    g_ascii_strcasecmp(name, "body");
		xmlNode *c;
		if (rtl_span)
			insert_text(ctx, "\xE2\x80\xAB"); /* RLE */
		for (c = node->children; c; c = c->next)
			walk_node(ctx, c);
		if (rtl_span)
			insert_text(ctx, "\xE2\x80\xAC"); /* PDF */
	}

	if (is_block(name))
		insert_break(ctx, FALSE);

	style_clear(&ctx->st);
	ctx->st = saved;
	ctx->skip = skip_saved;
	g_free(href);
	g_free(aname);
	g_free(klass);
	g_free(style);
	g_free(color);
	g_free(bgcolor);
	g_free(dir);
	g_free(hlid);
	g_free(size);
}

static void
walk_node(ParseCtx *ctx, xmlNode *node)
{
	if (!node)
		return;
	if (node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE) {
		insert_text(ctx, (const char *)node->content);
		return;
	}
	if (node->type == XML_ELEMENT_NODE)
		walk_element(ctx, node);
}

/* Drops the anchor records without touching the buffer. Used at
 * finalize, where the GtkTextView and its buffer have already been
 * torn down by GTK's container teardown: the marks died with the
 * buffer, so reaching for them there is a use-after-free (it shows up
 * as "gtk_text_mark_get_deleted: assertion 'GTK_IS_TEXT_MARK (mark)'
 * failed"). Long invisible because the main panes live for the whole
 * session; a table cell, which is a WkHtml destroyed and rebuilt on
 * every render, hits it on any document whose cells carry <a name=>. */
static void
free_anchor_list(WkHtmlPrivate *priv)
{
	guint i;
	if (!priv->anchor_list)
		return;
	for (i = 0; i < priv->anchor_list->len; i++) {
		Anchor *a = g_ptr_array_index(priv->anchor_list, i);
		g_free(a->name);
		g_free(a);
	}
	g_ptr_array_set_size(priv->anchor_list, 0);
	if (priv->anchor_ht)
		g_hash_table_remove_all(priv->anchor_ht);
}

static void
links_reset(WkHtmlPrivate *priv)
{
	guint i;

	if (!priv->links)
		return;
	for (i = 0; i < priv->links->len; i++)
		g_free(g_array_index(priv->links, Link, i).href);
	g_array_set_size(priv->links, 0);
}

/* Same, for a live buffer: the marks have to come out of it too,
 * otherwise they pile up across reloads. */
static void
clear_anchors(WkHtmlPrivate *priv)
{
	guint i;
	if (!priv->anchor_list)
		return;
	for (i = 0; i < priv->anchor_list->len; i++) {
		Anchor *a = g_ptr_array_index(priv->anchor_list, i);
		if (a->mark && GTK_IS_TEXT_MARK(a->mark) &&
		    !gtk_text_mark_get_deleted(a->mark))
			gtk_text_buffer_delete_mark(priv->buffer, a->mark);
	}
	free_anchor_list(priv);
}

static void
collect_hl_tag(GtkTextTag *tag, gpointer data)
{
	gchar *name = NULL;

	g_object_get(G_OBJECT(tag), "name", &name, NULL);
	if (name && g_str_has_prefix(name, "hl:"))
		g_ptr_array_add((GPtrArray *)data, tag);
	g_free(name);
}

/* Tags created while parsing one document: one per distinct href (see
 * apply_style_tags()) plus one per highlight id (hl_tag()). Both were
 * only ever dropped at finalize, so a session that browsed a few
 * chapters kept every link tag of every verse it had ever rendered
 * alive in a single tag table -- tens of thousands of GtkTextTag
 * objects after a long read, each one a GObject carrying a full
 * GtkTextAttributes. The buffer's text is already gone by the time we
 * run, so nothing references them any more; the next parse recreates
 * exactly the ones this document needs.
 *
 * Link tags no longer exist -- an href is recorded in priv->links by
 * offset instead of getting a tag of its own -- so only the highlight
 * ones are left to reclaim here. */
static void
clear_load_tags(WkHtmlPrivate *priv)
{
	GtkTextTagTable *table = gtk_text_buffer_get_tag_table(priv->buffer);
	GPtrArray *stale;
	guint i;

	/* Highlight tags are not tracked anywhere, so sweep the table by
	 * name prefix. Collect first: the table must not be modified from
	 * inside gtk_text_tag_table_foreach(). */
	stale = g_ptr_array_new();
	gtk_text_tag_table_foreach(table, collect_hl_tag, stale);
	for (i = 0; i < stale->len; i++)
		gtk_text_tag_table_remove(table,
					  GTK_TEXT_TAG(g_ptr_array_index(stale, i)));
	g_ptr_array_free(stale, TRUE);
}

static void
apply_body_colors(WkHtml *html, const char *bg, const char *fg)
{
	gchar *css;
	WkHtmlPrivate *priv = html->priv;

	if (!priv->css)
		return;
	css = g_strdup_printf(
	    "textview, textview text { background-color: %s; color: %s; }",
	    (bg && *bg) ? bg : "@theme_base_color",
	    (fg && *fg) ? fg : "@theme_text_color");
	gtk_css_provider_load_from_data(priv->css, css, -1, NULL);
	g_free(css);
}

static void
load_html(WkHtml *html)
{
	htmlDocPtr doc;
	ParseCtx ctx;
	xmlNode *root;
	WkHtmlPrivate *priv = html->priv;
	GtkTextIter start, end;

	ensure_stock_tags(priv->buffer);
	clear_anchors(priv);
	gtk_text_buffer_get_bounds(priv->buffer, &start, &end);
	gtk_text_buffer_delete(priv->buffer, &start, &end);
	/* after the delete: no text refers to these tags any more. */
	clear_load_tags(priv);
	links_reset(priv);

	if (!priv->content)
		return;

	memset(&ctx, 0, sizeof(ctx));
	ctx.html = html;
	ctx.buf = priv->buffer;
	ctx.at_line_start = TRUE;
	ctx.st.scale = 1.0;

	doc = htmlReadMemory(priv->content, (int)strlen(priv->content),
			     "file://", "UTF-8",
			     HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
				 HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
	if (!doc) {
		gtk_text_buffer_set_text(priv->buffer, priv->content, -1);
		return;
	}
	root = xmlDocGetRootElement(doc);
	walk_node(&ctx, root);
	xmlFreeDoc(doc);

	apply_body_colors(html, ctx.body_bg, ctx.body_fg);
	style_clear(&ctx.st);
	g_free(ctx.body_bg);
	g_free(ctx.body_fg);
	/* Do not jump here: priv->anchor is still the previous verse
	 * until HtmlOutput() calls wk_html_jump_to_anchor() after close. */
}

static gchar *
iter_href(WkHtml *html, const GtkTextIter *iter)
{
	GArray *links;
	gint off, lo, hi;

	if (!html || !WK_HTML_IS_HTML(html))
		return NULL;
	links = html->priv->links;
	if (!links || links->len == 0)
		return NULL;

	off = gtk_text_iter_get_offset(iter);
	lo = 0;
	hi = (gint)links->len - 1;
	while (lo <= hi) {
		gint mid = (lo + hi) / 2;
		Link *l = &g_array_index(links, Link, mid);
		if (off < l->start)
			hi = mid - 1;
		else if (off >= l->end)
			lo = mid + 1;
		else
			return g_strdup(l->href);
	}
	return NULL;
}

static gboolean
iter_at_xy(GtkTextView *view, GdkEventButton *event, GtkTextIter *iter)
{
	gint x, y;
	gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_TEXT,
					      (gint)event->x, (gint)event->y, &x, &y);
	gtk_text_view_get_iter_at_location(view, iter, x, y);
	return TRUE;
}

static gboolean
on_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	WkHtml *html = WK_HTML(data);
	GtkTextIter iter;
	gchar *href;
	gint x, y;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
					      GTK_TEXT_WINDOW_TEXT,
					      (gint)event->x, (gint)event->y, &x, &y);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget), &iter, x, y);
	href = iter_href(html, &iter);
	if (g_strcmp0(html->priv->hover_uri, href) != 0) {
		g_free(html->priv->hover_uri);
		html->priv->hover_uri = href;
		href = NULL;
		g_free(x_uri);
		x_uri = g_strdup(html->priv->hover_uri);
		in_url = html->priv->hover_uri != NULL;
		g_signal_emit(html, signals[URI_SELECTED], 0, html->priv->hover_uri, FALSE);
		{
			GdkWindow *win = gtk_text_view_get_window(GTK_TEXT_VIEW(widget),
								  GTK_TEXT_WINDOW_TEXT);
			GdkCursor *c = gdk_cursor_new_from_name(gdk_window_get_display(win),
								html->priv->hover_uri ? "pointer" : "text");
			gdk_window_set_cursor(win, c);
			if (c)
				g_object_unref(c);
		}
		if (html->priv->hover_uri) {
			if (html->priv->is_dialog)
				main_dialogs_url_handler(html->priv->dialog,
							 html->priv->hover_uri, FALSE);
			else
				main_url_handler(html->priv->hover_uri, FALSE);
		}
	}
	g_free(href);
	return FALSE;
}

static gboolean
on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	WkHtml *html = WK_HTML(data);
	GtkTextIter iter;
	gchar *href;

	if (event->type == GDK_2BUTTON_PRESS) {
		db_click = TRUE;
		return FALSE;
	}
	db_click = FALSE;

	if (event->button == 3) {
		g_signal_emit(html, signals[POPUPMENU_REQUESTED], 0,
			      html->priv->dialog, FALSE);
		return TRUE;
	}
	if (event->button == 2)
		return TRUE;
	if (event->button != 1)
		return FALSE;

	iter_at_xy(GTK_TEXT_VIEW(widget), event, &iter);
	href = iter_href(html, &iter);
	if (href) {
		if (html->priv->is_dialog)
			main_dialogs_url_handler(html->priv->dialog, href, TRUE);
		else
			main_url_handler(href, TRUE);
		g_free(href);
		return TRUE;
	}
	return FALSE;
}

static gboolean
on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	(void)data;
	if (event->type == GDK_BUTTON_RELEASE && db_click) {
		GtkClipboard *clipboard =
		    gtk_widget_get_clipboard(widget, GDK_SELECTION_PRIMARY);
		gtk_clipboard_request_text(clipboard, gui_get_clipboard_text_for_lookup, NULL);
	}
	return FALSE;
}

static void
on_print_draw(GtkPrintOperation *op, GtkPrintContext *context,
	      gint page, gpointer user_data)
{
	cairo_t *cr = gtk_print_context_get_cairo_context(context);
	PangoLayout *layout = user_data;
	gint h;
	gdouble ph = gtk_print_context_get_height(context);

	pango_layout_get_pixel_size(layout, NULL, &h);
	cairo_move_to(cr, 0, -page * (gint)ph);
	pango_cairo_show_layout(cr, layout);
	(void)h;
}

static void
on_print_begin(GtkPrintOperation *op, GtkPrintContext *context, gpointer user_data)
{
	PangoLayout *layout = user_data;
	gdouble ph = gtk_print_context_get_height(context);
	gdouble pw = gtk_print_context_get_width(context);
	gint h, pages;

	pango_layout_set_width(layout, (gint)(pw * PANGO_SCALE));
	pango_layout_get_pixel_size(layout, NULL, &h);
	pages = h > 0 ? (h + (gint)ph - 1) / (gint)ph : 1;
	gtk_print_operation_set_n_pages(op, MAX(pages, 1));
}

WkHtml *
wk_html_create(void)
{
	return WK_HTML(g_object_new(WK_TYPE_HTML, NULL));
}

WkHtml *
wk_html_new(DIALOG_DATA *dialog, gboolean is_dialog, gint pane)
{
	WkHtml *html = wk_html_create();
	html->priv->pane = pane;
	html->priv->is_dialog = is_dialog;
	html->priv->dialog = dialog;
	return html;
}

GtkTextView *
wk_html_get_view(WkHtml *html)
{
	g_return_val_if_fail(WK_HTML_IS_HTML(html), NULL);
	return html->priv->view;
}

gchar *
wk_html_anchor_at(WkHtml *html, const GtkTextIter *iter)
{
	gint off, i;
	WkHtmlPrivate *priv;

	g_return_val_if_fail(WK_HTML_IS_HTML(html), g_strdup("0"));
	priv = html->priv;
	off = gtk_text_iter_get_offset(iter);
	for (i = (int)priv->anchor_list->len - 1; i >= 0; i--) {
		Anchor *a = g_ptr_array_index(priv->anchor_list, i);
		GtkTextIter m;
		if (gtk_text_mark_get_deleted(a->mark))
			continue;
		gtk_text_buffer_get_iter_at_mark(priv->buffer, &m, a->mark);
		if (gtk_text_iter_get_offset(&m) <= off) {
			if (a->name && strcmp(a->name, "0") && strcmp(a->name, "0next") &&
			    strcmp(a->name, "0hdr") && strcmp(a->name, "TOP"))
				return g_strdup(a->name);
		}
	}
	return g_strdup("0");
}

gchar *
wk_html_highlight_id_at(const GtkTextIter *iter)
{
	GSList *tags, *l;
	gchar *id = NULL;
	gchar *name = NULL;

	tags = gtk_text_iter_get_tags(iter);
	for (l = tags; l; l = l->next) {
		g_object_get(l->data, "name", &name, NULL);
		if (name && g_str_has_prefix(name, "hl:")) {
			id = g_strdup(name + 3);
			g_free(name);
			break;
		}
		g_free(name);
		name = NULL;
	}
	g_slist_free(tags);
	return id;
}

static GtkTextTag *
hl_tag(WkHtml *html, const gchar *id, const gchar *color, gboolean create)
{
	gchar *name;
	GtkTextTagTable *table;
	GtkTextTag *tag;

	if (!id || !*id)
		return NULL;
	name = g_strdup_printf("hl:%s", id);
	table = gtk_text_buffer_get_tag_table(html->priv->buffer);
	tag = gtk_text_tag_table_lookup(table, name);
	if (!tag && create)
		tag = gtk_text_buffer_create_tag(html->priv->buffer, name,
						 "background", color ? color : "#FFEB3B",
						 "underline", PANGO_UNDERLINE_SINGLE,
						 NULL);
	else if (tag && color)
		g_object_set(tag, "background", color, NULL);
	g_free(name);
	return tag;
}

void
wk_html_highlight_apply(WkHtml *html, GtkTextIter *start, GtkTextIter *end,
			const gchar *id, const gchar *color)
{
	GtkTextTag *tag;
	g_return_if_fail(WK_HTML_IS_HTML(html));
	tag = hl_tag(html, id, color, TRUE);
	if (tag)
		gtk_text_buffer_apply_tag(html->priv->buffer, tag, start, end);
}

void
wk_html_highlight_set_color(WkHtml *html, const gchar *id, const gchar *color)
{
	g_return_if_fail(WK_HTML_IS_HTML(html));
	hl_tag(html, id, color, TRUE);
}

void
wk_html_highlight_remove(WkHtml *html, const gchar *id)
{
	GtkTextTag *tag;
	GtkTextIter s, e;
	g_return_if_fail(WK_HTML_IS_HTML(html));
	tag = hl_tag(html, id, NULL, FALSE);
	if (!tag)
		return;
	gtk_text_buffer_get_bounds(html->priv->buffer, &s, &e);
	gtk_text_buffer_remove_tag(html->priv->buffer, tag, &s, &e);
}

gboolean
wk_html_highlight_bounds(WkHtml *html, const gchar *id,
			 GtkTextIter *start, GtkTextIter *end)
{
	GtkTextTag *tag;
	g_return_val_if_fail(WK_HTML_IS_HTML(html), FALSE);
	tag = hl_tag(html, id, NULL, FALSE);
	if (!tag)
		return FALSE;
	gtk_text_buffer_get_start_iter(html->priv->buffer, start);
	if (!gtk_text_iter_forward_to_tag_toggle(start, tag) &&
	    !gtk_text_iter_starts_tag(start, tag)) {
		gtk_text_buffer_get_start_iter(html->priv->buffer, start);
		if (!gtk_text_iter_starts_tag(start, tag))
			return FALSE;
	}
	*end = *start;
	if (!gtk_text_iter_forward_to_tag_toggle(end, tag))
		gtk_text_buffer_get_end_iter(html->priv->buffer, end);
	return TRUE;
}

static gboolean
line_looks_like_heading(GtkTextBuffer *buf, GtkTextIter *pos)
{
	GtkTextIter a;
	GtkTextTag *bold;
	GSList *tags, *l;

	a = *pos;
	gtk_text_iter_set_line_offset(&a, 0);
	while (gtk_text_iter_compare(&a, pos) < 0 &&
	       g_unichar_isspace(gtk_text_iter_get_char(&a)))
		gtk_text_iter_forward_char(&a);

	bold = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buf), "bold");
	if (!bold || !gtk_text_iter_has_tag(&a, bold))
		return FALSE;
	/* A real verse starts with its number as a sword:// link. */
	tags = gtk_text_iter_get_tags(&a);
	for (l = tags; l; l = l->next) {
		if (g_object_get_data(G_OBJECT(l->data), "href")) {
			g_slist_free(tags);
			return FALSE;
		}
	}
	g_slist_free(tags);
	return TRUE;
}

static void
trim_ilblock(GtkTextBuffer *buf, const GtkTextIter *start, GtkTextIter *end)
{
	GtkTextTag *tag;
	GtkTextIter p;

	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buf), "ilblock");
	if (!tag || gtk_text_iter_compare(start, end) >= 0)
		return;
	p = *end;
	if (!gtk_text_iter_backward_char(&p))
		return;
	while (gtk_text_iter_compare(&p, start) > 0 &&
	       g_unichar_isspace(gtk_text_iter_get_char(&p))) {
		if (!gtk_text_iter_backward_char(&p))
			return;
	}
	if (gtk_text_iter_compare(&p, start) < 0)
		return;
	if (!gtk_text_iter_has_tag(&p, tag))
		return;
	if (!gtk_text_iter_backward_to_tag_toggle(&p, tag))
		return;
	if (gtk_text_iter_compare(&p, start) > 0)
		*end = p;
}

static void
trim_trailing_chrome(GtkTextBuffer *buf, const GtkTextIter *start, GtkTextIter *end)
{
	GtkTextIter p;
	gboolean skipped_nl = FALSE;

	(void)buf;
	if (gtk_text_iter_compare(start, end) >= 0)
		return;
	p = *end;
	while (gtk_text_iter_compare(&p, start) > 0) {
		GtkTextIter q = p;
		gunichar c;

		if (!gtk_text_iter_backward_char(&q))
			break;
		c = gtk_text_iter_get_char(&q);
		if (gtk_text_iter_get_child_anchor(&q) ||
		    (g_unichar_isspace(c) && c != '\n')) {
			p = q;
			continue;
		}
		if (c == '\n') {
			if (skipped_nl)
				break;
			skipped_nl = TRUE;
			p = q;
			continue;
		}
		break;
	}
	*end = p;
}

static void
trim_trailing_heading(GtkTextBuffer *buf, const GtkTextIter *start, GtkTextIter *end)
{
	GtkTextIter probe;

	if (gtk_text_iter_compare(start, end) >= 0)
		return;
	probe = *end;
	for (;;) {
		GtkTextIter line0;
		if (!gtk_text_iter_backward_char(&probe))
			return;
		if (gtk_text_iter_compare(&probe, start) <= 0)
			return;
		line0 = probe;
		gtk_text_iter_set_line_offset(&line0, 0);
		if (gtk_text_iter_ends_line(&line0) ||
		    line_looks_like_heading(buf, &line0)) {
			*end = line0;
			probe = line0;
			continue;
		}
		return;
	}
}

gboolean
wk_html_anchor_bounds(WkHtml *html, const gchar *anchor,
		      GtkTextIter *start, GtkTextIter *end)
{
	WkHtmlPrivate *priv;
	guint i;

	g_return_val_if_fail(WK_HTML_IS_HTML(html), FALSE);
	if (!anchor || !*anchor)
		return FALSE;
	priv = html->priv;

	for (i = 0; i < priv->anchor_list->len; i++) {
		Anchor *a = g_ptr_array_index(priv->anchor_list, i);
		if (!a->name || strcmp(a->name, anchor))
			continue;
		if (gtk_text_mark_get_deleted(a->mark))
			return FALSE;
		gtk_text_buffer_get_iter_at_mark(priv->buffer, start, a->mark);
		if (i + 1 < priv->anchor_list->len) {
			Anchor *nxt = g_ptr_array_index(priv->anchor_list, i + 1);
			if (nxt->mark && !gtk_text_mark_get_deleted(nxt->mark)) {
				gtk_text_buffer_get_iter_at_mark(priv->buffer, end, nxt->mark);
				trim_trailing_heading(priv->buffer, start, end);
				trim_ilblock(priv->buffer, start, end);
				trim_trailing_chrome(priv->buffer, start, end);
				return TRUE;
			}
		}
		gtk_text_buffer_get_end_iter(priv->buffer, end);
		trim_trailing_heading(priv->buffer, start, end);
		trim_ilblock(priv->buffer, start, end);
		trim_trailing_chrome(priv->buffer, start, end);
		return TRUE;
	}
	return FALSE;
}

static gboolean
bg_is_dark(const char *hex)
{
	GdkRGBA r;
	if (!hex || !gdk_rgba_parse(&r, hex))
		return TRUE;
	return (0.299 * r.red + 0.587 * r.green + 0.114 * r.blue) < 0.48;
}

static GtkTextTag *
ensure_curverse_tag(GtkTextBuffer *buf)
{
	GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
	GtkTextTag *tag = gtk_text_tag_table_lookup(table, "curverse");
	gboolean dark = bg_is_dark(settings.bible_bg_color);
	/* High-contrast band on the verse characters only (not
	 * paragraph-background: that painted the whole following
	 * section). Gold on dark, navy on light. */
	const char *wash = dark ? "#E6C989" : "#2C4A6E";
	const char *ink = dark ? "#1A1610" : "#F7F4EE";

	if (!tag)
		tag = gtk_text_buffer_create_tag(buf, "curverse",
						 "background", wash,
						 "foreground", ink,
						 "weight", PANGO_WEIGHT_SEMIBOLD,
						 NULL);
	else
		g_object_set(tag,
			     "paragraph-background-set", FALSE,
			     "left-margin-set", FALSE,
			     "background", wash,
			     "background-set", TRUE,
			     "foreground", ink,
			     "foreground-set", TRUE,
			     "weight", PANGO_WEIGHT_SEMIBOLD,
			     NULL);
	return tag;
}

void
wk_html_reading_focus_set(WkHtml *html, GtkTextIter *start, GtkTextIter *end,
			  const gchar *bg_color, const gchar *fg_color)
{
	GtkTextTag *tag;
	GtkTextIter s, e;

	(void)bg_color;
	(void)fg_color;
	g_return_if_fail(WK_HTML_IS_HTML(html));
	tag = ensure_curverse_tag(html->priv->buffer);

	gtk_text_buffer_get_bounds(html->priv->buffer, &s, &e);
	gtk_text_buffer_remove_tag(html->priv->buffer, tag, &s, &e);
	if (start && end) {
		GtkTextTag *il;
		GtkTextIter i;

		gtk_text_buffer_apply_tag(html->priv->buffer, tag, start, end);
		il = gtk_text_tag_table_lookup(
		    gtk_text_buffer_get_tag_table(html->priv->buffer), "ilblock");
		if (!il)
			return;
		i = *start;
		while (gtk_text_iter_compare(&i, end) < 0) {
			if (gtk_text_iter_has_tag(&i, il)) {
				GtkTextIter a = i, b = i;
				if (!gtk_text_iter_forward_to_tag_toggle(&b, il))
					b = *end;
				if (gtk_text_iter_compare(&b, end) > 0)
					b = *end;
				gtk_text_buffer_remove_tag(html->priv->buffer, tag, &a, &b);
				i = b;
			} else if (!gtk_text_iter_forward_to_tag_toggle(&i, il)) {
				break;
			}
		}
	}
}

void
wk_html_enable_caret_browsing(WkHtml *html)
{
	g_return_if_fail(WK_HTML_IS_HTML(html));
	gtk_text_view_set_cursor_visible(html->priv->view, TRUE);
}

static void
html_dispose(GObject *object)
{
	parent_class->dispose(object);
}

static void
html_finalize(GObject *object)
{
	WkHtml *html = WK_HTML(object);
	WkHtmlPrivate *priv = html->priv;

	if (priv->timeout)
		g_source_remove(priv->timeout);
	free_anchor_list(priv);	/* buffer is already gone -- see above */
	if (priv->anchor_list)
		g_ptr_array_free(priv->anchor_list, TRUE);
	if (priv->anchor_ht)
		g_hash_table_destroy(priv->anchor_ht);
	if (priv->links) {
		links_reset(priv);
		g_array_free(priv->links, TRUE);
	}
	g_free(priv->base_uri);
	g_free(priv->anchor);
	g_free(priv->content);
	g_free(priv->mime);
	g_free(priv->find_string);
	g_free(priv->hover_uri);
	if (priv->css)
		g_object_unref(priv->css);
	parent_class->finalize(object);
}

static void
wk_html_init(WkHtml *html)
{
	WkHtmlPrivate *priv;
	GtkStyleContext *ctx;

	html->priv = priv = wk_html_get_instance_private(html);
	memset(priv, 0, sizeof(*priv));
	priv->anchor_ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	priv->anchor_list = g_ptr_array_new();
	priv->links = g_array_new(FALSE, FALSE, sizeof(Link));

	gtk_orientable_set_orientation(GTK_ORIENTABLE(html), GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_hexpand(GTK_WIDGET(html), TRUE);
	gtk_widget_set_vexpand(GTK_WIDGET(html), TRUE);

	priv->view = GTK_TEXT_VIEW(gtk_text_view_new());
	priv->buffer = gtk_text_view_get_buffer(priv->view);
	gtk_text_view_set_wrap_mode(priv->view, GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_editable(priv->view, FALSE);
	gtk_text_view_set_cursor_visible(priv->view, FALSE);
	gtk_text_view_set_left_margin(priv->view, 14);
	gtk_text_view_set_right_margin(priv->view, 14);
	gtk_text_view_set_top_margin(priv->view, 8);
	gtk_text_view_set_bottom_margin(priv->view, 8);
	gtk_text_view_set_pixels_above_lines(priv->view, 1);
	gtk_text_view_set_pixels_below_lines(priv->view, 1);
	gtk_widget_set_name(GTK_WIDGET(priv->view), "elim-html");
	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(priv->view)),
				    "elim-html");

	priv->scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->scroll),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(priv->scroll),
					    GTK_SHADOW_NONE);
	gtk_widget_set_hexpand(priv->scroll, TRUE);
	gtk_widget_set_vexpand(priv->scroll, TRUE);
	gtk_container_add(GTK_CONTAINER(priv->scroll), GTK_WIDGET(priv->view));
	gtk_box_pack_start(GTK_BOX(html), priv->scroll, TRUE, TRUE, 0);

	priv->css = gtk_css_provider_new();
	ctx = gtk_widget_get_style_context(GTK_WIDGET(priv->view));
	gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(priv->css),
				       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_widget_add_events(GTK_WIDGET(priv->view),
			      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
				  GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
	g_signal_connect(priv->view, "button-press-event",
			 G_CALLBACK(on_button_press), html);
	g_signal_connect(priv->view, "button-release-event",
			 G_CALLBACK(on_button_release), html);
	g_signal_connect(priv->view, "motion-notify-event",
			 G_CALLBACK(on_motion), html);

	gtk_widget_show(GTK_WIDGET(priv->view));
	gtk_widget_show(priv->scroll);
}

static void
wk_html_class_init(WkHtmlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = html_finalize;
	object_class->dispose = html_dispose;

	signals[URI_SELECTED] =
	    g_signal_new("uri_selected",
			 G_TYPE_FROM_CLASS(klass),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(WkHtmlClass, uri_selected),
			 NULL, NULL,
			 wk_marshal_VOID__POINTER_BOOLEAN,
			 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	signals[FRAME_SELECTED] =
	    g_signal_new("frame_selected",
			 G_TYPE_FROM_CLASS(klass),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(WkHtmlClass, frame_selected),
			 g_signal_accumulator_true_handled, NULL,
			 wk_marshal_BOOLEAN__POINTER_BOOLEAN,
			 G_TYPE_BOOLEAN, 2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	signals[POPUPMENU_REQUESTED] =
	    g_signal_new("popupmenu_requested",
			 G_TYPE_FROM_CLASS(klass),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(WkHtmlClass, popupmenu_requested),
			 NULL, NULL,
			 wk_marshal_VOID__POINTER_BOOLEAN,
			 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
}

void
wk_html_set_base_uri(WkHtml *html, const gchar *uri)
{
	(void)uri;
	g_return_if_fail(WK_HTML_IS_HTML(html));
	g_free(html->priv->base_uri);
	html->priv->base_uri = g_strdup("file://");
}

void
wk_html_open_stream(WkHtml *html, const gchar *mime)
{
	wk_html_set_base_uri(html, NULL);
	html->priv->frames_enabled = FALSE;
	g_free(html->priv->content);
	html->priv->content = NULL;
	g_free(html->priv->mime);
	html->priv->mime = g_strdup(mime);
}

void
wk_html_write(WkHtml *html, const gchar *data, gint len)
{
	gchar *tmp;
	if (len < 0)
		len = (gint)strlen(data);
	if (html->priv->content) {
		gchar *chunk = g_strndup(data, len);
		tmp = g_strconcat(html->priv->content, chunk, NULL);
		g_free(chunk);
		g_free(html->priv->content);
		html->priv->content = tmp;
	} else
		html->priv->content = g_strndup(data, len);
}

void
wk_html_frames(WkHtml *html, gboolean enable)
{
	html->priv->frames_enabled = enable;
}

void
wk_html_printf(WkHtml *html, char *format, ...)
{
	va_list args;
	gchar *string;

	g_return_if_fail(format != NULL);
	va_start(args, format);
	string = g_strdup_vprintf(format, args);
	va_end(args);
	wk_html_write(html, string, -1);
	g_free(string);
}

void
wk_html_close(WkHtml *html)
{
	html->priv->initialised = TRUE;
	load_html(html);
	g_free(html->priv->content);
	html->priv->content = NULL;
	g_free(html->priv->mime);
	html->priv->mime = NULL;
}

void
wk_html_render_data(WkHtml *html, const char *data, guint32 len)
{
	wk_html_open_stream(html, "text/html");
	wk_html_write(html, data, (gint)len);
	wk_html_close(html);
}

static gboolean
search_buf(WkHtml *html, gboolean forward)
{
	GtkTextIter start, match_s, match_e, a, b;
	GtkTextSearchFlags flags = GTK_TEXT_SEARCH_CASE_INSENSITIVE |
				   GTK_TEXT_SEARCH_TEXT_ONLY;
	gboolean found;

	if (!html->priv->find_string || !*html->priv->find_string)
		return FALSE;

	ensure_stock_tags(html->priv->buffer);
	gtk_text_buffer_get_bounds(html->priv->buffer, &a, &b);
	gtk_text_buffer_remove_tag_by_name(html->priv->buffer, "find-hl", &a, &b);

	gtk_text_buffer_get_iter_at_offset(html->priv->buffer, &start,
					   html->priv->find_offset);
	if (forward)
		found = gtk_text_iter_forward_search(&start, html->priv->find_string,
						     flags, &match_s, &match_e, NULL);
	else
		found = gtk_text_iter_backward_search(&start, html->priv->find_string,
						      flags, &match_s, &match_e, NULL);
	if (!found) {
		if (forward)
			gtk_text_buffer_get_start_iter(html->priv->buffer, &start);
		else
			gtk_text_buffer_get_end_iter(html->priv->buffer, &start);
		if (forward)
			found = gtk_text_iter_forward_search(&start, html->priv->find_string,
							     flags, &match_s, &match_e, NULL);
		else
			found = gtk_text_iter_backward_search(&start, html->priv->find_string,
							      flags, &match_s, &match_e, NULL);
	}
	if (!found)
		return FALSE;
	gtk_text_buffer_apply_tag_by_name(html->priv->buffer, "find-hl", &match_s, &match_e);
	gtk_text_buffer_select_range(html->priv->buffer, &match_s, &match_e);
	gtk_text_view_scroll_to_iter(html->priv->view, &match_s, 0.2, FALSE, 0, 0);
	html->priv->find_offset = gtk_text_iter_get_offset(&match_e);
	return TRUE;
}

gboolean
wk_html_find(WkHtml *html, const gchar *find_string)
{
	g_free(html->priv->find_string);
	html->priv->find_string = g_strdup(find_string);
	html->priv->find_offset = 0;
	ensure_stock_tags(html->priv->buffer);
	{
		GtkTextIter a, b;
		gtk_text_buffer_get_bounds(html->priv->buffer, &a, &b);
		gtk_text_buffer_remove_tag_by_name(html->priv->buffer, "find-hl", &a, &b);
	}
	return search_buf(html, TRUE);
}

gboolean
wk_html_find_again(WkHtml *html, gboolean forward)
{
	return search_buf(html, forward);
}

static void
scroll_to_stored_anchor(WkHtml *html)
{
	WkHtmlPrivate *priv;
	Anchor *a;

	if (!html || !WK_HTML_IS_HTML(html))
		return;
	priv = html->priv;
	if (!priv->anchor || !*priv->anchor || !priv->view)
		return;
	a = g_hash_table_lookup(priv->anchor_ht, priv->anchor);
	if (a && a->mark && !gtk_text_mark_get_deleted(a->mark))
		/* Keep the verse clear of the top chrome (Interlinear /
		 * Comparar strip) instead of pinning it to the edge. */
		gtk_text_view_scroll_to_mark(priv->view, a->mark, 0.0, TRUE, 0.0,
					     READING_FOCUS_YALIGN);
}

static gboolean
scroll_to_stored_anchor_later(gpointer data)
{
	WkHtml *html = WK_HTML(data);

	if (!WK_HTML_IS_HTML(html))
		return G_SOURCE_REMOVE;
	scroll_to_stored_anchor(html);
	if (html->priv->jump_tries > 0) {
		html->priv->jump_tries--;
		return G_SOURCE_CONTINUE;
	}
	html->priv->timeout = 0;
	return G_SOURCE_REMOVE;
}

void
wk_html_ensure_anchor_visible(WkHtml *html, const gchar *anchor)
{
	Anchor *a;

	g_return_if_fail(html != NULL);
	if (!anchor || !*anchor)
		return;
	a = g_hash_table_lookup(html->priv->anchor_ht, anchor);
	if (a && a->mark && !gtk_text_mark_get_deleted(a->mark))
		/* Scroll only if the verse sits in the top/bottom margin
		 * or off-screen — never yank it to a fixed slot.
		 *
		 * Deliberately does NOT touch priv->anchor/timeout/jump_tries
		 * here anymore. Those belong to wk_html_jump_to_anchor()'s own
		 * self-healing retry loop (a sync scroll right after a fresh
		 * render, then 3 retries at 80ms while GtkTextView's layout
		 * settles -- the sync one runs against stale/zero line
		 * heights and is known-unreliable, which is why the retries
		 * exist at all). gui_bibletext_mark_current_verse() calls
		 * this function shortly after every render, for the *same*
		 * anchor jump_to_anchor already targeted; it used to cancel
		 * that pending retry timer, freezing the view at whatever the
		 * first unreliable scroll produced. Harmless most of the
		 * time, but on a last-verse-of-chapter jump -- very little
		 * text left after the mark -- that stale position could land
		 * past all visible text, making the pane look completely
		 * blank. */
		/* Align rather than do the minimal scroll. With
		 * use_align=FALSE this only moved the view when the verse
		 * had fallen off the edge, so walking forward with the
		 * arrows left the page still and the highlight creeping
		 * down it -- measured at 21%, 25%, 28% of the viewport on
		 * successive presses, then a jump when it hit the bottom.
		 * Anchoring it at READING_FOCUS_YALIGN keeps the verse you
		 * are on in the same place, high but not against the top
		 * edge, with a few lines of context above it.
		 *
		 * Safe to always align here because this is the
		 * navigation path: scroll-driven tracking repaints the band
		 * through reapply_current_verse_band(), which deliberately
		 * does not call this. */
		gtk_text_view_scroll_to_mark(html->priv->view, a->mark,
					     0.0, TRUE, 0.0,
					     READING_FOCUS_YALIGN);
}

void
wk_html_jump_to_anchor(WkHtml *html, gchar *anchor)
{
	g_return_if_fail(html != NULL);
	g_free(html->priv->anchor);
	html->priv->anchor = g_strdup(anchor);
	if (html->priv->timeout) {
		g_source_remove(html->priv->timeout);
		html->priv->timeout = 0;
	}
	html->priv->jump_tries = 0;
	if (!anchor || !*anchor)
		return;
	/* Try now, then again while GtkTextView lays out and while the
	 * Comparar split resizes the pane (that shrink happens after the
	 * first scroll and would otherwise leave the verse off-screen). */
	scroll_to_stored_anchor(html);
	html->priv->jump_tries = 3;
	html->priv->timeout = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 80,
						 scroll_to_stored_anchor_later,
						 g_object_ref(html),
						 g_object_unref);
}

void
wk_html_copy_selection(WkHtml *html)
{
	GtkClipboard *cb = gtk_widget_get_clipboard(GTK_WIDGET(html), GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_copy_clipboard(html->priv->buffer, cb);
}

void
wk_html_select_all(WkHtml *html)
{
	GtkTextIter s, e;
	gtk_text_buffer_get_bounds(html->priv->buffer, &s, &e);
	gtk_text_buffer_select_range(html->priv->buffer, &s, &e);
}

void
wk_html_print(WkHtml *html)
{
	GtkPrintOperation *op;
	PangoLayout *layout;
	gchar *text;
	GtkTextIter s, e;

	gtk_text_buffer_get_bounds(html->priv->buffer, &s, &e);
	text = gtk_text_buffer_get_text(html->priv->buffer, &s, &e, FALSE);
	op = gtk_print_operation_new();
	layout = gtk_widget_create_pango_layout(GTK_WIDGET(html->priv->view), text);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	g_signal_connect(op, "begin-print", G_CALLBACK(on_print_begin), layout);
	g_signal_connect(op, "draw-page", G_CALLBACK(on_print_draw), layout);
	gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				NULL, NULL);
	g_object_unref(layout);
	g_object_unref(op);
	g_free(text);
}

gboolean
wk_html_initialize(void)
{
	xmlInitParser();
	return TRUE;
}

void
wk_html_shutdown(void)
{
}
