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
	guint ilblock : 1;
	guint illabel : 1;
	guint ilorig : 1;
	guint ilrtl : 1;
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

static void walk_node(ParseCtx *ctx, xmlNode *node);
static void insert_verse_tools_button(ParseCtx *ctx, const char *key);

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
		WkHtmlPrivate *priv = ctx->html->priv;
		const char *link_fg = st->fg ? st->fg : "#2C4A6E";
		tag = g_hash_table_lookup(priv->link_tags, st->href);
		if (!tag) {
			gchar *nm = g_strdup_printf("a:%u", ++priv->link_seq);
			tag = gtk_text_buffer_create_tag(buf, nm,
							 "foreground", link_fg,
							 "underline", PANGO_UNDERLINE_NONE,
							 NULL);
			g_object_set_data_full(G_OBJECT(tag), "href",
					       g_strdup(st->href), g_free);
			g_hash_table_insert(priv->link_tags, g_strdup(st->href), tag);
			g_free(nm);
		}
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

	norm = g_string_new(NULL);
	for (p = text; *p;) {
		if (*p == '\r') {
			p++;
			continue;
		}
		if (*p == '\n' || *p == '\t') {
			g_string_append_c(norm, ' ');
			p++;
			continue;
		}
		run = p;
		p = g_utf8_next_char(p);
		g_string_append_len(norm, run, p - run);
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

static gboolean
on_vtools_icon_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	GtkWidget *parent = gtk_widget_get_parent(widget);
	GtkStyleContext *ctx = gtk_widget_get_style_context(parent ? parent : widget);
	GdkRGBA fg;
	int w, h;
	double s, ox, oy;

	(void)data;
	gtk_style_context_get_color(ctx, gtk_style_context_get_state(ctx), &fg);
	w = gtk_widget_get_allocated_width(widget);
	h = gtk_widget_get_allocated_height(widget);
	s = (w < h) ? w : h;
	if (s < 1.0)
		return FALSE;
	ox = (w - s) * 0.5;
	oy = (h - s) * 0.5;
	cairo_translate(cr, ox, oy);
	cairo_scale(cr, s / 16.0, s / 16.0);
	cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, fg.alpha * 0.85);
	cairo_set_line_width(cr, 1.35);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	/* three-bar "tools" menu */
	cairo_move_to(cr, 3.2, 5.0);
	cairo_line_to(cr, 12.8, 5.0);
	cairo_move_to(cr, 3.2, 8.0);
	cairo_line_to(cr, 12.8, 8.0);
	cairo_move_to(cr, 3.2, 11.0);
	cairo_line_to(cr, 12.8, 11.0);
	cairo_stroke(cr);
	return FALSE;
}

static void
on_vtools_clicked(GtkButton *button, gpointer data)
{
	const char *key = g_object_get_data(G_OBJECT(button), "verse-key");
	gchar *url;

	(void)data;
	if (!key || !*key)
		return;
	url = g_strdup_printf("passagestudy.jsp?action=verseTools&value=%s", key);
	main_url_handler(url, TRUE);
	g_free(url);
}

static void
insert_verse_tools_button(ParseCtx *ctx, const char *key)
{
	GtkTextIter iter;
	GtkTextChildAnchor *anchor;
	GtkWidget *btn, *icon;

	if (!ctx->html || !ctx->html->priv->view)
		return;
	if (settings.reading_mode)
		return;
	gtk_text_buffer_get_end_iter(ctx->buf, &iter);
	anchor = gtk_text_buffer_create_child_anchor(ctx->buf, &iter);

	btn = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus(btn, FALSE);
	gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
	gtk_widget_set_tooltip_text(btn, _("Herramientas de este versículo"));
	gtk_style_context_add_class(gtk_widget_get_style_context(btn), "elim-vtools");
	if (key && *key)
		g_object_set_data_full(G_OBJECT(btn), "verse-key",
				       g_strdup(key), g_free);
	g_signal_connect(btn, "clicked", G_CALLBACK(on_vtools_clicked), NULL);

	icon = gtk_drawing_area_new();
	gtk_widget_set_size_request(icon, 14, 12);
	gtk_widget_set_hexpand(icon, FALSE);
	gtk_widget_set_vexpand(icon, FALSE);
	g_signal_connect(icon, "draw", G_CALLBACK(on_vtools_icon_draw), NULL);
	gtk_container_add(GTK_CONTAINER(btn), icon);

	gtk_text_view_add_child_at_anchor(ctx->html->priv->view, btn, anchor);
	gtk_widget_show_all(btn);
	ctx->at_line_start = FALSE;
	insert_text(ctx, " ");
}

static void
il_table_fit(GtkWidget *view, GdkRectangle *alloc, GtkWidget *box)
{
	gint w, cur = -1;

	(void)view;
	w = alloc->width - 52;
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
		gtk_widget_set_size_request(box, alloc.width - 52, -1);
	gtk_widget_show_all(box);
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
		insert_break(ctx, TRUE);
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
		insert_verse_tools_button(ctx, key);
		g_free(key);
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

static void
clear_anchors(WkHtmlPrivate *priv)
{
	guint i;
	if (!priv->anchor_list)
		return;
	for (i = 0; i < priv->anchor_list->len; i++) {
		Anchor *a = g_ptr_array_index(priv->anchor_list, i);
		if (a->mark && !gtk_text_mark_get_deleted(a->mark))
			gtk_text_buffer_delete_mark(priv->buffer, a->mark);
		g_free(a->name);
		g_free(a);
	}
	g_ptr_array_set_size(priv->anchor_list, 0);
	g_hash_table_remove_all(priv->anchor_ht);
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

static const gchar *
tag_href(GtkTextTag *tag)
{
	return tag ? (const gchar *)g_object_get_data(G_OBJECT(tag), "href") : NULL;
}

static gchar *
iter_href(const GtkTextIter *iter)
{
	GSList *tags, *l;
	gchar *href = NULL;
	tags = gtk_text_iter_get_tags(iter);
	for (l = tags; l; l = l->next) {
		const gchar *h = tag_href(l->data);
		if (h) {
			href = g_strdup(h);
			break;
		}
	}
	g_slist_free(tags);
	return href;
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
	href = iter_href(&iter);
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
	href = iter_href(&iter);
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
			if (a->name && strcmp(a->name, "0") && strcmp(a->name, "TOP"))
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
				return TRUE;
			}
		}
		gtk_text_buffer_get_end_iter(priv->buffer, end);
		trim_trailing_heading(priv->buffer, start, end);
		trim_ilblock(priv->buffer, start, end);
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
	clear_anchors(priv);
	if (priv->anchor_list)
		g_ptr_array_free(priv->anchor_list, TRUE);
	if (priv->anchor_ht)
		g_hash_table_destroy(priv->anchor_ht);
	if (priv->link_tags)
		g_hash_table_destroy(priv->link_tags);
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
	priv->link_tags = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

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
		gtk_text_view_scroll_to_mark(priv->view, a->mark, 0.0, TRUE, 0.0, 0.22);
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
	g_free(html->priv->anchor);
	html->priv->anchor = g_strdup(anchor);
	if (html->priv->timeout) {
		g_source_remove(html->priv->timeout);
		html->priv->timeout = 0;
	}
	html->priv->jump_tries = 0;
	if (!anchor || !*anchor)
		return;
	a = g_hash_table_lookup(html->priv->anchor_ht, anchor);
	if (a && a->mark && !gtk_text_mark_get_deleted(a->mark))
		/* Scroll only if the verse sits in the top/bottom margin
		 * or off-screen — never yank it to a fixed slot. */
		gtk_text_view_scroll_to_mark(html->priv->view, a->mark,
					     0.16, FALSE, 0.0, 0.0);
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
